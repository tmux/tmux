// Copyright 2023 The Wuffs Authors.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

// ----------------

/*
This test program is typically run indirectly, by the "wuffs test" or "wuffs
bench" commands. These commands take an optional "-mimic" flag to check that
Wuffs' output mimics (i.e. exactly matches) other libraries' output, such as
giflib for GIF, libpng for PNG, etc.

To manually run this test:

for CC in clang gcc; do
  $CC -std=c99 -Wall -Werror crc64.c && ./a.out
  rm -f a.out
done

Each edition should print "PASS", amongst other information, and exit(0).

Add the "wuffs mimic cflags" (everything after the colon below) to the C
compiler flags (after the .c file) to run the mimic tests.

To manually run the benchmarks, replace "-Wall -Werror" with "-O3" and replace
the first "./a.out" with "./a.out -bench". Combine these changes with the
"wuffs mimic cflags" to run the mimic benchmarks.
*/

// ¿ wuffs mimic cflags: -DWUFFS_MIMIC

// Wuffs ships as a "single file C library" or "header file library" as per
// https://github.com/nothings/stb/blob/master/docs/stb_howto.txt
//
// To use that single file as a "foo.c"-like implementation, instead of a
// "foo.h"-like header, #define WUFFS_IMPLEMENTATION before #include'ing or
// compiling it.
#define WUFFS_IMPLEMENTATION

// Defining the WUFFS_CONFIG__MODULE* macros are optional, but it lets users of
// release/c/etc.c choose which parts of Wuffs to build. That file contains the
// entire Wuffs standard library, implementing a variety of codecs and file
// formats. Without this macro definition, an optimizing compiler or linker may
// very well discard Wuffs code for unused codecs, but listing the Wuffs
// modules we use makes that process explicit. Preprocessing means that such
// code simply isn't compiled.
#define WUFFS_CONFIG__MODULES
#define WUFFS_CONFIG__MODULE__BASE
#define WUFFS_CONFIG__MODULE__CRC64

// If building this program in an environment that doesn't easily accommodate
// relative includes, you can use the script/inline-c-relative-includes.go
// program to generate a stand-alone C file.
#include "../../../release/c/wuffs-unsupported-snapshot.c"
#include "../testlib/testlib.c"
#ifdef WUFFS_MIMIC
// No mimic library.
#endif

// ---------------- Golden Tests

golden_test g_crc64_midsummer_gt = {
    .src_filename = "test/data/midsummer.txt",
};

golden_test g_crc64_pi_gt = {
    .src_filename = "test/data/pi.txt",
};

// ---------------- CRC64 Tests

const char*  //
test_wuffs_crc64_ecma_interface() {
  CHECK_FOCUS(__func__);
  wuffs_crc64__ecma_hasher h;
  CHECK_STATUS("initialize",
               wuffs_crc64__ecma_hasher__initialize(
                   &h, sizeof h, WUFFS_VERSION,
                   WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED));
  return do_test__wuffs_base__hasher_u64(
      wuffs_crc64__ecma_hasher__upcast_as__wuffs_base__hasher_u64(&h),
      "test/data/hat.lossy.webp", 0, SIZE_MAX, 0xE52B1F3FF3D3389E);
}

const char*  //
test_wuffs_crc64_ecma_golden() {
  CHECK_FOCUS(__func__);

  struct {
    const char* filename;
    // The want values are determined by script/checksum.go.
    uint64_t want;
  } test_cases[] = {
      {
          .filename = "test/data/hat.bmp",
          .want = 0xEADD85183B8DD1B5,
      },
      {
          .filename = "test/data/hat.gif",
          .want = 0x04365C489DBC96CD,
      },
      {
          .filename = "test/data/hat.jpeg",
          .want = 0xA4C0DB421278B786,
      },
      {
          .filename = "test/data/hat.lossless.webp",
          .want = 0x090AF44557A4E13D,
      },
      {
          .filename = "test/data/hat.lossy.webp",
          .want = 0xE52B1F3FF3D3389E,
      },
      {
          .filename = "test/data/hat.png",
          .want = 0x92E9F67A8948B654,
      },
      {
          .filename = "test/data/hat.tiff",
          .want = 0xB640F37638B639B9,
      },
  };

  for (size_t tc = 0; tc < WUFFS_TESTLIB_ARRAY_SIZE(test_cases); tc++) {
    wuffs_base__io_buffer src = ((wuffs_base__io_buffer){
        .data = g_src_slice_u8,
    });
    CHECK_STRING(read_file(&src, test_cases[tc].filename));

    for (int j = 0; j < 2; j++) {
      wuffs_crc64__ecma_hasher checksum;
      CHECK_STATUS("initialize",
                   wuffs_crc64__ecma_hasher__initialize(
                       &checksum, sizeof checksum, WUFFS_VERSION,
                       WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED));

      uint64_t have = 0;
      size_t num_fragments = 0;
      size_t num_bytes = 0;
      do {
        wuffs_base__slice_u8 data = ((wuffs_base__slice_u8){
            .ptr = src.data.ptr + num_bytes,
            .len = src.meta.wi - num_bytes,
        });
        size_t limit = 101 + 103 * num_fragments;
        if ((j > 0) && (data.len > limit)) {
          data.len = limit;
        }
        have = wuffs_crc64__ecma_hasher__update_u64(&checksum, data);
        num_fragments++;
        num_bytes += data.len;
      } while (num_bytes < src.meta.wi);

      if (have != test_cases[tc].want) {
        RETURN_FAIL("tc=%zu, j=%d, filename=\"%s\": have 0x%016" PRIX64
                    ", want 0x%016" PRIX64 "\n",
                    tc, j, test_cases[tc].filename, have, test_cases[tc].want);
      }
    }
  }
  return NULL;
}

