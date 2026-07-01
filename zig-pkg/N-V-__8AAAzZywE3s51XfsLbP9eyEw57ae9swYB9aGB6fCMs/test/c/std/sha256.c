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
  $CC -std=c99 -Wall -Werror sha256.c && ./a.out
  rm -f a.out
done

Each edition should print "PASS", amongst other information, and exit(0).

Add the "wuffs mimic cflags" (everything after the colon below) to the C
compiler flags (after the .c file) to run the mimic tests.

To manually run the benchmarks, replace "-Wall -Werror" with "-O3" and replace
the first "./a.out" with "./a.out -bench". Combine these changes with the
"wuffs mimic cflags" to run the mimic benchmarks.
*/

// ¿ wuffs mimic cflags: -DWUFFS_MIMIC -lcrypto -lssl

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
#define WUFFS_CONFIG__MODULE__SHA256

// If building this program in an environment that doesn't easily accommodate
// relative includes, you can use the script/inline-c-relative-includes.go
// program to generate a stand-alone C file.
#include "../../../release/c/wuffs-unsupported-snapshot.c"
#include "../testlib/testlib.c"
#ifdef WUFFS_MIMIC
#include "../mimiclib/openssl.c"
#endif

// ---------------- Golden Tests

golden_test g_sha256_midsummer_gt = {
    .src_filename = "test/data/midsummer.txt",
};

golden_test g_sha256_pi_gt = {
    .src_filename = "test/data/pi.txt",
};

// ---------------- SHA256 Tests

const char*  //
test_wuffs_sha256_interface() {
  CHECK_FOCUS(__func__);
  wuffs_sha256__hasher h;
  CHECK_STATUS("initialize",
               wuffs_sha256__hasher__initialize(
                   &h, sizeof h, WUFFS_VERSION,
                   WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED));
  return do_test__wuffs_base__hasher_bitvec256(
      wuffs_sha256__hasher__upcast_as__wuffs_base__hasher_bitvec256(&h),
      "test/data/hat.lossy.webp", 0, SIZE_MAX,
      wuffs_base__make_bitvec256(0x5CC50E1942D6F258, 0x6AB8300CEABA7095,
                                 0x204081787511F1A3, 0xC3E4E8E405B501CB));
}

