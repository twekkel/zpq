#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <unistd.h>
#include <sys/stat.h>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include "libzpaq.h"
#pragma GCC diagnostic pop

#define ZPQ_LICENSE      "This is free and unencumbered software released into the public domain."
#define ZPQ_VERSION      "0.9.1"
#define ZPQ_CREDITS \
  "The libzpaq code (lib/libzpaq.*) is by Matt Mahoney, released into\n" \
  "the public domain (WBPE part: GPL-3, (C) 2011 Dell Inc.). The suffix\n" \
  "array construction is libsais (lib/libsais.*), Apache-2.0 with LLVM\n" \
  "exceptions, (C) 2021-2026 Ilya Grebnov. liblzma (lib/liblzma/) is public\n" \
  "domain, (C) Igor Pavlov (7-Zip).\n"

/* set while writing to a file, so error() can remove partial output */
static const char *partial_output = NULL;

/* libzpaq calls this on internal errors; zpaq.cpp defines its own */
namespace libzpaq {
void error(const char* msg) {
  fprintf(stderr, "zpq: libzpaq: %s\n", msg);
  if (partial_output) unlink(partial_output);
  exit(1);
}
}

#define SUFFIX ".zpq"
#define DEF_METHOD "4"

/* A bare level digit gets the zpaqf default block size appended:
 * 6 for levels >= 2, else 4 (see zpaq.cpp). libzpaq expands the digit
 * method per block; a full "x..." config is passed through unchanged. */
static const char *norm_method(const char *m) {
  static char buf[3];
  if (m[0] >= '0' && m[0] <= '5' && m[1] == 0) {
    buf[0] = m[0];
    buf[1] = m[0] >= '2' ? '6' : '4';
    buf[2] = 0;
    return buf;
  }
  return m;
}

static const char *progname = "zpq";

static struct {
  bool decompress, tostdout, keep, force, list, test, noname, quiet, verbose;
} opt;

static void warnmsg(const char *fmt, ...) {
  if (opt.quiet) return;
  va_list ap;
  va_start(ap, fmt);
  fprintf(stderr, "%s: ", progname);
  vfprintf(stderr, fmt, ap);
  fputc('\n', stderr);
  va_end(ap);
}

class FileReader: public libzpaq::Reader {
  FILE *f;
public:
  FileReader(FILE *f_): f(f_) {}
  int get() { return fgetc(f); }
  int read(char *buf, int n) { return (int)fread(buf, 1, (size_t)n, f); }
};

class FileWriter: public libzpaq::Writer {
  FILE *f;
public:
  unsigned long long count;
  FileWriter(FILE *f_): f(f_), count(0) {}
  void put(int c) { fputc(c, f); ++count; }
  void write(const char *buf, int n) { fwrite(buf, 1, (size_t)n, f); count += (unsigned)n; }
};

/* discards output but counts it, for -l */
class CountWriter: public libzpaq::Writer {
public:
  unsigned long long n;
  CountWriter(): n(0) {}
  void put(int) { ++n; }
  void write(const char*, int len) { n += (unsigned)len; }
};

/* captures a segment string (file name or comment) */
class NameWriter: public libzpaq::Writer {
  char buf[4096];
public:
  int len;
  NameWriter(): len(0) {}
  void reset() { len = 0; }
  void put(int c) { if (len < (int)sizeof buf - 1) buf[len++] = (char)c; }
  void write(const char *s, int n) { for (int i = 0; i < n; ++i) put(s[i]); }
  const char *name() { buf[len] = 0; return buf; }
};

static void usage(FILE *f) {
  fprintf(f,
    "%s %s (libzpaq) compressor\n"
    "\n"
    "Usage: %s [-cdfhkLlNnqVv] [-m METHOD] [file ...]\n"
    "  -c, --stdout       write to stdout, keep input files\n"
    "  -d, --decompress   decompress\n"
    "  -f, --force        overwrite output files, allow terminal I/O\n"
    "  -h, --help         show this help\n"
    "  -k, --keep         keep input files\n"
    "  -l, --list         list archive contents\n"
    "  -L, --license      display software license\n"
    "  -n, --no-name      do not store the original file name in the archive\n"
    "  -N, --name         store the original file name in the archive (default)\n"
    "  -q, --quiet        suppress warnings\n"
    "  -t, --test         test archive integrity (SHA-1)\n"
    "  -v, --verbose      verbose mode\n"
    "  -V, --version      display version number\n"
    "  -m, --zpaq METHOD  zpaq method: level 0..5 or libzpaq \"x...\" string\n"
    "                     (default -m4)\n"
    "\n"
    "With no FILE, or when FILE is -, read standard input.\n",
    progname, ZPQ_VERSION, progname);
}