const char*  //
do_test_xxxxx_crc64_ecma_pi(bool mimic) {
  const char* digits =
      "3."
      "141592653589793238462643383279502884197169399375105820974944592307816406"
      "286208998628034825342117067982148086513282306647093844609550582231725359"
      "408128481117450284102701938521105559644622948954930381964428810975665933"
      "446128475648233786783165271201909145648566923460348610454326648213393607"
      "260249141";
  if (strlen(digits) != 299) {
    RETURN_FAIL("strlen(digits): have %d, want 99", (int)(strlen(digits)));
  }

  // The want values are determined by script/checksum.go.
  //
  // wants[i] is the checksum of the first i bytes of the digits string.
  uint64_t wants[300] = {
      0x0000000000000000, 0xDEAB38D23CD56AB6, 0x5C257BEFCFF13F5E,
      0x4BABD3D6697B9834, 0x1FE60AA0B20E44C1, 0x34C4D86E0BC41325,
      0x6202C4935B5F9D03, 0x9A46F1A15337D622, 0x62AC46BA94076EC6,
      0x34B9922211E21A0F, 0x9A104AF7E27D6BA5, 0x2494B24742FED551,
      0xFBD89FFF950FD48C, 0x2B460EB540AFCB27, 0x1AE8349F28C5B4D5,
      0xBDDE413FCD2D1EC7, 0x4C9392FF3C6A7271, 0x8FD32F7CD8D872D1,
      0x0E65369A2D7A5C6E, 0x39D14684478C19D7, 0x82A2707ED574EA50,
      0xFB7EA93DAC985EB3, 0x1EC407757264A919, 0x548FDAE4E78AD430,
      0x90ADDB7AD74F4BE5, 0x7C529D717060C1D7, 0x32EC0E4574BA45DA,
      0x0DFDB8E543882264, 0x8AFC92B43B45B4C6, 0xFCAF4DCCE3655989,
      0xE09A4703D8AFBCCB, 0xB84AE0AEA83F5088, 0xE3FB13F0EDBAFAFF,
      0xB1B19761FF90117D, 0x44F5331DED6D95D3, 0xFA2A26D1E40F49D2,
      0x4A9F04247CFF685C, 0xFBB69449F631D531, 0xD7A898DDD1B90FD0,
      0xFAB97B7A24339D48, 0x4E639C3405727ED1, 0xC65A0913998FF73E,
      0xD4B0C51EDC88ADCF, 0x70804516491376C5, 0xB8DAFAACBDAEEC42,
      0x4E21FFB5D3EBE3A0, 0xEF9BBD96ABB95EB8, 0x650021E526ECD62A,
      0x26DCCBDD2CCB56CF, 0xC35C65A689446A51, 0xFB3F572874C46E33,
      0xEBD2A873E4535B6C, 0x8A1ABDA4ADE26FBF, 0xD56E5D054013CE49,
      0x4E4C4B127A165E82, 0x502359E552CF8C9E, 0x5553E5155C05710B,
      0x6DD027892A2E30A8, 0x60ECB99DC08F6503, 0xA51B17EF5E9ABBE0,
      0x8BC6ACB41AF7A841, 0x35C2206734C3E4D7, 0x0EDF2795369647F8,
      0x8D260A38CE239CC7, 0x4CA36AB43B697CF3, 0x011532F8EFBF81A9,
      0xA87EF3B24D62F5EE, 0xB7B1703D138CF108, 0x526DCD2F01570412,
      0xE7ED3F90BE5EB8F2, 0xF53A4AFA637A1636, 0x1F589B399E04454F,
      0x4DA3CC8A11965534, 0x6430C13B11F9F73F, 0x5B465A05A3CF25A7,
      0x63FF9EBE1B58982B, 0x25FF853C20A8FEBC, 0xD5C1B83DD89E84D8,
      0x4AB0EFBA90C3F991, 0x9E9248C7A8B1601F, 0xD87EC82655C1E3AC,
      0x637CA62C38AE96ED, 0xB45FC336DAA72156, 0xF86DE5D0B3C99145,
      0xBAE57EFD07A52486, 0x2B073354423D61D7, 0x82B0A60B05715B28,
      0x951537E4C14F942D, 0x1A5667A67944548A, 0x288231134E1D9C9A,
      0xD95A00F0AA88414E, 0x711F777C8CCA7837, 0xEB588853B0AB557A,
      0x3F7F198620ADF0A4, 0xAB6528C764F3FBA2, 0xABF132F625B7A5A9,
      0x5C5021E5EBE85D91, 0x2DAAE4B7F46D14D4, 0x0EC74F51E656E908,
      0x6EAE027D39CA7F0D, 0x296411D9BEF21CAF, 0x5CD2B4C6C4731828,
      0xD1449E41F0377567, 0xCD8A517B3411F4A5, 0x97ED64E2CD8FE6A1,
      0xABCDBABA001ED9B4, 0xD66A3B5C09E02EC2, 0x3728E501A021E172,
      0x8FA8940B26443942, 0xFE1D703B20894A27, 0xD1E651850DD38F35,
      0x1F7C4722E16AECD6, 0x75255A768CCCB0C3, 0x34AE1B3EDDFAD1D1,
      0x0EDE4BAE6F7F7ECD, 0xFC2B6F15F9316343, 0x4E650E206AAF7C2F,
      0xEE0921C815AF1439, 0x5BCC63E550CB7344, 0x096893C531E1790B,
      0xA657229B5135C9C1, 0xFC83E67CCC0F29F4, 0xF52124238F0847A7,
      0x97D5CF979534FF12, 0x5406CB2C056D8466, 0xCE2AA2700EBAB269,
      0xB5604D9DAD3E3B6C, 0xF194CCC5BC245CEC, 0x38BC6FD133B249C9,
      0xFF38BC6FD133B249, 0xB9C79AFAEF99C298, 0x9E613FB2E8CE3A24,
      0xD1862DCA841BC845, 0xFD66EFDCB67509EB, 0x7F1AE71B815FE7A8,
      0x947A450AFA64B48F, 0x6C83D6861E275F73, 0xC85945D10A1DC0C8,
      0x3736D67F2D221C9C, 0xE61ABC24C5DDC3F4, 0x7A6F48AA80CE6B25,
      0x96C8595437943720, 0x9624FE45C9236D7C, 0x3F0265F036D4789C,
      0xE61288974AC63590, 0xE5EE29247AE43BE0, 0x8B86598AD1D3D6C1,
      0xFCAE3707DD8FCFEB, 0x7F1B2FC35A341D6E, 0x89ABC57D4A0214C4,
      0xFCAC1A9B2A141E29, 0xD2C151B1FA870E5C, 0xFB2ECA1C63B7AD57,
      0xC77D9CEA9A463CF3, 0x02BB5D50E640B2D4, 0x81BCAB3B568B3AC6,
      0x08203BA2C448375C, 0xBF7BE0C18C308167, 0x063F50611F1BEE30,
      0x647B5DA7FAF77A84, 0xE327224DE4E832D5, 0x815D37444B899246,
      0x86E6F742C735C582, 0x53CE4207552CB338, 0x938F2BBC63A3F1BF,
      0xAEA70B17A752D1E4, 0xB492189BE138DD11, 0xA3840BF2F6073422,
      0xAA6B0B7A3B8A5F78, 0x8C1066BB0A818EC1, 0x34572E02107C9CEF,
      0x408CDEE95CCF5307, 0xE2910D6181E104E2, 0xCC2B5C473FCF2CCA,
      0xFFCC2B5C473FCF2C, 0x1A3CBEBAC1C224D1, 0x495AA3E24084B90C,
      0xA61710AB7644AC01, 0x2A8919953FE38E41, 0x864D236C1641AF9E,
      0x9D7B04552969FE74, 0xC8A8BD03D92A8E69, 0x0648833CDD4EF43F,
      0x5B242247A40392A4, 0xAB0173FCA57755C0, 0x4FA0FC93A85C3407,
      0x2A60AE79073D96D9, 0xF9610212D7BB05EC, 0xB7E06FCCB31628F8,
      0x01EE71FD9737FEFD, 0xF6F9F8EA590D634D, 0x35BF1F336A801E1C,
      0x2C51D32609437289, 0xDC3000F0662D3BE7, 0x889A36FD529103FC,
      0x7A01C820595927E5, 0x40C2880F7E8676BC, 0x5AF070E3BC0F5438,
      0xAFFCACE3D322735F, 0xBCF98D089A79F71E, 0x2FFD24EA65ED975D,
      0xFBD3F6693828C7CE, 0xC3816A9B3D5089C0, 0x4FC87C8ACFC413DB,
      0x82D46944DBFCA25A, 0x770F9BA9960734AE, 0x2479AD961C8AAF0E,
      0x1671CE4556E1E145, 0x8671DBBBC628ADF1, 0x01DFE049E042C078,
      0xC83419E7C5E3A557, 0x80E4FCD0CA0797CF, 0x377E6BC62CE206CB,
      0x0BB3483C6E6C425D, 0x302C866145555B7A, 0xB0F0985B4597F0C2,
      0x8460331CA4CAC9C3, 0x345F5E57B7D2D7A8, 0x1B65F560E19DC7D7,
      0x0EF18040314319DB, 0x829550B811022550, 0x0FFAA84AC21C8827,
      0xED6D0F302859BBF0, 0x461E8674C04B4E5A, 0x74EEE018F1429F7F,
      0xCB6499B9C3ACB935, 0xEBE2F3BD75E433BB, 0xAEDF66CFA6449626,
      0x6298DF2DFAF21D86, 0xA48BBB26C50FC886, 0x283CECCFCEA1D706,
      0xE2F9BD53A7736A66, 0xCE9C5D067118AC87, 0x2B734A77B94BDC5F,
      0x77A63C8AA56583D0, 0xBE96BE698FFEA2CD, 0x846E553A9600A091,
      0xA2262F107CDF0241, 0xB99A846990342E28, 0xD284672F083D2E6C,
      0x390DA7D5F2A95EA5, 0xECC920909CC60038, 0x201E6060F7CD4463,
      0xB58E795FBDC74C9A, 0x121C32DC4D0D86FF, 0xCA90B3C42CBFF832,
      0x1F6731C0A04B80A1, 0x186B7C5121D442BD, 0x5AA8D917E250060C,
      0x51A565D87CF8C4D8, 0xBEB0BD30DD273F8A, 0x1779DFFADB8F42CB,
      0x37E9F6E506F38E1E, 0xE788BBAB74591C78, 0x44A30A3127E65CDE,
      0xBEA5BB5F347C2112, 0x6B70B36BCEFA7182, 0x5323D4437C257C8C,
      0x14B1FDCDFF705CB5, 0x917F3DF2D5F8BF73, 0xB3747ABE8159411B,
      0x10ADA12ED0C00AD1, 0x0EFA48147F724416, 0x2C6A96712E5680D3,
      0x75164CA7DF038CAF, 0xEFA08A25B9B5B6D7, 0x75D586BB8B946F99,
      0x2EA6D04EFD5374DB, 0x82B507E81FCE353D, 0xD7D19B4E7050F030,
      0x902E853B7DD891C1, 0x344B10E1900BC5F0, 0xB243963CB9B3BC7F,
      0xCBA234CFE7E44816, 0xA3FB3BDEA201E8B7, 0x91C87734C6A5CEC7,
      0x4F9A3597603FE69C, 0x554C5C792E378161, 0x3AAFDEB0F3D14837,
      0xACB94213D733B17E, 0x789282A5CB2D9774, 0xF7125CB62AFE773A,
      0xE8FB34262D537D48, 0xB9D05972A665A257, 0xFB45DB14A0EB4FFB,
      0x452D1FFEB39DE093, 0x2E9628D7B86B7D54, 0xBC78E78CAE12BE10,
      0x6B726E371D601F1D, 0xDBAE995EF22A2EEE, 0x8BB8193AAB5B18D4,
      0xC69FD2969721DE58, 0xF81F25C1138417BA, 0xD51C589D25ADA831,
      0x58D2C7C252A56DC5, 0x87ADA01D6A83E763, 0x0522377F94A3416B,
      0x05A0B8E8F65D61CD, 0x7051556BBF39A309, 0x6ED0946703931047,
  };

  for (int i = 0; i < 300; i++) {
    uint64_t have = 0;
    wuffs_base__slice_u8 data = ((wuffs_base__slice_u8){
        .ptr = (uint8_t*)(digits),
        .len = (size_t)(i),
    });

    if (mimic) {
      // A simple, slow CRC-64 ECMA implementation, 1 bit at a time.
      have = 0xFFFFFFFFFFFFFFFF;
      while (data.len--) {
        uint8_t byte = *data.ptr++;
        for (int i = 0; i < 8; i++) {
          if ((have ^ byte) & 1) {
            have = (have >> 1) ^ 0xC96C5795D7870F42;
          } else {
            have = (have >> 1);
          }
          byte >>= 1;
        }
      }
      have ^= 0xFFFFFFFFFFFFFFFF;

    } else {
      wuffs_crc64__ecma_hasher checksum;
      CHECK_STATUS("initialize",
                   wuffs_crc64__ecma_hasher__initialize(
                       &checksum, sizeof checksum, WUFFS_VERSION,
                       WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED));
      have = wuffs_crc64__ecma_hasher__update_u64(&checksum, data);
    }

    if (have != wants[i]) {
      RETURN_FAIL("i=%d: have 0x%016" PRIX64 ", want 0x%016" PRIX64, i, have,
                  wants[i]);
    }
  }
  return NULL;
}

