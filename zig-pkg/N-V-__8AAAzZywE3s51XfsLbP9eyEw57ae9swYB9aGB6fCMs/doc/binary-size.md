# Binary Size

Binary size measurements of `wuffs genlib` libraries on x86\_64 are below.
Lower is better.

    clang11-dynamic:
    -rwxr-xr-x 1 tao tao 322264 Jan 26 15:33 libwuffs.so
    -rw-r--r-- 1 tao tao   5232 Jan 26 15:32 wuffs-base-core.lo
    -rw-r--r-- 1 tao tao  26976 Jan 26 15:32 wuffs-base-floatconv.lo
    -rw-r--r-- 1 tao tao   9312 Jan 26 15:32 wuffs-base-intconv.lo
    -rw-r--r-- 1 tao tao   7920 Jan 26 15:33 wuffs-base-interfaces.lo
    -rw-r--r-- 1 tao tao   3216 Jan 26 15:33 wuffs-base-magic.lo
    -rw-r--r-- 1 tao tao  56192 Jan 26 15:33 wuffs-base-pixconv.lo
    -rw-r--r-- 1 tao tao   3080 Jan 26 15:33 wuffs-base-utf8.lo
    -rw-r--r-- 1 tao tao   4056 Jan 26 15:33 wuffs-std-adler32.lo
    -rw-r--r-- 1 tao tao  28600 Jan 26 15:33 wuffs-std-bmp.lo
    -rw-r--r-- 1 tao tao  17304 Jan 26 15:33 wuffs-std-bzip2.lo
    -rw-r--r-- 1 tao tao  10168 Jan 26 15:33 wuffs-std-cbor.lo
    -rw-r--r-- 1 tao tao  22336 Jan 26 15:33 wuffs-std-crc32.lo
    -rw-r--r-- 1 tao tao  24816 Jan 26 15:33 wuffs-std-deflate.lo
    -rw-r--r-- 1 tao tao  25088 Jan 26 15:33 wuffs-std-gif.lo
    -rw-r--r-- 1 tao tao   7408 Jan 26 15:33 wuffs-std-gzip.lo
    -rw-r--r-- 1 tao tao  20136 Jan 26 15:33 wuffs-std-json.lo
    -rw-r--r-- 1 tao tao   6344 Jan 26 15:33 wuffs-std-lzw.lo
    -rw-r--r-- 1 tao tao   9928 Jan 26 15:33 wuffs-std-nie.lo
    -rw-r--r-- 1 tao tao  54760 Jan 26 15:33 wuffs-std-png.lo
    -rw-r--r-- 1 tao tao  14552 Jan 26 15:33 wuffs-std-tga.lo
    -rw-r--r-- 1 tao tao   9080 Jan 26 15:33 wuffs-std-wbmp.lo
    -rw-r--r-- 1 tao tao   7224 Jan 26 15:33 wuffs-std-zlib.lo

    clang11-static:
    -rw-r--r-- 1 tao tao 398014 Jan 26 15:32 libwuffs.a
    -rw-r--r-- 1 tao tao   5232 Jan 26 15:32 wuffs-base-core.o
    -rw-r--r-- 1 tao tao  27136 Jan 26 15:32 wuffs-base-floatconv.o
    -rw-r--r-- 1 tao tao   9912 Jan 26 15:32 wuffs-base-intconv.o
    -rw-r--r-- 1 tao tao   7856 Jan 26 15:32 wuffs-base-interfaces.o
    -rw-r--r-- 1 tao tao   3112 Jan 26 15:32 wuffs-base-magic.o
    -rw-r--r-- 1 tao tao  59000 Jan 26 15:32 wuffs-base-pixconv.o
    -rw-r--r-- 1 tao tao   3080 Jan 26 15:32 wuffs-base-utf8.o
    -rw-r--r-- 1 tao tao   4088 Jan 26 15:32 wuffs-std-adler32.o
    -rw-r--r-- 1 tao tao  29384 Jan 26 15:32 wuffs-std-bmp.o
    -rw-r--r-- 1 tao tao  16784 Jan 26 15:32 wuffs-std-bzip2.o
    -rw-r--r-- 1 tao tao  10304 Jan 26 15:32 wuffs-std-cbor.o
    -rw-r--r-- 1 tao tao  23352 Jan 26 15:32 wuffs-std-crc32.o
    -rw-r--r-- 1 tao tao  25344 Jan 26 15:32 wuffs-std-deflate.o
    -rw-r--r-- 1 tao tao  25608 Jan 26 15:32 wuffs-std-gif.o
    -rw-r--r-- 1 tao tao   7000 Jan 26 15:32 wuffs-std-gzip.o
    -rw-r--r-- 1 tao tao  21000 Jan 26 15:32 wuffs-std-json.o
    -rw-r--r-- 1 tao tao   6272 Jan 26 15:32 wuffs-std-lzw.o
    -rw-r--r-- 1 tao tao   9840 Jan 26 15:32 wuffs-std-nie.o
    -rw-r--r-- 1 tao tao  56560 Jan 26 15:32 wuffs-std-png.o
    -rw-r--r-- 1 tao tao  14720 Jan 26 15:32 wuffs-std-tga.o
    -rw-r--r-- 1 tao tao   8984 Jan 26 15:32 wuffs-std-wbmp.o
    -rw-r--r-- 1 tao tao   7064 Jan 26 15:32 wuffs-std-zlib.o

    gcc10-dynamic:
    -rwxr-xr-x 1 tao tao 388712 Jan 26 15:33 libwuffs.so
    -rw-r--r-- 1 tao tao   5632 Jan 26 15:33 wuffs-base-core.lo
    -rw-r--r-- 1 tao tao  26904 Jan 26 15:33 wuffs-base-floatconv.lo
    -rw-r--r-- 1 tao tao   8584 Jan 26 15:33 wuffs-base-intconv.lo
    -rw-r--r-- 1 tao tao   7664 Jan 26 15:33 wuffs-base-interfaces.lo
    -rw-r--r-- 1 tao tao   4448 Jan 26 15:33 wuffs-base-magic.lo
    -rw-r--r-- 1 tao tao 109488 Jan 26 15:33 wuffs-base-pixconv.lo
    -rw-r--r-- 1 tao tao   3248 Jan 26 15:33 wuffs-base-utf8.lo
    -rw-r--r-- 1 tao tao   4624 Jan 26 15:33 wuffs-std-adler32.lo
    -rw-r--r-- 1 tao tao  28984 Jan 26 15:33 wuffs-std-bmp.lo
    -rw-r--r-- 1 tao tao  18920 Jan 26 15:33 wuffs-std-bzip2.lo
    -rw-r--r-- 1 tao tao  10040 Jan 26 15:33 wuffs-std-cbor.lo
    -rw-r--r-- 1 tao tao  23056 Jan 26 15:33 wuffs-std-crc32.lo
    -rw-r--r-- 1 tao tao  24264 Jan 26 15:33 wuffs-std-deflate.lo
    -rw-r--r-- 1 tao tao  29808 Jan 26 15:33 wuffs-std-gif.lo
    -rw-r--r-- 1 tao tao   8328 Jan 26 15:33 wuffs-std-gzip.lo
    -rw-r--r-- 1 tao tao  20128 Jan 26 15:33 wuffs-std-json.lo
    -rw-r--r-- 1 tao tao   7128 Jan 26 15:33 wuffs-std-lzw.lo
    -rw-r--r-- 1 tao tao  11960 Jan 26 15:33 wuffs-std-nie.lo
    -rw-r--r-- 1 tao tao  68440 Jan 26 15:33 wuffs-std-png.lo
    -rw-r--r-- 1 tao tao  15736 Jan 26 15:33 wuffs-std-tga.lo
    -rw-r--r-- 1 tao tao  10440 Jan 26 15:33 wuffs-std-wbmp.lo
    -rw-r--r-- 1 tao tao   7744 Jan 26 15:33 wuffs-std-zlib.lo

    gcc10-static:
    -rw-r--r-- 1 tao tao 480206 Jan 26 15:33 libwuffs.a
    -rw-r--r-- 1 tao tao   5632 Jan 26 15:33 wuffs-base-core.o
    -rw-r--r-- 1 tao tao  27008 Jan 26 15:33 wuffs-base-floatconv.o
    -rw-r--r-- 1 tao tao   8768 Jan 26 15:33 wuffs-base-intconv.o
    -rw-r--r-- 1 tao tao   7560 Jan 26 15:33 wuffs-base-interfaces.o
    -rw-r--r-- 1 tao tao   4448 Jan 26 15:33 wuffs-base-magic.o
    -rw-r--r-- 1 tao tao 109408 Jan 26 15:33 wuffs-base-pixconv.o
    -rw-r--r-- 1 tao tao   3824 Jan 26 15:33 wuffs-base-utf8.o
    -rw-r--r-- 1 tao tao   4680 Jan 26 15:33 wuffs-std-adler32.o
    -rw-r--r-- 1 tao tao  29512 Jan 26 15:33 wuffs-std-bmp.o
    -rw-r--r-- 1 tao tao  19000 Jan 26 15:33 wuffs-std-bzip2.o
    -rw-r--r-- 1 tao tao  10400 Jan 26 15:33 wuffs-std-cbor.o
    -rw-r--r-- 1 tao tao  23112 Jan 26 15:33 wuffs-std-crc32.o
    -rw-r--r-- 1 tao tao  24936 Jan 26 15:33 wuffs-std-deflate.o
    -rw-r--r-- 1 tao tao  30232 Jan 26 15:33 wuffs-std-gif.o
    -rw-r--r-- 1 tao tao   8424 Jan 26 15:33 wuffs-std-gzip.o
    -rw-r--r-- 1 tao tao  21456 Jan 26 15:33 wuffs-std-json.o
    -rw-r--r-- 1 tao tao   7144 Jan 26 15:33 wuffs-std-lzw.o
    -rw-r--r-- 1 tao tao  12168 Jan 26 15:33 wuffs-std-nie.o
    -rw-r--r-- 1 tao tao  71504 Jan 26 15:33 wuffs-std-png.o
    -rw-r--r-- 1 tao tao  16184 Jan 26 15:33 wuffs-std-tga.o
    -rw-r--r-- 1 tao tao  10520 Jan 26 15:33 wuffs-std-wbmp.o
    -rw-r--r-- 1 tao tao   7904 Jan 26 15:33 wuffs-std-zlib.o