static unsigned long long list_total_c, list_total_u;
static int list_count;

static void list_row(const char *name, const char *stored,
                     long long csize, unsigned long long usize) {
  if (list_count++ == 0)
    printf("%19s %19s %6s %s\n", "compressed", "uncompressed", "ratio", "uncompressed_name");

  const char *shown = stored && *stored ? stored : (name ? name : "-");
  int len = (int)strlen(shown);
  if (shown == name && len > (int)strlen(SUFFIX) && !strcmp(shown + len - strlen(SUFFIX), SUFFIX))
    len -= (int)strlen(SUFFIX);

  char c[24] = "-", ratio[16] = "-";
  if (csize >= 0) {
    snprintf(c, sizeof c, "%lld", csize);
    snprintf(ratio, sizeof ratio, "%.1f%%",
             usize ? 100.0 * (double)((long long)usize - csize) / (double)usize : 0.0);
    list_total_c += (unsigned long long)csize;
  }
  list_total_u += usize;
  printf("%19s %19llu %6s %.*s\n", c, usize, ratio, len, shown);
}

static void list_totals(void) {
  if (list_count > 1)
    printf("%19llu %19llu %5.1f%% (totals)\n", list_total_c, list_total_u,
           list_total_u ? 100.0 * ((double)list_total_u - (double)list_total_c) / (double)list_total_u : 0.0);
}

/* List one archive: decompress to a counting writer, capture the name. */
static int list_archive(const char *name) {
  FILE *fi = stdin;
  if (name) {
    fi = fopen(name, "rb");
    if (!fi) { perror(name); return 1; }
  }
  setvbuf(fi, NULL, _IOFBF, 1 << 16);

  FileReader r(fi);
  libzpaq::Decompresser d;
  d.setInput(&r);
  CountWriter cw;
  d.setOutput(&cw);
  NameWriter nw;
  char last[4096] = "";
  while (d.findBlock()) {
    for (;;) {
      nw.reset();
      if (!d.findFilename(&nw)) break;
      d.readComment();
      d.decompress();
      d.readSegmentEnd();
      snprintf(last, sizeof last, "%s", nw.name());
    }
  }

  long long csize = -1;
  struct stat st;
  if (fstat(fileno(fi), &st) == 0 && S_ISREG(st.st_mode)) csize = (long long)st.st_size;
  if (fi != stdin) fclose(fi);
  list_row(name, last, csize, cw.n);
  return 0;
}

/* Test one archive: decompress to a counting writer, verify the SHA-1. */
static int test_archive(const char *name) {
  FILE *fi = stdin;
  if (name) {
    fi = fopen(name, "rb");
    if (!fi) { perror(name); return 1; }
  }
  setvbuf(fi, NULL, _IOFBF, 1 << 16);

  FileReader r(fi);
  libzpaq::Decompresser d;
  d.setInput(&r);
  CountWriter cw;
  d.setOutput(&cw);
  libzpaq::SHA1 sha1;
  d.setSHA1(&sha1);
  char sha1result[21];
  int rc = 0;
  while (d.findBlock()) {
    while (d.findFilename()) {
      d.readComment();
      d.decompress();
      d.readSegmentEnd(sha1result);
      if (!sha1result[0]) {
        fprintf(stderr, "%s: %s: no SHA-1 checksum\n", progname, name ? name : "stdin");
        rc = 1;
      } else if (memcmp(sha1result + 1, sha1.result(), 20)) {
        fprintf(stderr, "%s: %s: SHA-1 mismatch\n", progname, name ? name : "stdin");
        rc = 1;
      }
    }
  }
  if (fi != stdin) fclose(fi);
  if (rc == 0 && opt.verbose)
    fprintf(stderr, "%s:\tOK\n", name ? name : "stdin");
  return rc;
}

