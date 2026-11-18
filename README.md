# zpq - a libzpaq based compressor (no archiving)

```
zpq 0.9.1 (libzpaq) compressor

Usage: zpq [-cdfhkLlNnqVv] [-m METHOD] [file ...]
  -c, --stdout       write to stdout, keep input files
  -d, --decompress   decompress
  -f, --force        overwrite output files, allow terminal I/O
  -h, --help         show this help
  -k, --keep         keep input files
  -l, --list         list archive contents
  -L, --license      display software license
  -n, --no-name      do not store the original file name in the archive
  -N, --name         store the original file name in the archive (default)
  -m, --zpaq METHOD  zpaq method: level 0..5 or libzpaq "x..." string
                     (default -m4)
  -q, --quiet        suppress warnings
  -t, --test         test archive integrity (SHA-1)
  -v, --verbose      verbose mode
  -V, --version      display version number
```

`zpq` is a file compression tool built around the **zpaq** architecture, featuring a traditional gzip/xz/zstd-style command-line interface.

It prioritizes maximum compression density over speed.

Additional features:
 * libsais replaces libdivsufsort
 * liblzma is included from zpaqf

```
zpq -m 0 big.bin                           # store only
zpq -m 14,128,0 big.log                    # libzpaq default: LZ77, 16 MiB blocks
zpq -m "x6.3ci1m" book.txt                 # BWT + order-0 CM + order-1 ISSE + MIX
zpq -m "x4.3ci1" small.txt                 # BWT + CM + ISSE, 16 MiB blocks
zpq -m "x6,14,9,3,0,2,273,2,1" access.log  # LZMA-9 + WBPE + capital, 64 MiB blocks
zpq -m "x6,14,9,3,0,2,273,1,0" app.bin     # LZMA-9 + E8E9 (x86 code)
zpq -m "x6,14,5,3,0,2,32,0,0" big.log      # LZMA-5, no filter
```

`xN,14,L,lc,lp,pb,fb,F,G`: N = log2 block MiB (also LZMA dict size),
L = LZMA level 0..9, lc/lp/pb = 0..8/0..4/0..4, fb = 5..273,
F = 0 none / 1 E8E9 / 2 WBPE, G = WBPE capital transform 0/1.

## Benchmark

~14 MB log (100k lines from [supercomputer log](https://www.usenix.org/cfdr-data#hpc4)), single thread, x86-64 Linux build. Other tools shown for comparison
zpq rows at the bottom. Numbers are machine-dependent.

| Configuration                                 | c/d |  Wall(s) |  User(s) |   Sys(s) |   Memory(MB) |  Size(bytes) |   Ratio(%) |
|-----------------------------------------------|-----|----------|----------|----------|--------------|--------------|------------|
| lz4                                           | c   |     0.03 |     0.02 |     0.02 |         18.2 |      1394671 |       9.72 |
| lz4                                           | d   |     0.02 |     0.02 |     0.00 |         18.1 |            - |          - |
| gzip                                          | c   |     0.11 |     0.11 |     0.00 |          7.4 |       836843 |       5.83 |
| gzip                                          | d   |     0.01 |     0.01 |     0.00 |          7.4 |            - |          - |
| zstd                                          | c   |     0.03 |     0.03 |     0.01 |         21.0 |       816004 |       5.69 |
| zstd                                          | d   |     0.01 |     0.01 |     0.00 |          7.4 |            - |          - |
| zstd -11                                      | c   |     0.29 |     0.28 |     0.02 |         40.7 |       668096 |       4.66 |
| zstd -11                                      | d   |     0.01 |     0.01 |     0.00 |          7.8 |            - |          - |
| xz                                            | c   |     2.97 |     2.95 |     0.03 |        114.8 |       533844 |       3.72 |
| xz                                            | d   |     0.04 |     0.04 |     0.00 |         28.3 |            - |          - |
| zpq -m"x6,14,9,3,0,2,273,2,1"                 | c   |     5.17 |     5.13 |     0.03 |         85.7 |       408964 |       2.85 |
| zpq -m"x6,14,9,3,0,2,273,2,1"                 | d   |     0.14 |     0.14 |     0.00 |         16.3 |            - |          - |
| kanzi -c -l 9                                 | c   |     3.58 |     3.49 |     0.05 |        857.6 |       253513 |       1.77 |
| kanzi -c -l 9                                 | d   |     3.40 |     3.36 |     0.02 |        844.3 |            - |          - |
| kanzi -c -t lzpx -e tpaqx                     | c   |     0.37 |     0.73 |     0.11 |       1046.4 |       289546 |       2.02 |
| kanzi -c -t lzpx -e tpaqx                     | d   |     0.38 |     0.75 |     0.12 |       1041.4 |            - |          - |
| zpq                                           | c   |    10.56 |    10.52 |     0.03 |        104.8 |       275553 |       1.92 |
| zpq                                           | d   |    10.58 |    10.56 |     0.01 |         90.5 |            - |          - |
| zpq -m511                                     | c   |    31.39 |    31.30 |     0.03 |        288.5 |       237553 |       1.66 |
| zpq -m511                                     | d   |    31.27 |    31.20 |     0.02 |        274.8 |            - |          - |

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