## Comparison

Below are some standard C libraries shipped as part of Debian 11 Bullseye as of
January 2023. The numbers aren't directly comparable, as these libraries have a
richer API, especially in providing an encoder and not just a decoder. Still,
it is a reference point for e.g. Wuffs (adler32 + crc32 + deflate + gzip +
zlib) vs libz and Wuffs (gif + lzw) vs libgif.

    dynamic:
    -rw-r--r-- 1 root root  74688 Jul 20  2020 /lib/x86_64-linux-gnu/libbz2.so.1.0.4
    -rw-r--r-- 1 root root  38752 Dec 21  2020 /lib/x86_64-linux-gnu/libgif.so.7.1.0
    -rw-r--r-- 1 root root 231344 Sep 16  2020 /lib/x86_64-linux-gnu/libpng16.so.16.37.0
    -rw-r--r-- 1 root root 113088 Aug 24 04:54 /lib/x86_64-linux-gnu/libz.so.1.2.11

    static:
    -rw-r--r-- 1 root root  80690 Jul 20  2020 /lib/x86_64-linux-gnu/libbz2.a
    -rw-r--r-- 1 root root  51940 Dec 21  2020 /lib/x86_64-linux-gnu/libgif.a
    -rw-r--r-- 1 root root 374338 Sep 16  2020 /lib/x86_64-linux-gnu/libpng16.a
    -rw-r--r-- 1 root root 144866 Aug 24 04:54 /lib/x86_64-linux-gnu/libz.a


---

Updated on January 2023.