const char*  //
test_wuffs_sha256_golden() {
  CHECK_FOCUS(__func__);

  struct {
    const char* filename;
    // The want values are determined by script/checksum.go.
    wuffs_base__bitvec256 want;
  } test_cases[] = {
      {
          .filename = "test/data/hat.bmp",
          .want = wuffs_base__make_bitvec256(
              0x2FC725BA7661C777, 0x5D80DB0568DB9741, 0x14CE2B683B211925,
              0x0EE3A9C0B94EBD3E),
      },
      {
          .filename = "test/data/hat.gif",
          .want = wuffs_base__make_bitvec256(
              0x64E368566B47BA08, 0xF37522D2D09BF24A, 0x483A2E527CC031B0,
              0xDA46A35274A6B3DB),
      },
      {
          .filename = "test/data/hat.jpeg",
          .want = wuffs_base__make_bitvec256(
              0x2C6B9551F7574AB3, 0x7486DECA160CA57C, 0x50F1B82175229C03,
              0x6085C5A688495233),
      },
      {
          .filename = "test/data/hat.lossless.webp",
          .want = wuffs_base__make_bitvec256(
              0x7AC2F1DC78F9AC80, 0xCC1142375132C3C2, 0x311B3C16A251A65E,
              0xD91429191B532107),
      },
      {
          .filename = "test/data/hat.lossy.webp",
          .want = wuffs_base__make_bitvec256(
              0x5CC50E1942D6F258, 0x6AB8300CEABA7095, 0x204081787511F1A3,
              0xC3E4E8E405B501CB),
      },
      {
          .filename = "test/data/hat.png",
          .want = wuffs_base__make_bitvec256(
              0x23958198999B6D59, 0x2DF493278110663A, 0x7A1BDBCAE77A1D75,
              0xD3F360AF629B5780),
      },
      {
          .filename = "test/data/hat.tiff",
          .want = wuffs_base__make_bitvec256(
              0xFAC8637F04EC2392, 0x373D5F707D803B99, 0x852E5F5F54D5BBD0,
              0xF44307C419C5F9BB),
      },
  };

  for (size_t tc = 0; tc < WUFFS_TESTLIB_ARRAY_SIZE(test_cases); tc++) {
    wuffs_base__io_buffer src = ((wuffs_base__io_buffer){
        .data = g_src_slice_u8,
    });
    CHECK_STRING(read_file(&src, test_cases[tc].filename));

    for (int j = 0; j < 2; j++) {
      wuffs_sha256__hasher checksum;
      CHECK_STATUS("initialize",
                   wuffs_sha256__hasher__initialize(
                       &checksum, sizeof checksum, WUFFS_VERSION,
                       WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED));

      wuffs_base__bitvec256 have = wuffs_base__make_bitvec256(0u, 0u, 0u, 0u);
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
        have = wuffs_sha256__hasher__update_bitvec256(&checksum, data);
        num_fragments++;
        num_bytes += data.len;
      } while (num_bytes < src.meta.wi);

      if ((have.elements_u64[0] != test_cases[tc].want.elements_u64[0]) ||
          (have.elements_u64[1] != test_cases[tc].want.elements_u64[1]) ||
          (have.elements_u64[2] != test_cases[tc].want.elements_u64[2]) ||
          (have.elements_u64[3] != test_cases[tc].want.elements_u64[3])) {
        RETURN_FAIL(
            "tc=%zu, j=%d, filename=\"%s\": "            //
            "have 0x%016" PRIX64 "_%016" PRIX64          //
            "_%016" PRIX64 "_%016" PRIX64                //
            ", want 0x%016" PRIX64 "_%016" PRIX64        //
            "_%016" PRIX64 "_%016" PRIX64,               //
            tc, j, test_cases[tc].filename,              //
            have.elements_u64[3], have.elements_u64[2],  //
            have.elements_u64[1], have.elements_u64[0],  //
            test_cases[tc].want.elements_u64[3],         //
            test_cases[tc].want.elements_u64[2],         //
            test_cases[tc].want.elements_u64[1],         //
            test_cases[tc].want.elements_u64[0]);
      }
    }
  }
  return NULL;
}