const char*  //
test_wuffs_crc64_ecma_pi() {
  CHECK_FOCUS(__func__);
  return do_test_xxxxx_crc64_ecma_pi(false);
}

// ---------------- Mimic Tests

#ifdef WUFFS_MIMIC

const char*  //
test_mimic_crc64_ecma_pi() {
  CHECK_FOCUS(__func__);
  return do_test_xxxxx_crc64_ecma_pi(true);
}

#endif  // WUFFS_MIMIC

// ---------------- CRC64 Benches

uint64_t g_wuffs_crc64_unused_u64;

const char*  //
wuffs_bench_crc64_ecma(wuffs_base__io_buffer* dst,
                       wuffs_base__io_buffer* src,
                       uint32_t wuffs_initialize_flags,
                       uint64_t wlimit,
                       uint64_t rlimit) {
  uint64_t len = src->meta.wi - src->meta.ri;
  if (rlimit) {
    len = wuffs_base__u64__min(len, rlimit);
  }
  wuffs_crc64__ecma_hasher checksum = {0};
  CHECK_STATUS("initialize", wuffs_crc64__ecma_hasher__initialize(
                                 &checksum, sizeof checksum, WUFFS_VERSION,
                                 wuffs_initialize_flags));
  g_wuffs_crc64_unused_u64 = wuffs_crc64__ecma_hasher__update_u64(
      &checksum, ((wuffs_base__slice_u8){
                     .ptr = src->data.ptr + src->meta.ri,
                     .len = len,
                 }));
  src->meta.ri += len;
  return NULL;
}