/* Run one job. Returns 0 ok, 1 failed, 2 output not created. */
static int process(const char *in, const char *out, const char *method) {
  FILE *fi = stdin, *fo = stdout;
  if (in) {
    fi = fopen(in, "rb");
    if (!fi) { perror(in); return 1; }
  }
  if (out) {
    /* "wbx" fails if the file exists, so there is no access()/fopen race */
    fo = fopen(out, opt.force ? "wb" : "wbx");
    if (!fo) {
      if (!opt.force && errno == EEXIST)
        warnmsg("%s already exists; not overwritten (use -f)", out);
      else
        perror(out);
      if (fi != stdin) fclose(fi);
      return 2;  /* output not created; caller must not unlink it */
    }
    partial_output = out;
  }
  setvbuf(fi, NULL, _IOFBF, 1 << 16);
  setvbuf(fo, NULL, _IOFBF, 1 << 16);

  FileReader r(fi);
  FileWriter w(fo);
  struct stat st;
  bool have_st = in && fstat(fileno(fi), &st) == 0 && S_ISREG(st.st_mode);
  /* metadata goes in the segment comment: "mode atime mtime" */
  unsigned out_mode = 0;
  struct timespec out_times[2];
  bool have_meta = false;
  if (opt.decompress) {
    libzpaq::Decompresser d;
    d.setInput(&r);
    d.setOutput(&w);
    NameWriter cw;
    while (d.findBlock()) {
      while (d.findFilename()) {
        cw.reset();
        d.readComment(&cw);
        d.decompress();
        d.readSegmentEnd();
        unsigned m;
        long long as, ms;
        long an, mn;
        /* libzpaq prefixes the comment with the uncompressed size */
        if (sscanf(cw.name(), "%*d %o %lld %ld %lld %ld",
                   &m, &as, &an, &ms, &mn) == 5) {
          out_mode = m;
          out_times[0].tv_sec = (time_t)as;
          out_times[0].tv_nsec = an;
          out_times[1].tv_sec = (time_t)ms;
          out_times[1].tv_nsec = mn;
          have_meta = true;
        }
      }
    }
  } else {
    const char *base = NULL;
    if (in && !opt.noname) {  /* -n: store no name in the archive */
      const char *slash = strrchr(in, '/');
      base = slash ? slash + 1 : in;
    }
    char comment[64] = "";
    if (have_st)
      snprintf(comment, sizeof comment, "%o %lld %ld %lld %ld",
               st.st_mode & 07777,
               (long long)st.st_atim.tv_sec, (long)st.st_atim.tv_nsec,
               (long long)st.st_mtim.tv_sec, (long)st.st_mtim.tv_nsec);
    libzpaq::compress(&r, &w, method, base, comment);
  }

  long long isize = -1;
  if (have_st) isize = (long long)st.st_size;

  int rc = 0;
  /* fgetc/fread return EOF on error too, so libzpaq sees a read error as
   * end of file; without this check the caller would unlink the original */
  if (ferror(fi)) {
    fprintf(stderr, "%s: %s: read error\n", progname, in ? in : "stdin");
    rc = 1;
  }
  int werr = ferror(fo);  /* check before fclose invalidates the stream */
  int wrc = fo == stdout ? fflush(stdout) : fclose(fo);
  if (werr || wrc != 0) {
    fprintf(stderr, "%s: %s: write error\n", progname, out ? out : "stdout");
    rc = 1;
  }
  if (fi != stdin && fclose(fi) != 0) {
    if (!rc) fprintf(stderr, "%s: %s: read error\n", progname, in);
    rc = 1;
  }
  /* after fclose(fo) so the writes cannot clobber the restored times */
  if (rc == 0 && out && have_meta) {
    chmod(out, out_mode & 07777);
    utimensat(AT_FDCWD, out, out_times, 0);
  }
  partial_output = NULL;
  if (rc == 0 && opt.verbose)
    fprintf(stderr, "%s:\t%lld -> %llu bytes\n", in ? in : "stdin", isize, w.count);
  return rc;
}

