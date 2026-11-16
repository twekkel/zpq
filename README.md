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

## Benchmark

~1.4 GB log (10M lines from [supercomputer log](https://www.usenix.org/cfdr-data#hpc4)), single thread, x86-64 Linux build. Other tools shown for comparison
zpq rows at the bottom. Numbers are machine-dependent.

| Configuration                                 | c/d |  Wall(s) |  User(s) |   Sys(s) |   Memory(MB) |  Size(bytes) |   Ratio(%) |
|-----------------------------------------------|-----|----------|----------|----------|--------------|--------------|------------|
| lz4 -1                                        | c   |     2.29 |     2.43 |     0.52 |         31.9 |     74130923 |       5.23 |
| lz4 -1                                        | d   |     0.89 |     0.88 |     0.04 |         32.0 |            - |          - |
| flzp                                          | c   |     1.94 |     1.73 |     0.29 |          9.9 |     47047264 |       3.32 |
| flzp                                          | d   |     1.13 |     1.13 |     0.01 |          9.5 |            - |          - |
| zstd -3                                       | c   |     0.77 |     0.83 |     0.35 |         39.0 |     45472775 |       3.21 |
| zstd -3                                       | d   |     0.32 |     0.32 |     0.01 |          7.5 |            - |          - |
| gzip -6                                       | c   |     3.76 |     3.67 |     0.33 |          7.4 |     40599414 |       2.87 |
| gzip -6                                       | d   |     0.43 |     0.42 |     0.01 |          7.5 |            - |          - |
| zstd -11                                      | c   |     6.34 |     6.41 |     0.40 |         90.9 |     30219009 |       2.13 |
| zstd -11                                      | d   |     0.22 |     0.24 |     0.00 |          8.4 |            - |          - |
| flzp + gzip                                   | c   |     2.33 |     3.45 |     0.29 |          9.8 |     16002356 |       1.13 |
| flzp + gzip                                   | d   |     1.18 |     1.32 |     0.00 |          9.6 |            - |          - |
| kanzi -c -b64M -j 1 -t lzpx -e tpaq           | c   |    12.30 |    11.23 |     1.19 |        109.7 |      9376559 |       0.66 |
| kanzi -c -b64M -j 1 -t lzpx -e tpaq           | d   |    11.93 |    11.17 |     0.74 |        111.5 |            - |          - |
| kanzi -c -b64M -j 1 -t lzpx -e tpaqx          | c   |    18.52 |    14.90 |     3.72 |        359.4 |      9203874 |       0.65 |
| kanzi -c -b64M -j 1 -t lzpx -e tpaqx          | d   |    17.75 |    14.49 |     3.21 |        360.9 |            - |          - |
| flzp + zpq                                    | c   |    24.43 |    24.18 |     0.32 |        361.0 |      8474774 |       0.60 |
| flzp + zpq                                    | d   |    23.85 |    25.54 |     0.04 |        314.5 |            - |          - |
| flzp -B28 + zpq -z511                         | c   |    56.80 |    56.47 |     0.37 |        814.6 |      7897042 |       0.56 |
| flzp -B28 + zpq -z511                         | d   |    56.30 |    58.03 |     0.11 |        768.6 |            - |          - |
| flzp + ppmd                                   | c   |     3.06 |     4.38 |     0.33 |         17.8 |     10909404 |       0.77 |
| flzp + ppmd                                   | d   |     2.90 |     3.95 |     0.01 |         17.7 |            - |          - |
| flzp -B28 + ppmd -o8                          | c   |     3.42 |     4.82 |     0.37 |        262.0 |     10819851 |       0.76 |
| flzp -B28 + ppmd -o8                          | d   |     3.27 |     4.38 |     0.01 |        261.8 |            - |          - |
| flzp + bzip3                                  | c   |     4.91 |     4.68 |     0.30 |        100.3 |     10105572 |       0.71 |
| flzp + bzip3                                  | d   |     4.67 |     4.66 |     0.01 |        100.9 |            - |          - |
| flzp + xz                                     | c   |     8.67 |    18.16 |     0.39 |        256.2 |     12784860 |       0.90 |
| flzp + xz                                     | d   |     1.19 |     1.52 |     0.01 |         75.5 |            - |          - |
| flzp -B28 + xz -9e                            | c   |    16.56 |    18.29 |     0.34 |        532.2 |     12595712 |       0.89 |
| flzp -B28 + xz -9e                            | d   |     1.24 |     1.54 |     0.03 |        261.8 |            - |          - |

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