const char*  //
bench_wuffs_crc64_ecma_10k() {
  CHECK_FOCUS(__func__);
  return do_bench_io_buffers(
      wuffs_bench_crc64_ecma,
      WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED, tcounter_src,
      &g_crc64_midsummer_gt, UINT64_MAX, UINT64_MAX, 2000);
}

const char*  //
bench_wuffs_crc64_ecma_100k() {
  CHECK_FOCUS(__func__);
  return do_bench_io_buffers(
      wuffs_bench_crc64_ecma,
      WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED, tcounter_src,
      &g_crc64_pi_gt, UINT64_MAX, UINT64_MAX, 200);
}

// ---------------- Mimic Benches

#ifdef WUFFS_MIMIC

// No mimic benches.

#endif  // WUFFS_MIMIC

// ---------------- Manifest

proc g_tests[] = {

    test_wuffs_crc64_ecma_golden,
    test_wuffs_crc64_ecma_interface,
    test_wuffs_crc64_ecma_pi,

#ifdef WUFFS_MIMIC

    test_mimic_crc64_ecma_pi,

#endif  // WUFFS_MIMIC

    NULL,
};

proc g_benches[] = {

    bench_wuffs_crc64_ecma_10k,
    bench_wuffs_crc64_ecma_100k,

#ifdef WUFFS_MIMIC

// No mimic benches.

#endif  // WUFFS_MIMIC

    NULL,
};

int  //
main(int argc, char** argv) {
  g_proc_package_name = "std/crc64";
  return test_main(argc, argv, g_tests, g_benches);
}