int main(int argc, char **argv) {
  const char *method = norm_method(DEF_METHOD);
  static const struct option lopts[] = {
    {"decompress", no_argument,       0, 'd'},
    {"uncompress", no_argument,       0, 'd'},
    {"stdout",     no_argument,       0, 'c'},
    {"to-stdout",  no_argument,       0, 'c'},
    {"keep",       no_argument,       0, 'k'},
    {"force",      no_argument,       0, 'f'},
    {"list",       no_argument,       0, 'l'},
    {"test",       no_argument,       0, 't'},
    {"license",    no_argument,       0, 'L'},
    {"no-name",    no_argument,       0, 'n'},
    {"name",       no_argument,       0, 'N'},
    {"quiet",      no_argument,       0, 'q'},
    {"verbose",    no_argument,       0, 'v'},
    {"zpaq",       required_argument, 0, 'm'},
    {"help",       no_argument,       0, 'h'},
    {"version",    no_argument,       0, 'V'},
    {0, 0, 0, 0}
  };

  int o;
  while ((o = getopt_long(argc, argv, "cdfhkLlNnqtVvm:", lopts, NULL)) != -1) {
    switch (o) {
      case 'c': opt.tostdout = true;  break;
      case 'd': opt.decompress = true; break;
      case 'f': opt.force = true;     break;
      case 'h': usage(stdout); return 0;
      case 'k': opt.keep = true;      break;
      case 'l': opt.list = true;      break;
      case 't': opt.test = true;      break;
      case 'L': printf("%s %s\n%s\n\n%s", progname, ZPQ_VERSION, ZPQ_LICENSE, ZPQ_CREDITS);
                return 0;
      case 'n': opt.noname = true;    break;
      case 'N': opt.noname = false;   break;
      case 'q': opt.quiet = true;     break;
      case 'v': opt.verbose = true;   break;
      case 'V': printf("%s %s\n", progname, ZPQ_VERSION); return 0;
      case 'm': method = norm_method(optarg); break;
      default:  usage(stderr); return 1;
    }
  }

  if (!opt.force) {
    bool reading = opt.decompress || opt.list || opt.test;
    if (optind == argc && (reading ? isatty(0) : isatty(1))) {
      fprintf(stderr, "%s: compressed data not %s a terminal (use -f)\n",
              progname, reading ? "read from" : "written to");
      return 1;
    }
    if (optind < argc && opt.tostdout && !reading && isatty(1)) {
      fprintf(stderr, "%s: compressed data not written to a terminal "
                      "(use -f)\n", progname);
      return 1;
    }
  }

  if (optind == argc) {  /* no files: stdin -> stdout */
    if (opt.list) return list_archive(NULL);
    if (opt.test) return test_archive(NULL);
    return process(NULL, NULL, method);
  }

  const size_t sl = strlen(SUFFIX);
  int status = 0;
  for (int i = optind; i < argc; ++i) {
    const char *in = argv[i];
    size_t n = strlen(in);

    if (!strcmp(in, "-")) {
      if (!opt.force) {
        bool reading = opt.decompress || opt.list || opt.test;
        if (reading && isatty(0)) {
          fprintf(stderr, "%s: compressed data not read from a terminal "
                  "(use -f)\n", progname);
          status = 1;
          continue;
        }
        if (!reading && isatty(1)) {
          fprintf(stderr, "%s: compressed data not written to a terminal "
                  "(use -f)\n", progname);
          status = 1;
          continue;
        }
      }
      int rc = opt.list ? list_archive(NULL)
               : opt.test ? test_archive(NULL)
               : process(NULL, NULL, method);
      if (rc) status = 1;
      continue;
    }

    if (opt.list) {
      if (list_archive(in)) status = 1;
      continue;
    }
    if (opt.test) {
      if (test_archive(in)) status = 1;
      continue;
    }

    char *out = NULL;
    int has_suffix = n > sl && !strcmp(in + n - sl, SUFFIX);

    if (!opt.tostdout) {
      if (opt.decompress) {
        if (!has_suffix) {
          warnmsg("%s: unknown suffix -- ignored", in);
          status = 1;
          continue;
        }
        out = (char *)malloc(n - sl + 1);
        if (!out) { fprintf(stderr, "%s: out of memory\n", progname); return 1; }
        memcpy(out, in, n - sl);
        out[n - sl] = 0;
      } else {
        if (has_suffix) {
          warnmsg("%s already has %s suffix -- unchanged", in, SUFFIX);
          status = 1;
          continue;
        }
        out = (char *)malloc(n + sl + 1);
        if (!out) { fprintf(stderr, "%s: out of memory\n", progname); return 1; }
        memcpy(out, in, n);
        memcpy(out + n, SUFFIX, sl + 1);
      }
    }

    int rc = process(in, out, method);
    if (out) {
      if (rc == 1)                     unlink(out);  /* no partial output */
      else if (rc == 0 && !opt.keep)   unlink(in);   /* success: drop the input */
      free(out);
    }
    if (rc) status = 1;
  }
  if (opt.list) list_totals();
  return status;
}