const char*  //
do_test_xxxxx_sha256_pi(bool mimic) {
  const char* digits =
      "3."
      "141592653589793238462643383279502884197169399375105820974944592307816406"
      "2862089986280348253421170";
  if (strlen(digits) != 99) {
    RETURN_FAIL("strlen(digits): have %d, want 99", (int)(strlen(digits)));
  }

  // The want values are determined by script/checksum.go.
  //
  // wants[(4*i) .. ((4*i)+4)] is the checksum of the first i bytes of the
  // digits string.
  uint64_t wants[400] = {
      0xA495991B7852B855, 0x27AE41E4649B934C, 0x9AFBF4C8996FB924,
      0xE3B0C44298FC1C14, 0x640B7E4729B49FCE, 0x16B72230967DE01F,
      0x60CE05C1DECFE3AD, 0x4E07408562BEDB8B, 0x614E6AB3B84FA55C,
      0xD208FA9C2FAFD974, 0x9BD88B9F5D1962D3, 0x267C23695B6E8FC2,
      0xF9E34E60E3AE4F44, 0xF59FCAAF3A56B538, 0xA8D86E4F33294BE0,
      0x35EFC6DED4E13F29, 0x7EE6614143500215, 0x7107CA98B0A6C242,
      0xD6698EA1047F5C0A, 0x2EFFF1261C25D94D, 0x00EC68941F27440C,
      0x038F6515DC7AD2A2, 0x02A4C5061F8AD16C, 0xA4F7BA76DB1A09F4,
      0x3A3A1DD8934D1958, 0xCE872F8C370A08DC, 0xF91001FCB6DAB28B,
      0x4E25F49BC91285E1, 0x527E79D0A3035852, 0xCC0BF86F2A60ED7E,
      0x9C8D5AB452E8B69B, 0xC0740DD25C9DE39B, 0x5D827521BB8D54DF,
      0x0C44B4C673812078, 0x0A05F1DE66EE555F, 0x8C954CD9B6C8F818,
      0x2FC3A26B33A91CD8, 0x2AF07CC393025875, 0xFF121FED87539C49,
      0x8721D1333B764926, 0xA4CD30C8355943FD, 0x4481071EAFEDEB1E,
      0x19B8C4ECE54CA63D, 0xCAAC67952F8CD5C1, 0xD21560096807F0CB,
      0xD2D991AF41C5EF75, 0x7FB2FADEB54168EC, 0x3DEC3E77C7B4AE98,
      0xFEEF53EFE3CBEE09, 0x7C1533C0CB71A163, 0x612F6D5C2BA2B291,
      0x330548C742A7C77A, 0xAA3566FE98696357, 0x5C595BAB01FDBDFB,
      0xB3150FCE2878C94A, 0x32FA47541DA9C49E, 0x8A2BAD94F28B1D20,
      0x423A5FB1BBFF501E, 0xE9F3784E332A1CCA, 0x1F0960C3EF5338A9,
      0x0CCCEEC918ED7221, 0xF989C8334DAABE36, 0xA6983AEDD3CAD63F,
      0x1564E00B0413DB17, 0xFAE59D87E444EA39, 0x76DDEB33198A342C,
      0x07E4D2A1F7B2225A, 0xB9A283FF2DBC461A, 0x71263158DFFB4E87,
      0x1DA33E2623A69F0A, 0x0D28F3CC44E9A624, 0xF0A1B7875844C3AE,
      0x21FBDC5793903EB6, 0x1192B6E4BDEC029C, 0x280ABB25EE957ACB,
      0xF983F1F5CF4BD0F5, 0x8F8DA51908599678, 0x8BF1794930B31D48,
      0xE035C38FE2581543, 0x99B340ACCCD6137D, 0x76610E7F8041E29D,
      0x79C81D3CF596E134, 0xB5E234BF4F2E2F2F, 0xA630621B389E0269,
      0xA32BEE851AA8195F, 0x1916D3919A37F28B, 0x52BD93F5CF2BB1C9,
      0x203A83F7BC429000, 0xB941961D7FD0DC88, 0x52AE42D747CC9B7F,
      0x779C702B13086F27, 0x2C40420C10F6C5C9, 0x89E082DA67E7A2BA,
      0xD62C06AE3B1262D5, 0x4E0846AEFE097B34, 0x5E6DED4E901BE009,
      0x9A74E9A524DEF058, 0x13FA1A8D6184DE71, 0x1BEDE46B96346B8E,
      0x8BFE4F313A316226, 0x0D2B653DA3F00458, 0x676C5640EC523D07,
      0xDA7B1A41CCAE7D4E, 0x641FB4D8D2D13726, 0xCEE611F144692B66,
      0x987643879977A1BD, 0xFB1904FC6EE0D915, 0x70FE652365FEDCF9,
      0x124A7D6202DE83FC, 0x137C66C93DD8A735, 0x4A43DA05BCD32DBA,
      0x6E172C87859B68E3, 0xD909261806A23376, 0x71DCC0C5DB9810FE,
      0x1743C2DA282BE204, 0xE574568E07C2F447, 0x47004C6705CF1330,
      0x189F8C3D310C9189, 0xFFBB604311CBA061, 0x57C45B1D54E9293D,
      0x407FD98EA2EC9790, 0xA3A1BA2CAD658AEF, 0xE177F3D9A301E214,
      0xEBF75EB10D6B8225, 0x9449C881047FEF89, 0x345EAEF124098DFF,
      0x76293F79FA5D6837, 0xBA81CC1276BFF143, 0x8194DA38905F2A76,
      0x57496BFA62C36212, 0x5CC7F80FCA76488B, 0x27F6EF06C05A6667,
      0x859A911428426908, 0xAC839788AD1E3AFE, 0xBBCF0E62C9C98465,
      0x16C99E22FE8F336D, 0x7944DCB715D2B48E, 0x6D500BA806F21FED,
      0xE2C0783E3EA9F6BE, 0xAD95772328B63AA4, 0x9F2DAA9DD934B738,
      0x19B67552963DBA24, 0x4AA889C916C78EEC, 0xE183CB786F7D0225,
      0x8204624FB1DF4072, 0x1C52B79198588F75, 0x5215491F588E9061,
      0xC0ECB4BF497B3D24, 0xA3DDCF951C863B90, 0x3DC34E03715D1E86,
      0x93C7003309B2B16D, 0xDB8DC55940E6D405, 0x22A674F0A5C51BEE,
      0xB9CC42BE352273A4, 0x5288081DC7BE6307, 0x3ABFFE72285284CB,
      0xB03A58DF3811A955, 0x4D937AB0E3F28BC7, 0xFF1260C7907EBD92,
      0xEA012389BB29FCFA, 0xA6F634F7B0A91D90, 0xA1C732F0466BA190,
      0x1694A93FA04F924F, 0x7A68A298FBBE78F8, 0x688784515705B894,
      0x7BF3B2235A9BFCCF, 0x3A8D7B10D3E16143, 0xBEA09BF0EBEC453C,
      0x7A964C634DF35A00, 0xA578BC3FB1805216, 0x22511DCBA44BE006,
      0x473CF801B7E7DC82, 0x2E61B3D57F5A6400, 0xECFA2730978ECD31,
      0x4D76B0F79873109D, 0xA5E570AA856BEFC9, 0x85D50A9055BCC917,
      0x09B71ACFE36EC46A, 0xB7219D64858B1D1B, 0x6322E0AEA4762644,
      0x6110A33948746949, 0xA3834380FA84B98C, 0x416827D9BD10517B,
      0x205191477BA5E2D4, 0x8583927ED0D61E42, 0xF23C26620985246C,
      0x463FA0BD48E2D9CD, 0x5BE1E31786234856, 0x076B608F1F16EC54,
      0xFCF291E7DE66100D, 0x027B9593BE686708, 0xF7ECCC236C2F031F,
      0x101C24CB8038FFA2, 0x36FBD8565EE584ED, 0x981E43EE73218C77,
      0xE99B6E19BF8CE8DA, 0x5A45E8D434AD96C8, 0x949668B5F689B688,
      0x2596516D1350B71E, 0x6739DB9427BB8C02, 0x8031B2D3C446DD6E,
      0x4C2224CC1742F920, 0x59093F03EDBA8CD1, 0x7CB4BB75F9E08F1A,
      0x6B64AEB9CD1CDFF5, 0x2CA127A0C57C6D6C, 0xE1D9E0F0471EB1AC,
      0x3A335F3D7180856D, 0xD87D65A08487BF82, 0x07864D342C10C819,
      0x511BF7E38AF42BAE, 0x8B5D843EC502DA3B, 0x80125447E4888B8F,
      0xA1CC69B692DEA1B2, 0xDDD2B9DB35EEFC4E, 0x49F11DA1947B362E,
      0x34E40280999D3EA6, 0xA491F7A67A19442E, 0x38CD84D6A14D87F1,
      0xD3A19502C9450B5F, 0xDF47D72860C62910, 0xB29D464603F6E8F0,
      0x82F22502D37F5561, 0x4B4450DC4DC1583D, 0x85BADFAFE321525E,
      0x8296939672321D16, 0x36E12D7A4AB1B08A, 0x860650B802184FD4,
      0xE94462C6B55C3FF6, 0xE83FED74E65B3EDD, 0x3EE9B1254D10432D,
      0x78E5956D6BC70090, 0xF6F0EB6D1E334F56, 0xD7452F3FB84E4A36,
      0x6B6356F7826A9DD1, 0xF8B38562F8AF7F5A, 0x83442AE9FE5BE79B,
      0x9B6E82F6BCF031AB, 0xF36FDF177673476D, 0x112E1F92164E0E52,
      0x0BFA74605FB8A9B8, 0x48E45F150E86DEC8, 0xC4BA24790041D453,
      0x9AA2B91B81053FC2, 0x2A7A944B1AF44A23, 0x4A471FF96E24FDB7,
      0x9A2C561F8A2401C2, 0xA7AF4303D32EF538, 0xF6D40B18E4846693,
      0xF73C7D03097E2A85, 0x5963AAAC8B2E391A, 0x2326CF3AD8A24949,
      0x2955AA4CFD0955B0, 0xEFB503670B6B1CDA, 0x8F36DD3C25A09004,
      0x72C73ED09101189B, 0xE48C7DBF4C345FFA, 0xF465188486D12732,
      0x8F7191F537DC3023, 0xD76FBBE816AB8193, 0x78971FF1C5759434,
      0x9EBB4D1E515EE18A, 0x9F34BC53A2BEACEF, 0xA0DA75B06F9F8EA6,
      0x38E11C269938E34C, 0x47ADCCF552546D6D, 0x4728ED1535C94B4A,
      0xEC794468363C2E0E, 0x2938E9D38B388722, 0x2DC07DBB29AF4947,
      0x1E3B4531213FFBA2, 0x957E8DCC3E00727C, 0xD6C75341415F04D6,
      0xE336F0579666B64F, 0x1BA3345394F98EB3, 0x375B0F6C5B15DD3D,
      0xE04DB04888B4380D, 0xF83213CF696EE88C, 0x9B31BCE678AD6B18,
      0xF8EFC9964ADE38E4, 0xBC2004109A0E3AFA, 0xFAF1EA27888B1A3C,
      0x5E98EAF21609A0A7, 0x118C1DB41DA78E81, 0x86C4619A31B85BFC,
      0x5F8C9067ACAA4269, 0xF0B267621ACC79EA, 0x9C6050C66D54D3BC,
      0x24818E3F7A769CBD, 0x2250FA7C3FCB1CBB, 0xB61B7A8E81ED778D,
      0x4538060EB1141D17, 0xA221626CF97D97CB, 0x8A7723D63091967E,
      0x080F04C095C14517, 0x323E944917F9EB93, 0xA3EF852EDF53A001,
      0xA4353F820201EE29, 0x4F73E39FEE8BDBFE, 0xF0ACAAA72D2B62F7,
      0x1BE147496E7750CA, 0x91189876D80EDE4C, 0x3389F591EE806FBB,
      0x7C74F4B2303DAD0D, 0x5FBECE9186741846, 0xB04B9AE381183FAE,
      0xEE6D1C39B794CE62, 0x9016FB0579FA9402, 0x97EC8D2C65EA43B8,
      0xF2D37ED8E008422A, 0xA88DFD8B0F3AAB89, 0xD7CF6CDF34BFD209,
      0xE3628B3721931FDB, 0x0C451B3E3B436D15, 0x239BB072E5B34E41,
      0x3DC51A97B5A940C1, 0xAC0161A0DA84D53B, 0x345394823D66FC62,
      0x1B7F7E176A836AEE, 0x6612B6D3AFB30CA8, 0x01C47C26E7B2CC00,
      0x043B730599D20260, 0x3F9E463CB7C946B2, 0x4A8DAAEBFB336D43,
      0xF9B617983070ED60, 0x9AF2AF0BA91EA0D7, 0x0AC44EF9103AE372,
      0x8B66DBF1D2C5E991, 0xC3B16BB058908557, 0xBD8240471D86C144,
      0x3E2C14E27BC64098, 0xDDCD99498E58F066, 0xE388E41366082790,
      0xA63D08B8AF4977D9, 0x0BCF499AFA092637, 0xDBB0A064FFA9E303,
      0xBD45DDD5BA1477EE, 0xB8F750B4D84076E9, 0x3A535BB3BDE3D3D9,
      0x9525F2A6EE6F38D3, 0x6CDF53C189D3E726, 0x538AB825624B96DD,
      0x730DE4A6B79DD8B1, 0x51B74D121690D957, 0x8E3E7626617E0B24,
      0xC44263402923D601, 0xD0530C69581EF7EF, 0xBA6C3F80FCF5DB55,
      0x4B7E7C204C8ACD97, 0xB0E7053FD43FACD4, 0xED5F518F302EFD69,
      0x1624AE47324832FC, 0x7F17547B953914AD, 0x525C47949C18BCEA,
      0x313955A415351A58, 0x307E18AD60ED4B54, 0xB03344E911DF01D6,
      0x20C38803D44CF9CD, 0xDDB3956944628B64, 0x11A5DEA12D3DB3EC,
      0x593659D66BC87815, 0xE5AF653BAE6F1869, 0xF47815E9DE6A716E,
      0x2508DCD62F17EAD9, 0x64A61858259ED57B, 0xFDAAE31E9178B455,
      0x266008D2BEBD6ECC, 0x01A7BC2FBD4F1FF1, 0x75D074C7FE629A1D,
      0x726F46CB272A92A1, 0x217F12238920FE7A, 0x99A0B1A4A8EB311C,
      0x48E5BD99F0A5CDDE, 0x0CC73F9EFFEA61E2, 0x142BADB1DCD76ED6,
      0x6B6696D8D16E993E, 0xF73977B29D3332B2, 0x3532EC396A463E8D,
      0x08485AF320F6EF4A, 0x3BE313B7F5F5462D, 0xC7EDDABFD3A52DEC,
      0xE3C0AE2EE970AB95, 0x59DACF3BB3C53E2B, 0x3E135135F158E39C,
      0x4B25757F5EF36523, 0xDCE58EEC3A4E8B12, 0x27A7532E62B6147F,
      0xAA5D819242B97295, 0x815E4E99C89F0115, 0x90AD0232C1A6A145,
      0xE6FC5968AC8C1AFE, 0xB24A3CF866116B23, 0x3134AEF31EA120B2,
      0xFBD01F222353BCFA, 0xC5C85E7FE228C34A, 0x385ABA08C78A08FB,
      0x28EEFFB4F741E27D, 0x10F7CA989D8373CE, 0x99E053B9784F8535,
      0x350A957CEEA5AA93,
  };

  for (int i = 0; i < 100; i++) {
    wuffs_base__bitvec256 have = wuffs_base__make_bitvec256(0u, 0u, 0u, 0u);
    wuffs_base__slice_u8 data = ((wuffs_base__slice_u8){
        .ptr = (uint8_t*)(digits),
        .len = (size_t)(i),
    });

    if (mimic) {
#ifdef WUFFS_MIMIC
      have = mimic_sha256_one_shot_checksum_bitvec256(data);
#endif  // WUFFS_MIMIC

    } else {
      wuffs_sha256__hasher checksum;
      CHECK_STATUS("initialize",
                   wuffs_sha256__hasher__initialize(
                       &checksum, sizeof checksum, WUFFS_VERSION,
                       WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED));
      have = wuffs_sha256__hasher__update_bitvec256(&checksum, data);
    }

    if ((have.elements_u64[0] != wants[(4 * i) + 0]) ||
        (have.elements_u64[1] != wants[(4 * i) + 1]) ||
        (have.elements_u64[2] != wants[(4 * i) + 2]) ||
        (have.elements_u64[3] != wants[(4 * i) + 3])) {
      RETURN_FAIL(
          "i=%d: "                                     //
          "have 0x%016" PRIX64 "_%016" PRIX64          //
          "_%016" PRIX64 "_%016" PRIX64                //
          ", want 0x%016" PRIX64 "_%016" PRIX64        //
          "_%016" PRIX64 "_%016" PRIX64,               //
          i,                                           //
          have.elements_u64[3], have.elements_u64[2],  //
          have.elements_u64[1], have.elements_u64[0],  //
          wants[(4 * i) + 3], wants[(4 * i) + 2],      //
          wants[(4 * i) + 1], wants[(4 * i) + 0]);
    }
  }
  return NULL;
}

