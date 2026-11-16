# zpq - simple zpaq (libzpaq) compressor

```
zpq 0.9.1 (libzpaq) compressor

Usage: zpq [-cdfhkLlNnqVv] [-z METHOD] [file ...]
  -c, --stdout       write to stdout, keep input files
  -d, --decompress   decompress
  -f, --force        overwrite output files, allow terminal I/O
  -h, --help         show this help
  -k, --keep         keep input files
  -l, --list         list archive contents
  -L, --license      display software license
  -n, --no-name      do not store the original file name in the archive
  -N, --name         store the original file name in the archive (default)
  -q, --quiet        suppress warnings
  -t, --test         test archive integrity (SHA-1)
  -v, --verbose      verbose mode
  -V, --version      display version number
  -z, --zpaq METHOD  zpaq method: level 0..5 or libzpaq "x..." string
                     (default -z4)
```

Thin wrapper around libzpaq; libzpaq (from ../zpaqf, with its bundled
libdivsufsort replaced by libsais from ../libsais), liblzma
(byte-identical to ../7zip/C) and libsais are vendored in `lib/`, so
this directory builds standalone.
I/O streams through libzpaq's Reader/Writer, so memory is bounded by the
zpaq block size (2^N MiB), not by the input size.

The default is `-z4` (zpaqf level 4, 2^6 MiB blocks), which libzpaq
expands per block; measured on liberty2.log, level 4 gives 0.0201 at
1.3 MB/s while level 5 (`-z5`) reaches 0.0178 at only 0.6 MB/s. A full
libzpaq `"x..."` method string works too. Method strings are documented
in `lib/libzpaq.h`; a few examples:

```
zpq -z 0 big.bin                           # store only
zpq -z 14,128,0 big.log                    # libzpaq default: LZ77, 16 MiB blocks
zpq -z "x6.3ci1m" book.txt                 # BWT + order-0 CM + order-1 ISSE + MIX
zpq -z "x4.3ci1" small.txt                 # BWT + CM + ISSE, 16 MiB blocks
zpq -z "x6,14,9,3,0,2,273,2,1" access.log  # LZMA-9 + WBPE + capital, 64 MiB blocks
zpq -z "x6,14,9,3,0,2,273,1,0" app.bin     # LZMA-9 + E8E9 (x86 code)
zpq -z "x6,14,5,3,0,2,32,0,0" big.log      # LZMA-5, no filter
```

`xN,14,L,lc,lp,pb,fb,F,G`: N = log2 block MiB (also LZMA dict size),
L = LZMA level 0..9, lc/lp/pb = 0..8/0..4/0..4, fb = 5..273,
F = 0 none / 1 E8E9 / 2 WBPE, G = WBPE capital transform 0/1.

Build and test:

```
make
make test
```

## Credits & Acknowledgments
* **zpaq / libzpaq:** (C) Matt Mahoney, public domain - https://www.mattmahoney.net/dc/zpaq.html
* **zpaqf:** fork of zpaq v7.15 by Kaido Orav - https://github.com/kaitz/zpaqf (source of `lib/libzpaq.*`)
* **LZMA SDK (`lib/liblzma/`):** public domain, (C) 2026 Igor Pavlov (7-Zip project)
* **WBPE** (in libzpaq): (C) 2011 Dell Inc., written by Matt Mahoney, GPL v3
* **libsais** (`lib/libsais.*`, replaces libdivsufsort in libzpaq): Apache-2.0 with LLVM exceptions, (C) 2021-2026 Ilya Grebnov - https://github.com/IlyaGrebnov/libsais

## License

This project is open source and available under the [The Unlicense](LICENSE) (Public Domain).