const char*  //
test_wuffs_sha256_pi() {
  CHECK_FOCUS(__func__);
  return do_test_xxxxx_sha256_pi(false);
}

// ---------------- Mimic Tests

#ifdef WUFFS_MIMIC

const char*  //
test_mimic_sha256_pi() {
  CHECK_FOCUS(__func__);
  return do_test_xxxxx_sha256_pi(true);
}

#endif  // WUFFS_MIMIC

// ---------------- SHA256 Benches

wuffs_base__bitvec256 g_wuffs_sha256_unused_bitvec256;

const char*  //
wuffs_bench_sha256(wuffs_base__io_buffer* dst,
                   wuffs_base__io_buffer* src,
                   uint32_t wuffs_initialize_flags,
                   uint64_t wlimit,
                   uint64_t rlimit) {
  uint64_t len = src->meta.wi - src->meta.ri;
  if (rlimit) {
    len = wuffs_base__u64__min(len, rlimit);
  }
  wuffs_sha256__hasher checksum = {0};
  CHECK_STATUS("initialize", wuffs_sha256__hasher__initialize(
                                 &checksum, sizeof checksum, WUFFS_VERSION,
                                 wuffs_initialize_flags));
  g_wuffs_sha256_unused_bitvec256 = wuffs_sha256__hasher__update_bitvec256(
      &checksum, ((wuffs_base__slice_u8){
                     .ptr = src->data.ptr + src->meta.ri,
                     .len = len,
                 }));
  src->meta.ri += len;
  return NULL;
}

const char*  //
bench_wuffs_sha256_10k() {
  CHECK_FOCUS(__func__);
  return do_bench_io_buffers(
      wuffs_bench_sha256,
      WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED, tcounter_src,
      &g_sha256_midsummer_gt, UINT64_MAX, UINT64_MAX, 100);
}

const char*  //
bench_wuffs_sha256_100k() {
  CHECK_FOCUS(__func__);
  return do_bench_io_buffers(
      wuffs_bench_sha256,
      WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED, tcounter_src,
      &g_sha256_pi_gt, UINT64_MAX, UINT64_MAX, 10);
}

// ---------------- Mimic Benches

#ifdef WUFFS_MIMIC

const char*  //
bench_mimic_sha256_10k() {
  CHECK_FOCUS(__func__);
  return do_bench_io_buffers(
      mimic_bench_sha256,
      WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED, tcounter_src,
      &g_sha256_midsummer_gt, UINT64_MAX, UINT64_MAX, 100);
}

const char*  //
bench_mimic_sha256_100k() {
  CHECK_FOCUS(__func__);
  return do_bench_io_buffers(
      mimic_bench_sha256,
      WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED, tcounter_src,
      &g_sha256_pi_gt, UINT64_MAX, UINT64_MAX, 10);
}

#endif  // WUFFS_MIMIC

// ---------------- Manifest

proc g_tests[] = {

    test_wuffs_sha256_golden,
    test_wuffs_sha256_interface,
    test_wuffs_sha256_pi,

#ifdef WUFFS_MIMIC

    test_mimic_sha256_pi,

#endif  // WUFFS_MIMIC

    NULL,
};

proc g_benches[] = {

    bench_wuffs_sha256_10k,
    bench_wuffs_sha256_100k,

#ifdef WUFFS_MIMIC

    bench_mimic_sha256_10k,
    bench_mimic_sha256_100k,

#endif  // WUFFS_MIMIC

    NULL,
};

int  //
main(int argc, char** argv) {
  g_proc_package_name = "std/sha256";
  return test_main(argc, argv, g_tests, g_benches);
}
