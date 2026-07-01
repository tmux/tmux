// Copyright 2017 The Wuffs Authors.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

// ---------------- Version

// WUFFS_VERSION is the major.minor.patch version, as per https://semver.org/,
// as a uint64_t. The major number is the high 32 bits. The minor number is the
// middle 16 bits. The patch number is the low 16 bits. The pre-release label
// and build metadata are part of the string representation (such as
// "1.2.3-beta+456.20181231") but not the uint64_t representation.
//
// WUFFS_VERSION_PRE_RELEASE_LABEL (such as "", "beta" or "rc.1") being
// non-empty denotes a developer preview, not a release version, and has no
// backwards or forwards compatibility guarantees.
//
// WUFFS_VERSION_BUILD_METADATA_XXX, if non-zero, are the number of commits and
// the last commit date in the repository used to build this library. Within
// each major.minor branch, the commit count should increase monotonically.
//
// ¡ Some code generation programs can override WUFFS_VERSION.
#define WUFFS_VERSION 0
#define WUFFS_VERSION_MAJOR 0
#define WUFFS_VERSION_MINOR 0
#define WUFFS_VERSION_PATCH 0
#define WUFFS_VERSION_PRE_RELEASE_LABEL "unsupported.snapshot"
#define WUFFS_VERSION_BUILD_METADATA_COMMIT_COUNT 0
#define WUFFS_VERSION_BUILD_METADATA_COMMIT_DATE 0
#define WUFFS_VERSION_STRING "0.0.0+0.00000000"

// ---------------- Private Implementation Macros Re-definition Check

// Users (those who #include the "wuffs-vM.N.c" file) should not define any
// WUFFS_PRIVATE_IMPL__ETC macros, only WUFFS_CONFIG__ETC macros (and
// WUFFS_IMPLEMENTATION). Mucking about with the private implementation macros
// is not supported and may break when upgrading to newer Wuffs versions.

#if defined(WUFFS_PRIVATE_IMPL__CPU_ARCH__ARM_CRC32) ||       \
    defined(WUFFS_PRIVATE_IMPL__CPU_ARCH__ARM_NEON) ||        \
    defined(WUFFS_PRIVATE_IMPL__CPU_ARCH__X86_64) ||          \
    defined(WUFFS_PRIVATE_IMPL__CPU_ARCH__X86_64_V2) ||       \
    defined(WUFFS_PRIVATE_IMPL__CPU_ARCH__X86_64_V3) ||       \
    defined(WUFFS_PRIVATE_IMPL__HPD__DECIMAL_POINT__RANGE) || \
    defined(WUFFS_PRIVATE_IMPL__HPD__DIGITS_PRECISION) ||     \
    defined(WUFFS_PRIVATE_IMPL__HPD__SHIFT__MAX_INCL) ||      \
    defined(WUFFS_PRIVATE_IMPL__LOW_BITS_MASK__U16) ||        \
    defined(WUFFS_PRIVATE_IMPL__LOW_BITS_MASK__U32) ||        \
    defined(WUFFS_PRIVATE_IMPL__LOW_BITS_MASK__U64) ||        \
    defined(WUFFS_PRIVATE_IMPL__LOW_BITS_MASK__U8)

#if defined(__GNUC__) || defined(__clang__)
#warning "Defining WUFFS_PRIVATE_IMPL__ETC yourself is not supported"
#elif defined(_MSC_VER)
#pragma message("Defining WUFFS_PRIVATE_IMPL__ETC yourself is not supported")
#endif

#endif

// ---------------- Obsolete Macros Check

// std/tga was renamed to std/targa in September 2024 (Wuffs v0.4 alpha).
#if defined(WUFFS_CONFIG__MODULE__TGA)

#if defined(__GNUC__) || defined(__clang__)
#warning "WUFFS_CONFIG__MODULE__TGA was renamed to WUFFS_CONFIG__MODULE__TARGA"
#elif defined(_MSC_VER)
#pragma message( \
    "WUFFS_CONFIG__MODULE__TGA was renamed to WUFFS_CONFIG__MODULE__TARGA")
#endif

#endif

// ---------------- Configuration

// Define WUFFS_CONFIG__AVOID_CPU_ARCH to avoid any code tied to a specific CPU
// architecture, such as SSE SIMD for the x86 CPU family.
#if defined(WUFFS_CONFIG__AVOID_CPU_ARCH)  // (#if-chain ref AVOID_CPU_ARCH_0)
// No-op.
#else  // (#if-chain ref AVOID_CPU_ARCH_0)

// The "defined(__clang__)" isn't redundant. While vanilla clang defines
// __GNUC__, clang-cl (which mimics MSVC's cl.exe) does not.
#if defined(__GNUC__) || defined(__clang__)
#define WUFFS_BASE__MAYBE_ATTRIBUTE_TARGET(arg) __attribute__((target(arg)))
#else
#define WUFFS_BASE__MAYBE_ATTRIBUTE_TARGET(arg)
#endif  // defined(__GNUC__) || defined(__clang__)

#if defined(__GNUC__)  // (#if-chain ref AVOID_CPU_ARCH_1)

// To simplify Wuffs code, "cpu_arch >= arm_xxx" requires xxx but also
// unaligned little-endian load/stores.
#if defined(__ARM_FEATURE_UNALIGNED) && !defined(__native_client__) && \
    defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
// Not all gcc versions define __ARM_ACLE, even if they support crc32
// intrinsics. Look for __ARM_FEATURE_CRC32 instead.
#if defined(__ARM_FEATURE_CRC32)
#include <arm_acle.h>
#define WUFFS_PRIVATE_IMPL__CPU_ARCH__ARM_CRC32
#endif  // defined(__ARM_FEATURE_CRC32)
#if defined(__ARM_NEON)
#include <arm_neon.h>
#define WUFFS_PRIVATE_IMPL__CPU_ARCH__ARM_NEON
#endif  // defined(__ARM_NEON)
#endif  // defined(__ARM_FEATURE_UNALIGNED) etc

// Similarly, "cpu_arch >= x86_sse42" requires SSE4.2 but also PCLMUL and
// POPCNT. This is checked at runtime via cpuid, not at compile time.
//
// Likewise, "cpu_arch >= x86_avx2" also requires PCLMUL, POPCNT and SSE4.2.
//
// ----
//
// Technically, we could use the SSE family on 32-bit x86, not just 64-bit x86.
// But some intrinsics don't compile in 32-bit mode. It's not worth the hassle.
// https://github.com/google/wuffs/issues/145
#if defined(__x86_64__)
#if !defined(__native_client__)
#include <cpuid.h>
#include <x86intrin.h>
#define WUFFS_PRIVATE_IMPL__CPU_ARCH__X86_64
#define WUFFS_PRIVATE_IMPL__CPU_ARCH__X86_64_V2
#define WUFFS_PRIVATE_IMPL__CPU_ARCH__X86_64_V3
#endif  // !defined(__native_client__)
#endif  // defined(__x86_64__)

#elif defined(_MSC_VER)  // (#if-chain ref AVOID_CPU_ARCH_1)

#if defined(_M_X64)

// On X86_64, Microsoft Visual C/C++ (MSVC) only supports SSE2 by default.
// There are /arch:SSE2, /arch:AVX and /arch:AVX2 compiler flags (the AVX2 one
// is roughly equivalent to X86_64_V3), but there is no /arch:SSE42 compiler
// flag that's equivalent to X86_64_V2.
//
// For getting maximum performance with X86_64 MSVC and Wuffs, pass /arch:AVX2
// (and then test on the oldest hardware you intend to support).
//
// Absent that compiler flag, either define one of the three macros listed
// below or else the X86_64 SIMD code will be disabled and you'll get a #pragma
// message stating this library "performs best with /arch:AVX2". This message
// is harmless and ignorable, in that the non-SIMD code is still correct and
// reasonably performant, but is a reminder that when combining Wuffs and MSVC,
// some compiler configuration is required for maximum performance.
//
//  - WUFFS_CONFIG__DISABLE_MSVC_CPU_ARCH__X86_64_FAMILY
//  - WUFFS_CONFIG__ENABLE_MSVC_CPU_ARCH__X86_64_V2 (enables SSE4.2 and below)
//  - WUFFS_CONFIG__ENABLE_MSVC_CPU_ARCH__X86_64_V3 (enables AVX2 and below)
//
// Defining the first one (WUFFS_CONFIG__DISABLE_MSVC_CPU_ARCH__X86_64_FAMILY)
// or defining none of those three (the default state) are equivalent (in that
// both disable the SIMD code paths), other than that pragma message.
//
// When defining these WUFFS_CONFIG__ENABLE_ETC macros with MSVC, be aware that
// some users report it leading to ICEs (Internal Compiler Errors), but other
// users report no problems at all (and improved performance). It's unclear
// exactly what combination of SIMD code and MSVC configuration lead to ICEs.
// Do your own testing with your own MSVC version and configuration.
//
// https://github.com/google/wuffs/issues/148
// https://github.com/google/wuffs/issues/151
// https://developercommunity.visualstudio.com/t/fatal--error-C1001:-Internal-compiler-er/10703305
//
// Clang (including clang-cl) and GCC don't need this WUFFS_CONFIG__ETC macro
// machinery, or having the Wuffs-the-library user to fiddle with compiler
// flags, because they support "__attribute__((target(arg)))".
#if defined(__AVX2__) || defined(__clang__) || \
    defined(WUFFS_CONFIG__ENABLE_MSVC_CPU_ARCH__X86_64_V3)
#define WUFFS_PRIVATE_IMPL__CPU_ARCH__X86_64
#define WUFFS_PRIVATE_IMPL__CPU_ARCH__X86_64_V2
#define WUFFS_PRIVATE_IMPL__CPU_ARCH__X86_64_V3
#elif defined(WUFFS_CONFIG__ENABLE_MSVC_CPU_ARCH__X86_64_V2)
#define WUFFS_PRIVATE_IMPL__CPU_ARCH__X86_64
#define WUFFS_PRIVATE_IMPL__CPU_ARCH__X86_64_V2
#elif !defined(WUFFS_CONFIG__DISABLE_MSVC_CPU_ARCH__X86_64_FAMILY)
#pragma message("Wuffs with MSVC+X64 performs best with /arch:AVX2")
#endif  // defined(__AVX2__) || defined(__clang__) || etc

#if defined(WUFFS_PRIVATE_IMPL__CPU_ARCH__X86_64)

#if defined(WUFFS_CONFIG__DISABLE_MSVC_CPU_ARCH__X86_64_FAMILY)
#error "MSVC_CPU_ARCH simultaneously enabled and disabled"
#endif

#include <intrin.h>
// intrin.h isn't enough for X64 SIMD, with clang-cl, if we want to use
// "__attribute__((target(arg)))" without e.g. "/arch:AVX".
//
// Some web pages suggest that <immintrin.h> is all you need, as it pulls in
// the earlier SIMD families like SSE4.2, but that doesn't seem to work in
// practice, possibly for the same reason that just <intrin.h> doesn't work.
#include <immintrin.h>  // AVX, AVX2, FMA, POPCNT
#include <nmmintrin.h>  // SSE4.2
#include <wmmintrin.h>  // AES, PCLMUL

#endif  // defined(WUFFS_PRIVATE_IMPL__CPU_ARCH__X86_64)
#endif  // defined(_M_X64)
#endif  // (#if-chain ref AVOID_CPU_ARCH_1)
#endif  // (#if-chain ref AVOID_CPU_ARCH_0)

// --------

// Define WUFFS_CONFIG__STATIC_FUNCTIONS (combined with WUFFS_IMPLEMENTATION)
// to make all of Wuffs' functions have static storage.
//
// This can help the compiler ignore or discard unused code, which can produce
// faster compiles and smaller binaries. Other motivations are discussed in the
// "ALLOW STATIC IMPLEMENTATION" section of
// https://raw.githubusercontent.com/nothings/stb/master/docs/stb_howto.txt
#if defined(WUFFS_CONFIG__STATIC_FUNCTIONS)
#define WUFFS_BASE__MAYBE_STATIC static
#else
#define WUFFS_BASE__MAYBE_STATIC
#endif  // defined(WUFFS_CONFIG__STATIC_FUNCTIONS)

// ---------------- CPU Architecture

static inline bool  //
wuffs_base__cpu_arch__have_arm_crc32(void) {
#if defined(WUFFS_PRIVATE_IMPL__CPU_ARCH__ARM_CRC32)
  return true;
#else
  return false;
#endif  // defined(WUFFS_PRIVATE_IMPL__CPU_ARCH__ARM_CRC32)
}

static inline bool  //
wuffs_base__cpu_arch__have_arm_neon(void) {
#if defined(WUFFS_PRIVATE_IMPL__CPU_ARCH__ARM_NEON)
  return true;
#else
  return false;
#endif  // defined(WUFFS_PRIVATE_IMPL__CPU_ARCH__ARM_NEON)
}

static inline bool  //
wuffs_base__cpu_arch__have_x86_avx2(void) {
#if defined(__PCLMUL__) && defined(__POPCNT__) && defined(__SSE4_2__) && \
    defined(__AVX2__)
  return true;
#else
#if defined(WUFFS_PRIVATE_IMPL__CPU_ARCH__X86_64)
  // GCC defines these macros but MSVC does not.
  //  - bit_AVX2 = (1 <<  5)
  const unsigned int avx2_ebx7 = 0x00000020;
  // GCC defines these macros but MSVC does not.
  //  - bit_PCLMUL = (1 <<  1)
  //  - bit_POPCNT = (1 << 23)
  //  - bit_SSE4_2 = (1 << 20)
  const unsigned int avx2_ecx1 = 0x00900002;

  // clang defines __GNUC__ and clang-cl defines _MSC_VER (but not __GNUC__).
#if defined(__GNUC__)
  unsigned int eax7 = 0;
  unsigned int ebx7 = 0;
  unsigned int ecx7 = 0;
  unsigned int edx7 = 0;
  if (__get_cpuid_count(7, 0, &eax7, &ebx7, &ecx7, &edx7) &&
      ((ebx7 & avx2_ebx7) == avx2_ebx7)) {
    unsigned int eax1 = 0;
    unsigned int ebx1 = 0;
    unsigned int ecx1 = 0;
    unsigned int edx1 = 0;
    if (__get_cpuid(1, &eax1, &ebx1, &ecx1, &edx1) &&
        ((ecx1 & avx2_ecx1) == avx2_ecx1)) {
      return true;
    }
  }
#elif defined(_MSC_VER)  // defined(__GNUC__)
  int x7[4];
  __cpuidex(x7, 7, 0);
  if ((((unsigned int)(x7[1])) & avx2_ebx7) == avx2_ebx7) {
    int x1[4];
    __cpuid(x1, 1);
    if ((((unsigned int)(x1[2])) & avx2_ecx1) == avx2_ecx1) {
      return true;
    }
  }
#else
#error "WUFFS_PRIVATE_IMPL__CPU_ARCH__ETC combined with an unsupported compiler"
#endif  // defined(__GNUC__); defined(_MSC_VER)
#endif  // defined(WUFFS_PRIVATE_IMPL__CPU_ARCH__X86_64)
  return false;
#endif  // defined(__PCLMUL__) && defined(__POPCNT__) && defined(__SSE4_2__) &&
        // defined(__AVX2__)
}

static inline bool  //
wuffs_base__cpu_arch__have_x86_bmi2(void) {
#if defined(__BMI2__)
  return true;
#else
#if defined(WUFFS_PRIVATE_IMPL__CPU_ARCH__X86_64)
  // GCC defines these macros but MSVC does not.
  //  - bit_BMI2 = (1 <<  8)
  const unsigned int bmi2_ebx7 = 0x00000100;

  // clang defines __GNUC__ and clang-cl defines _MSC_VER (but not __GNUC__).
#if defined(__GNUC__)
  unsigned int eax7 = 0;
  unsigned int ebx7 = 0;
  unsigned int ecx7 = 0;
  unsigned int edx7 = 0;
  if (__get_cpuid_count(7, 0, &eax7, &ebx7, &ecx7, &edx7) &&
      ((ebx7 & bmi2_ebx7) == bmi2_ebx7)) {
    return true;
  }
#elif defined(_MSC_VER)  // defined(__GNUC__)
  int x7[4];
  __cpuidex(x7, 7, 0);
  if ((((unsigned int)(x7[1])) & bmi2_ebx7) == bmi2_ebx7) {
    return true;
  }
#else
#error "WUFFS_PRIVATE_IMPL__CPU_ARCH__ETC combined with an unsupported compiler"
#endif  // defined(__GNUC__); defined(_MSC_VER)
#endif  // defined(WUFFS_PRIVATE_IMPL__CPU_ARCH__X86_64)
  return false;
#endif  // defined(__BMI2__)
}

static inline bool  //
wuffs_base__cpu_arch__have_x86_sse42(void) {
#if defined(__PCLMUL__) && defined(__POPCNT__) && defined(__SSE4_2__)
  return true;
#else
#if defined(WUFFS_PRIVATE_IMPL__CPU_ARCH__X86_64)
  // GCC defines these macros but MSVC does not.
  //  - bit_PCLMUL = (1 <<  1)
  //  - bit_POPCNT = (1 << 23)
  //  - bit_SSE4_2 = (1 << 20)
  const unsigned int sse42_ecx1 = 0x00900002;

  // clang defines __GNUC__ and clang-cl defines _MSC_VER (but not __GNUC__).
#if defined(__GNUC__)
  unsigned int eax1 = 0;
  unsigned int ebx1 = 0;
  unsigned int ecx1 = 0;
  unsigned int edx1 = 0;
  if (__get_cpuid(1, &eax1, &ebx1, &ecx1, &edx1) &&
      ((ecx1 & sse42_ecx1) == sse42_ecx1)) {
    return true;
  }
#elif defined(_MSC_VER)  // defined(__GNUC__)
  int x1[4];
  __cpuid(x1, 1);
  if ((((unsigned int)(x1[2])) & sse42_ecx1) == sse42_ecx1) {
    return true;
  }
#else
#error "WUFFS_PRIVATE_IMPL__CPU_ARCH__ETC combined with an unsupported compiler"
#endif  // defined(__GNUC__); defined(_MSC_VER)
#endif  // defined(WUFFS_PRIVATE_IMPL__CPU_ARCH__X86_64)
  return false;
#endif  // defined(__PCLMUL__) && defined(__POPCNT__) && defined(__SSE4_2__)
}

// ---------------- Fundamentals

// Wuffs assumes that:
//  - converting a uint32_t to a size_t will never overflow.
//  - converting a size_t to a uint64_t will never overflow.
#if defined(__WORDSIZE)
#if (__WORDSIZE != 32) && (__WORDSIZE != 64)
#error "Wuffs requires a word size of either 32 or 64 bits"
#endif
#endif

// The "defined(__clang__)" isn't redundant. While vanilla clang defines
// __GNUC__, clang-cl (which mimics MSVC's cl.exe) does not.
#if defined(__GNUC__) || defined(__clang__)
#define WUFFS_BASE__FORCE_INLINE __attribute__((__always_inline__))
#define WUFFS_BASE__POTENTIALLY_UNUSED __attribute__((unused))
#define WUFFS_BASE__WARN_UNUSED_RESULT __attribute__((warn_unused_result))
#elif defined(_MSC_VER)
#define WUFFS_BASE__FORCE_INLINE __forceinline
#define WUFFS_BASE__POTENTIALLY_UNUSED
#define WUFFS_BASE__WARN_UNUSED_RESULT
#else
#define WUFFS_BASE__FORCE_INLINE
#define WUFFS_BASE__POTENTIALLY_UNUSED
#define WUFFS_BASE__WARN_UNUSED_RESULT
#endif

// Clang's "-fsanitize=integer" checks for "unsigned-integer-overflow" even
// though, for *unsigned* integers, it is *not* undefined behavior. The check
// is still made because unsigned integer overflow "is often unintentional".
// https://clang.llvm.org/docs/UndefinedBehaviorSanitizer.html
//
// However, for Wuffs' generated C code, unsigned overflow is intentional. The
// programmer has to use "~mod+" instead of a plain "+" operator in Wuffs code.
// Further runtime checks for unsigned integer overflow can add performance
// overhead (fuzzers can then time out) and can raise false negatives, without
// generating much benefits. We disable the "unsigned-integer-overflow" check.
#if defined(__has_feature)
#if __has_feature(undefined_behavior_sanitizer)
#define WUFFS_BASE__GENERATED_C_CODE \
  __attribute__((no_sanitize("unsigned-integer-overflow")))
#endif
#endif
#if !defined(WUFFS_BASE__GENERATED_C_CODE)
#define WUFFS_BASE__GENERATED_C_CODE
#endif

// --------

// Options (bitwise or'ed together) for wuffs_foo__bar__initialize functions.

#define WUFFS_INITIALIZE__DEFAULT_OPTIONS ((uint32_t)0x00000000)

// WUFFS_INITIALIZE__ALREADY_ZEROED means that the "self" receiver struct value
// has already been set to all zeroes.
#define WUFFS_INITIALIZE__ALREADY_ZEROED ((uint32_t)0x00000001)

// WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED means that, absent
// WUFFS_INITIALIZE__ALREADY_ZEROED, only some of the "self" receiver struct
// value will be set to all zeroes. Internal buffers, which tend to be a large
// proportion of the struct's size, will be left uninitialized. Internal means
// that the buffer is contained by the receiver struct, as opposed to being
// passed as a separately allocated "work buffer".
//
// For more detail, see:
// https://github.com/google/wuffs/blob/main/doc/note/initialization.md
#define WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED \
  ((uint32_t)0x00000002)

// --------

#ifdef __cplusplus
// Wuffs structs are just data, not resources (in the RAII sense). They don't
// subclass anything. They don't have virtual destructors. They don't contain
// pointers to dynamically allocated memory. They don't contain file
// descriptors. And so on. Destroying such a struct (e.g. via a
// wuffs_foo__bar::unique_ptr) can just call free, especially as
// sizeof(wuffs_foo__bar) isn't supposed to be part of the public (stable) API.
struct wuffs_unique_ptr_deleter {
  void operator()(void* p) { free(p); }
};
#endif  // __cplusplus

// --------

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"
#endif

static inline uint8_t*  //
wuffs_base__strip_const_from_u8_ptr(const uint8_t* ptr) {
  return (uint8_t*)ptr;
}

static inline uint16_t*  //
wuffs_base__strip_const_from_u16_ptr(const uint16_t* ptr) {
  return (uint16_t*)ptr;
}

static inline uint32_t*  //
wuffs_base__strip_const_from_u32_ptr(const uint32_t* ptr) {
  return (uint32_t*)ptr;
}

static inline uint64_t*  //
wuffs_base__strip_const_from_u64_ptr(const uint64_t* ptr) {
  return (uint64_t*)ptr;
}

#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

// --------

// wuffs_base__empty_struct is used when a Wuffs function returns an empty
// struct. In C, if a function f returns void, you can't say "x = f()", but in
// Wuffs, if a function g returns empty, you can say "y = g()".
typedef struct wuffs_base__empty_struct__struct {
  // private_impl is a placeholder field. It isn't explicitly used, except that
  // without it, the sizeof a struct with no fields can differ across C/C++
  // compilers, and it is undefined behavior in C99. For example, gcc says that
  // the sizeof an empty struct is 0, and g++ says that it is 1. This leads to
  // ABI incompatibility if a Wuffs .c file is processed by one compiler and
  // its .h file with another compiler.
  //
  // Instead, we explicitly insert an otherwise unused field, so that the
  // sizeof this struct is always 1.
  uint8_t private_impl;
} wuffs_base__empty_struct;

static inline wuffs_base__empty_struct  //
wuffs_base__make_empty_struct(void) {
  wuffs_base__empty_struct ret;
  ret.private_impl = 0;
  return ret;
}

// wuffs_base__utility is a placeholder receiver type. It enables what Java
// calls static methods, as opposed to regular methods.
typedef struct wuffs_base__utility__struct {
  // private_impl is a placeholder field. It isn't explicitly used, except that
  // without it, the sizeof a struct with no fields can differ across C/C++
  // compilers, and it is undefined behavior in C99. For example, gcc says that
  // the sizeof an empty struct is 0, and g++ says that it is 1. This leads to
  // ABI incompatibility if a Wuffs .c file is processed by one compiler and
  // its .h file with another compiler.
  //
  // Instead, we explicitly insert an otherwise unused field, so that the
  // sizeof this struct is always 1.
  uint8_t private_impl;
} wuffs_base__utility;

typedef struct wuffs_base__vtable__struct {
  const char* vtable_name;
  const void* function_pointers;
} wuffs_base__vtable;

// --------

// See https://github.com/google/wuffs/blob/main/doc/note/statuses.md
typedef struct wuffs_base__status__struct {
  const char* repr;

#ifdef __cplusplus
  inline bool is_complete() const;
  inline bool is_error() const;
  inline bool is_note() const;
  inline bool is_ok() const;
  inline bool is_suspension() const;
  inline bool is_truncated_input_error() const;
  inline const char* message() const;
#endif  // __cplusplus

} wuffs_base__status;

// ¡ INSERT wuffs_base__status names.

static inline wuffs_base__status  //
wuffs_base__make_status(const char* repr) {
  wuffs_base__status z;
  z.repr = repr;
  return z;
}

static inline bool  //
wuffs_base__status__is_complete(const wuffs_base__status* z) {
  return (z->repr == NULL) || ((*z->repr != '$') && (*z->repr != '#'));
}

static inline bool  //
wuffs_base__status__is_error(const wuffs_base__status* z) {
  return z->repr && (*z->repr == '#');
}

static inline bool  //
wuffs_base__status__is_note(const wuffs_base__status* z) {
  return z->repr && (*z->repr != '$') && (*z->repr != '#');
}

static inline bool  //
wuffs_base__status__is_ok(const wuffs_base__status* z) {
  return z->repr == NULL;
}

static inline bool  //
wuffs_base__status__is_suspension(const wuffs_base__status* z) {
  return z->repr && (*z->repr == '$');
}

static inline bool  //
wuffs_base__status__is_truncated_input_error(const wuffs_base__status* z) {
  const char* p = z->repr;
  if (!p || (*p != '#')) {
    return false;
  }
  p++;
  while (1) {
    if (*p == 0) {
      return false;
    } else if (*p++ == ':') {
      break;
    }
  }
  return strcmp(p, " truncated input") == 0;
}

// wuffs_base__status__message strips the leading '$', '#' or '@'.
static inline const char*  //
wuffs_base__status__message(const wuffs_base__status* z) {
  if (z->repr) {
    if ((*z->repr == '$') || (*z->repr == '#') || (*z->repr == '@')) {
      return z->repr + 1;
    }
  }
  return z->repr;
}

#ifdef __cplusplus

inline bool  //
wuffs_base__status::is_complete() const {
  return wuffs_base__status__is_complete(this);
}

inline bool  //
wuffs_base__status::is_error() const {
  return wuffs_base__status__is_error(this);
}

inline bool  //
wuffs_base__status::is_note() const {
  return wuffs_base__status__is_note(this);
}

inline bool  //
wuffs_base__status::is_ok() const {
  return wuffs_base__status__is_ok(this);
}

inline bool  //
wuffs_base__status::is_suspension() const {
  return wuffs_base__status__is_suspension(this);
}

inline bool  //
wuffs_base__status::is_truncated_input_error() const {
  return wuffs_base__status__is_truncated_input_error(this);
}

inline const char*  //
wuffs_base__status::message() const {
  return wuffs_base__status__message(this);
}

#endif  // __cplusplus

// --------

// WUFFS_BASE__RESULT is a result type: either a status (an error) or a value.
//
// A result with all fields NULL or zero is as valid as a zero-valued T.
#define WUFFS_BASE__RESULT(T)  \
  struct {                     \
    wuffs_base__status status; \
    T value;                   \
  }

typedef WUFFS_BASE__RESULT(double) wuffs_base__result_f64;
typedef WUFFS_BASE__RESULT(int64_t) wuffs_base__result_i64;
typedef WUFFS_BASE__RESULT(uint64_t) wuffs_base__result_u64;

// --------

// wuffs_base__transform__output is the result of transforming from a src slice
// to a dst slice.
typedef struct wuffs_base__transform__output__struct {
  wuffs_base__status status;
  size_t num_dst;
  size_t num_src;
} wuffs_base__transform__output;

// --------

// FourCC constants. Four Character Codes are literally four ASCII characters
// (sometimes padded with ' ' spaces) that pack neatly into a signed or
// unsigned 32-bit integer. ASCII letters are conventionally upper case.
//
// They are often used to identify video codecs (e.g. "H265") and pixel formats
// (e.g. "YV12"). Wuffs uses them for that but also generally for naming
// various things: compression formats (e.g. "BZ2 "), image metadata (e.g.
// "EXIF"), file formats (e.g. "HTML"), etc.
//
// Wuffs' u32 values are big-endian ("JPEG" is 0x4A504547 not 0x4745504A) to
// preserve ordering: "JPEG" < "MP3 " and 0x4A504547 < 0x4D503320.

// ¡ INSERT FourCCs.

// --------

// Quirks.

// ¡ INSERT Quirks.

// --------

// Flicks are a unit of time. One flick (frame-tick) is 1 / 705_600_000 of a
// second. See https://github.com/OculusVR/Flicks
typedef int64_t wuffs_base__flicks;

#define WUFFS_BASE__FLICKS_PER_SECOND ((uint64_t)705600000)
#define WUFFS_BASE__FLICKS_PER_MILLISECOND ((uint64_t)705600)

// ---------------- Numeric Types

// The helpers below are functions, instead of macros, because their arguments
// can be an expression that we shouldn't evaluate more than once.
//
// They are static, so that linking multiple wuffs .o files won't complain about
// duplicate function definitions.
//
// They are explicitly marked inline, even if modern compilers don't use the
// inline attribute to guide optimizations such as inlining, to avoid the
// -Wunused-function warning, and we like to compile with -Wall -Werror.

static inline int8_t  //
wuffs_base__i8__min(int8_t x, int8_t y) {
  return x < y ? x : y;
}

static inline int8_t  //
wuffs_base__i8__max(int8_t x, int8_t y) {
  return x > y ? x : y;
}

static inline int16_t  //
wuffs_base__i16__min(int16_t x, int16_t y) {
  return x < y ? x : y;
}

static inline int16_t  //
wuffs_base__i16__max(int16_t x, int16_t y) {
  return x > y ? x : y;
}

static inline int32_t  //
wuffs_base__i32__min(int32_t x, int32_t y) {
  return x < y ? x : y;
}

static inline int32_t  //
wuffs_base__i32__max(int32_t x, int32_t y) {
  return x > y ? x : y;
}

static inline int64_t  //
wuffs_base__i64__min(int64_t x, int64_t y) {
  return x < y ? x : y;
}

static inline int64_t  //
wuffs_base__i64__max(int64_t x, int64_t y) {
  return x > y ? x : y;
}

static inline uint8_t  //
wuffs_base__u8__min(uint8_t x, uint8_t y) {
  return x < y ? x : y;
}

static inline uint8_t  //
wuffs_base__u8__max(uint8_t x, uint8_t y) {
  return x > y ? x : y;
}

static inline uint16_t  //
wuffs_base__u16__min(uint16_t x, uint16_t y) {
  return x < y ? x : y;
}

static inline uint16_t  //
wuffs_base__u16__max(uint16_t x, uint16_t y) {
  return x > y ? x : y;
}

static inline uint32_t  //
wuffs_base__u32__min(uint32_t x, uint32_t y) {
  return x < y ? x : y;
}

static inline uint32_t  //
wuffs_base__u32__max(uint32_t x, uint32_t y) {
  return x > y ? x : y;
}

static inline uint64_t  //
wuffs_base__u64__min(uint64_t x, uint64_t y) {
  return x < y ? x : y;
}

static inline uint64_t  //
wuffs_base__u64__max(uint64_t x, uint64_t y) {
  return x > y ? x : y;
}

// --------

static inline uint8_t  //
wuffs_base__u8__rotate_left(uint8_t x, uint32_t n) {
  n &= 7;
  return ((uint8_t)(x << n)) | ((uint8_t)(x >> (8 - n)));
}

static inline uint8_t  //
wuffs_base__u8__rotate_right(uint8_t x, uint32_t n) {
  n &= 7;
  return ((uint8_t)(x >> n)) | ((uint8_t)(x << (8 - n)));
}

static inline uint16_t  //
wuffs_base__u16__rotate_left(uint16_t x, uint32_t n) {
  n &= 15;
  return ((uint16_t)(x << n)) | ((uint16_t)(x >> (16 - n)));
}

static inline uint16_t  //
wuffs_base__u16__rotate_right(uint16_t x, uint32_t n) {
  n &= 15;
  return ((uint16_t)(x >> n)) | ((uint16_t)(x << (16 - n)));
}

static inline uint32_t  //
wuffs_base__u32__rotate_left(uint32_t x, uint32_t n) {
  n &= 31;
  return ((uint32_t)(x << n)) | ((uint32_t)(x >> (32 - n)));
}

static inline uint32_t  //
wuffs_base__u32__rotate_right(uint32_t x, uint32_t n) {
  n &= 31;
  return ((uint32_t)(x >> n)) | ((uint32_t)(x << (32 - n)));
}

static inline uint64_t  //
wuffs_base__u64__rotate_left(uint64_t x, uint32_t n) {
  n &= 63;
  return ((uint64_t)(x << n)) | ((uint64_t)(x >> (64 - n)));
}

static inline uint64_t  //
wuffs_base__u64__rotate_right(uint64_t x, uint32_t n) {
  n &= 63;
  return ((uint64_t)(x >> n)) | ((uint64_t)(x << (64 - n)));
}

// --------

// Saturating arithmetic (sat_add, sat_sub) branchless bit-twiddling algorithms
// are per https://locklessinc.com/articles/sat_arithmetic/
//
// It is important that the underlying types are unsigned integers, as signed
// integer arithmetic overflow is undefined behavior in C.

static inline uint8_t  //
wuffs_base__u8__sat_add(uint8_t x, uint8_t y) {
  uint8_t res = (uint8_t)(x + y);
  res |= (uint8_t)(-(res < x));
  return res;
}

static inline uint8_t  //
wuffs_base__u8__sat_sub(uint8_t x, uint8_t y) {
  uint8_t res = (uint8_t)(x - y);
  res &= (uint8_t)(-(res <= x));
  return res;
}

static inline uint16_t  //
wuffs_base__u16__sat_add(uint16_t x, uint16_t y) {
  uint16_t res = (uint16_t)(x + y);
  res |= (uint16_t)(-(res < x));
  return res;
}

static inline uint16_t  //
wuffs_base__u16__sat_sub(uint16_t x, uint16_t y) {
  uint16_t res = (uint16_t)(x - y);
  res &= (uint16_t)(-(res <= x));
  return res;
}

static inline uint32_t  //
wuffs_base__u32__sat_add(uint32_t x, uint32_t y) {
  uint32_t res = (uint32_t)(x + y);
  res |= (uint32_t)(-(res < x));
  return res;
}

static inline uint32_t  //
wuffs_base__u32__sat_sub(uint32_t x, uint32_t y) {
  uint32_t res = (uint32_t)(x - y);
  res &= (uint32_t)(-(res <= x));
  return res;
}

static inline uint64_t  //
wuffs_base__u64__sat_add(uint64_t x, uint64_t y) {
  uint64_t res = (uint64_t)(x + y);
  res |= (uint64_t)(-(res < x));
  return res;
}

static inline uint64_t  //
wuffs_base__u64__sat_sub(uint64_t x, uint64_t y) {
  uint64_t res = (uint64_t)(x - y);
  res &= (uint64_t)(-(res <= x));
  return res;
}

// --------

typedef struct wuffs_base__multiply_u64__output__struct {
  uint64_t lo;
  uint64_t hi;
} wuffs_base__multiply_u64__output;

// wuffs_base__multiply_u64 returns x*y as a 128-bit value.
//
// The maximum inclusive output hi_lo is 0xFFFFFFFFFFFFFFFE_0000000000000001.
static inline wuffs_base__multiply_u64__output  //
wuffs_base__multiply_u64(uint64_t x, uint64_t y) {
#if defined(__SIZEOF_INT128__)
  __uint128_t z = ((__uint128_t)x) * ((__uint128_t)y);
  wuffs_base__multiply_u64__output o;
  o.lo = ((uint64_t)(z));
  o.hi = ((uint64_t)(z >> 64));
  return o;
#else
  // TODO: consider using the _mul128 intrinsic if defined(_MSC_VER).
  uint64_t x0 = x & 0xFFFFFFFF;
  uint64_t x1 = x >> 32;
  uint64_t y0 = y & 0xFFFFFFFF;
  uint64_t y1 = y >> 32;
  uint64_t w0 = x0 * y0;
  uint64_t t = (x1 * y0) + (w0 >> 32);
  uint64_t w1 = t & 0xFFFFFFFF;
  uint64_t w2 = t >> 32;
  w1 += x0 * y1;
  wuffs_base__multiply_u64__output o;
  o.lo = x * y;
  o.hi = (x1 * y1) + w2 + (w1 >> 32);
  return o;
#endif
}

// --------

typedef struct wuffs_base__bitvec256__struct {
  // elements_u64[0] holds the LSBs (least significant bits) and
  // elements_u64[3] holds the MSBs (most significant bits).
  uint64_t elements_u64[4];
} wuffs_base__bitvec256;

static inline wuffs_base__bitvec256  //
wuffs_base__make_bitvec256(uint64_t e00,
                           uint64_t e01,
                           uint64_t e02,
                           uint64_t e03) {
  wuffs_base__bitvec256 res;
  res.elements_u64[0] = e00;
  res.elements_u64[1] = e01;
  res.elements_u64[2] = e02;
  res.elements_u64[3] = e03;
  return res;
}

static inline uint64_t  //
wuffs_base__bitvec256__get_u64(const wuffs_base__bitvec256* b, uint32_t i) {
  return b->elements_u64[i & 3];
}

// --------

// wuffs_base__optional_u63 is like a std::optional<uint64_t>, but for C (not
// just C++) and the value can only hold 63 bits (not 64).
//
// Do not manipulate repr directly; it is a private implementation detail.
typedef struct wuffs_base__optional_u63__struct {
  uint64_t repr;

#ifdef __cplusplus
  inline bool has_value() const;
  inline uint64_t value() const;
  inline uint64_t value_or(uint64_t default_value) const;
#endif  // __cplusplus

} wuffs_base__optional_u63;

// wuffs_base__make_optional_u63 ignores value when has_value is false.
//
// Preconditions:
//  - value < (1 << 63).
static inline wuffs_base__optional_u63  //
wuffs_base__make_optional_u63(bool has_value, uint64_t value) {
  wuffs_base__optional_u63 res;
  res.repr = has_value ? ((value << 1u) | 1u) : 0u;
  return res;
}

static inline bool  //
wuffs_base__optional_u63__has_value(const wuffs_base__optional_u63* o) {
  return o->repr;
}

// wuffs_base__optional_u63__value returns zero when o does not have a value.
static inline uint64_t  //
wuffs_base__optional_u63__value(const wuffs_base__optional_u63* o) {
  return o->repr >> 1u;
}

static inline uint64_t  //
wuffs_base__optional_u63__value_or(const wuffs_base__optional_u63* o,
                                   uint64_t default_value) {
  return o->repr ? (o->repr >> 1u) : default_value;
}

#ifdef __cplusplus

inline bool  //
wuffs_base__optional_u63::has_value() const {
  return wuffs_base__optional_u63__has_value(this);
}

inline uint64_t  //
wuffs_base__optional_u63::value() const {
  return wuffs_base__optional_u63__value(this);
}

inline uint64_t  //
wuffs_base__optional_u63::value_or(uint64_t default_value) const {
  return wuffs_base__optional_u63__value_or(this, default_value);
}

#endif  // __cplusplus

// --------

// The "defined(__clang__)" isn't redundant. While vanilla clang defines
// __GNUC__, clang-cl (which mimics MSVC's cl.exe) does not.
#if (defined(__GNUC__) || defined(__clang__)) && (__SIZEOF_LONG__ == 8)

static inline uint32_t  //
wuffs_base__count_leading_zeroes_u64(uint64_t u) {
  return u ? ((uint32_t)(__builtin_clzl(u))) : 64u;
}

#else
// TODO: consider using the _BitScanReverse intrinsic if defined(_MSC_VER).

static inline uint32_t  //
wuffs_base__count_leading_zeroes_u64(uint64_t u) {
  if (u == 0) {
    return 64;
  }

  uint32_t n = 0;
  if ((u >> 32) == 0) {
    n |= 32;
    u <<= 32;
  }
  if ((u >> 48) == 0) {
    n |= 16;
    u <<= 16;
  }
  if ((u >> 56) == 0) {
    n |= 8;
    u <<= 8;
  }
  if ((u >> 60) == 0) {
    n |= 4;
    u <<= 4;
  }
  if ((u >> 62) == 0) {
    n |= 2;
    u <<= 2;
  }
  if ((u >> 63) == 0) {
    n |= 1;
    u <<= 1;
  }
  return n;
}

#endif  // (defined(__GNUC__) || defined(__clang__)) && (__SIZEOF_LONG__ == 8)

// --------

// Normally, the wuffs_base__peek_etc and wuffs_base__poke_etc implementations
// are both (1) correct regardless of CPU endianness and (2) very fast (e.g. an
// inlined wuffs_base__peek_u32le__no_bounds_check call, in an optimized clang
// or gcc build, is a single MOV instruction on x86_64).
//
// However, the endian-agnostic implementations are slow on Microsoft's C
// compiler (MSC). Alternative memcpy-based implementations restore speed, but
// they are only correct on little-endian CPU architectures. Defining
// WUFFS_BASE__USE_MEMCPY_LE_PEEK_POKE opts in to these implementations.
//
// https://godbolt.org/z/q4MfjzTPh
#if defined(_MSC_VER) && !defined(__clang__) && \
    (defined(_M_ARM64) || defined(_M_X64))
#define WUFFS_BASE__USE_MEMCPY_LE_PEEK_POKE
#endif

#define wuffs_base__peek_u8be__no_bounds_check \
  wuffs_base__peek_u8__no_bounds_check
#define wuffs_base__peek_u8le__no_bounds_check \
  wuffs_base__peek_u8__no_bounds_check

static inline uint8_t  //
wuffs_base__peek_u8__no_bounds_check(const uint8_t* p) {
  return p[0];
}

static inline uint16_t  //
wuffs_base__peek_u16be__no_bounds_check(const uint8_t* p) {
#if defined(WUFFS_BASE__USE_MEMCPY_LE_PEEK_POKE)
  uint16_t x;
  memcpy(&x, p, 2);
  return _byteswap_ushort(x);
#else
  return (uint16_t)(((uint16_t)(p[0]) << 8) | ((uint16_t)(p[1]) << 0));
#endif
}

static inline uint16_t  //
wuffs_base__peek_u16le__no_bounds_check(const uint8_t* p) {
#if defined(WUFFS_BASE__USE_MEMCPY_LE_PEEK_POKE)
  uint16_t x;
  memcpy(&x, p, 2);
  return x;
#else
  return (uint16_t)(((uint16_t)(p[0]) << 0) | ((uint16_t)(p[1]) << 8));
#endif
}

static inline uint32_t  //
wuffs_base__peek_u24be__no_bounds_check(const uint8_t* p) {
  return ((uint32_t)(p[0]) << 16) | ((uint32_t)(p[1]) << 8) |
         ((uint32_t)(p[2]) << 0);
}

static inline uint32_t  //
wuffs_base__peek_u24le__no_bounds_check(const uint8_t* p) {
  return ((uint32_t)(p[0]) << 0) | ((uint32_t)(p[1]) << 8) |
         ((uint32_t)(p[2]) << 16);
}

static inline uint32_t  //
wuffs_base__peek_u32be__no_bounds_check(const uint8_t* p) {
#if defined(WUFFS_BASE__USE_MEMCPY_LE_PEEK_POKE)
  uint32_t x;
  memcpy(&x, p, 4);
  return _byteswap_ulong(x);
#else
  return ((uint32_t)(p[0]) << 24) | ((uint32_t)(p[1]) << 16) |
         ((uint32_t)(p[2]) << 8) | ((uint32_t)(p[3]) << 0);
#endif
}

static inline uint32_t  //
wuffs_base__peek_u32le__no_bounds_check(const uint8_t* p) {
#if defined(WUFFS_BASE__USE_MEMCPY_LE_PEEK_POKE)
  uint32_t x;
  memcpy(&x, p, 4);
  return x;
#else
  return ((uint32_t)(p[0]) << 0) | ((uint32_t)(p[1]) << 8) |
         ((uint32_t)(p[2]) << 16) | ((uint32_t)(p[3]) << 24);
#endif
}

static inline uint64_t  //
wuffs_base__peek_u40be__no_bounds_check(const uint8_t* p) {
  return ((uint64_t)(p[0]) << 32) | ((uint64_t)(p[1]) << 24) |
         ((uint64_t)(p[2]) << 16) | ((uint64_t)(p[3]) << 8) |
         ((uint64_t)(p[4]) << 0);
}

static inline uint64_t  //
wuffs_base__peek_u40le__no_bounds_check(const uint8_t* p) {
  return ((uint64_t)(p[0]) << 0) | ((uint64_t)(p[1]) << 8) |
         ((uint64_t)(p[2]) << 16) | ((uint64_t)(p[3]) << 24) |
         ((uint64_t)(p[4]) << 32);
}

static inline uint64_t  //
wuffs_base__peek_u48be__no_bounds_check(const uint8_t* p) {
  return ((uint64_t)(p[0]) << 40) | ((uint64_t)(p[1]) << 32) |
         ((uint64_t)(p[2]) << 24) | ((uint64_t)(p[3]) << 16) |
         ((uint64_t)(p[4]) << 8) | ((uint64_t)(p[5]) << 0);
}

static inline uint64_t  //
wuffs_base__peek_u48le__no_bounds_check(const uint8_t* p) {
  return ((uint64_t)(p[0]) << 0) | ((uint64_t)(p[1]) << 8) |
         ((uint64_t)(p[2]) << 16) | ((uint64_t)(p[3]) << 24) |
         ((uint64_t)(p[4]) << 32) | ((uint64_t)(p[5]) << 40);
}

static inline uint64_t  //
wuffs_base__peek_u56be__no_bounds_check(const uint8_t* p) {
  return ((uint64_t)(p[0]) << 48) | ((uint64_t)(p[1]) << 40) |
         ((uint64_t)(p[2]) << 32) | ((uint64_t)(p[3]) << 24) |
         ((uint64_t)(p[4]) << 16) | ((uint64_t)(p[5]) << 8) |
         ((uint64_t)(p[6]) << 0);
}

static inline uint64_t  //
wuffs_base__peek_u56le__no_bounds_check(const uint8_t* p) {
  return ((uint64_t)(p[0]) << 0) | ((uint64_t)(p[1]) << 8) |
         ((uint64_t)(p[2]) << 16) | ((uint64_t)(p[3]) << 24) |
         ((uint64_t)(p[4]) << 32) | ((uint64_t)(p[5]) << 40) |
         ((uint64_t)(p[6]) << 48);
}

static inline uint64_t  //
wuffs_base__peek_u64be__no_bounds_check(const uint8_t* p) {
#if defined(WUFFS_BASE__USE_MEMCPY_LE_PEEK_POKE)
  uint64_t x;
  memcpy(&x, p, 8);
  return _byteswap_uint64(x);
#else
  return ((uint64_t)(p[0]) << 56) | ((uint64_t)(p[1]) << 48) |
         ((uint64_t)(p[2]) << 40) | ((uint64_t)(p[3]) << 32) |
         ((uint64_t)(p[4]) << 24) | ((uint64_t)(p[5]) << 16) |
         ((uint64_t)(p[6]) << 8) | ((uint64_t)(p[7]) << 0);
#endif
}

static inline uint64_t  //
wuffs_base__peek_u64le__no_bounds_check(const uint8_t* p) {
#if defined(WUFFS_BASE__USE_MEMCPY_LE_PEEK_POKE)
  uint64_t x;
  memcpy(&x, p, 8);
  return x;
#else
  return ((uint64_t)(p[0]) << 0) | ((uint64_t)(p[1]) << 8) |
         ((uint64_t)(p[2]) << 16) | ((uint64_t)(p[3]) << 24) |
         ((uint64_t)(p[4]) << 32) | ((uint64_t)(p[5]) << 40) |
         ((uint64_t)(p[6]) << 48) | ((uint64_t)(p[7]) << 56);
#endif
}

// --------

#define wuffs_base__poke_u8be__no_bounds_check \
  wuffs_base__poke_u8__no_bounds_check
#define wuffs_base__poke_u8le__no_bounds_check \
  wuffs_base__poke_u8__no_bounds_check

static inline void  //
wuffs_base__poke_u8__no_bounds_check(uint8_t* p, uint8_t x) {
  p[0] = x;
}

static inline void  //
wuffs_base__poke_u16be__no_bounds_check(uint8_t* p, uint16_t x) {
  p[0] = (uint8_t)(x >> 8);
  p[1] = (uint8_t)(x >> 0);
}

static inline void  //
wuffs_base__poke_u16le__no_bounds_check(uint8_t* p, uint16_t x) {
#if defined(WUFFS_BASE__USE_MEMCPY_LE_PEEK_POKE) || \
    (defined(__GNUC__) && !defined(__clang__) && defined(__x86_64__))
  // This seems to perform better on gcc 10 (but not clang 9). Clang also
  // defines "__GNUC__".
  memcpy(p, &x, 2);
#else
  p[0] = (uint8_t)(x >> 0);
  p[1] = (uint8_t)(x >> 8);
#endif
}

static inline void  //
wuffs_base__poke_u24be__no_bounds_check(uint8_t* p, uint32_t x) {
  p[0] = (uint8_t)(x >> 16);
  p[1] = (uint8_t)(x >> 8);
  p[2] = (uint8_t)(x >> 0);
}

static inline void  //
wuffs_base__poke_u24le__no_bounds_check(uint8_t* p, uint32_t x) {
  p[0] = (uint8_t)(x >> 0);
  p[1] = (uint8_t)(x >> 8);
  p[2] = (uint8_t)(x >> 16);
}

static inline void  //
wuffs_base__poke_u32be__no_bounds_check(uint8_t* p, uint32_t x) {
  p[0] = (uint8_t)(x >> 24);
  p[1] = (uint8_t)(x >> 16);
  p[2] = (uint8_t)(x >> 8);
  p[3] = (uint8_t)(x >> 0);
}

static inline void  //
wuffs_base__poke_u32le__no_bounds_check(uint8_t* p, uint32_t x) {
#if defined(WUFFS_BASE__USE_MEMCPY_LE_PEEK_POKE) || \
    (defined(__GNUC__) && !defined(__clang__) && defined(__x86_64__))
  // This seems to perform better on gcc 10 (but not clang 9). Clang also
  // defines "__GNUC__".
  memcpy(p, &x, 4);
#else
  p[0] = (uint8_t)(x >> 0);
  p[1] = (uint8_t)(x >> 8);
  p[2] = (uint8_t)(x >> 16);
  p[3] = (uint8_t)(x >> 24);
#endif
}

static inline void  //
wuffs_base__poke_u40be__no_bounds_check(uint8_t* p, uint64_t x) {
  p[0] = (uint8_t)(x >> 32);
  p[1] = (uint8_t)(x >> 24);
  p[2] = (uint8_t)(x >> 16);
  p[3] = (uint8_t)(x >> 8);
  p[4] = (uint8_t)(x >> 0);
}

static inline void  //
wuffs_base__poke_u40le__no_bounds_check(uint8_t* p, uint64_t x) {
  p[0] = (uint8_t)(x >> 0);
  p[1] = (uint8_t)(x >> 8);
  p[2] = (uint8_t)(x >> 16);
  p[3] = (uint8_t)(x >> 24);
  p[4] = (uint8_t)(x >> 32);
}

static inline void  //
wuffs_base__poke_u48be__no_bounds_check(uint8_t* p, uint64_t x) {
  p[0] = (uint8_t)(x >> 40);
  p[1] = (uint8_t)(x >> 32);
  p[2] = (uint8_t)(x >> 24);
  p[3] = (uint8_t)(x >> 16);
  p[4] = (uint8_t)(x >> 8);
  p[5] = (uint8_t)(x >> 0);
}

static inline void  //
wuffs_base__poke_u48le__no_bounds_check(uint8_t* p, uint64_t x) {
  p[0] = (uint8_t)(x >> 0);
  p[1] = (uint8_t)(x >> 8);
  p[2] = (uint8_t)(x >> 16);
  p[3] = (uint8_t)(x >> 24);
  p[4] = (uint8_t)(x >> 32);
  p[5] = (uint8_t)(x >> 40);
}

static inline void  //
wuffs_base__poke_u56be__no_bounds_check(uint8_t* p, uint64_t x) {
  p[0] = (uint8_t)(x >> 48);
  p[1] = (uint8_t)(x >> 40);
  p[2] = (uint8_t)(x >> 32);
  p[3] = (uint8_t)(x >> 24);
  p[4] = (uint8_t)(x >> 16);
  p[5] = (uint8_t)(x >> 8);
  p[6] = (uint8_t)(x >> 0);
}

static inline void  //
wuffs_base__poke_u56le__no_bounds_check(uint8_t* p, uint64_t x) {
  p[0] = (uint8_t)(x >> 0);
  p[1] = (uint8_t)(x >> 8);
  p[2] = (uint8_t)(x >> 16);
  p[3] = (uint8_t)(x >> 24);
  p[4] = (uint8_t)(x >> 32);
  p[5] = (uint8_t)(x >> 40);
  p[6] = (uint8_t)(x >> 48);
}

static inline void  //
wuffs_base__poke_u64be__no_bounds_check(uint8_t* p, uint64_t x) {
  p[0] = (uint8_t)(x >> 56);
  p[1] = (uint8_t)(x >> 48);
  p[2] = (uint8_t)(x >> 40);
  p[3] = (uint8_t)(x >> 32);
  p[4] = (uint8_t)(x >> 24);
  p[5] = (uint8_t)(x >> 16);
  p[6] = (uint8_t)(x >> 8);
  p[7] = (uint8_t)(x >> 0);
}

static inline void  //
wuffs_base__poke_u64le__no_bounds_check(uint8_t* p, uint64_t x) {
#if defined(WUFFS_BASE__USE_MEMCPY_LE_PEEK_POKE) || \
    (defined(__GNUC__) && !defined(__clang__) && defined(__x86_64__))
  // This seems to perform better on gcc 10 (but not clang 9). Clang also
  // defines "__GNUC__".
  memcpy(p, &x, 8);
#else
  p[0] = (uint8_t)(x >> 0);
  p[1] = (uint8_t)(x >> 8);
  p[2] = (uint8_t)(x >> 16);
  p[3] = (uint8_t)(x >> 24);
  p[4] = (uint8_t)(x >> 32);
  p[5] = (uint8_t)(x >> 40);
  p[6] = (uint8_t)(x >> 48);
  p[7] = (uint8_t)(x >> 56);
#endif
}

// ---------------- Slices and Tables

// WUFFS_BASE__SLICE is a 1-dimensional buffer.
//
// len measures a number of elements, not necessarily a size in bytes.
//
// A value with all fields NULL or zero is a valid, empty slice.
#define WUFFS_BASE__SLICE(T) \
  struct {                   \
    T* ptr;                  \
    size_t len;              \
  }

// WUFFS_BASE__TABLE is a 2-dimensional buffer.
//
// width, height and stride measure a number of elements, not necessarily a
// size in bytes.
//
// A value with all fields NULL or zero is a valid, empty table.
#define WUFFS_BASE__TABLE(T) \
  struct {                   \
    T* ptr;                  \
    size_t width;            \
    size_t height;           \
    size_t stride;           \
  }

typedef WUFFS_BASE__SLICE(uint8_t) wuffs_base__slice_u8;
typedef WUFFS_BASE__SLICE(uint16_t) wuffs_base__slice_u16;
typedef WUFFS_BASE__SLICE(uint32_t) wuffs_base__slice_u32;
typedef WUFFS_BASE__SLICE(uint64_t) wuffs_base__slice_u64;

typedef WUFFS_BASE__TABLE(uint8_t) wuffs_base__table_u8;
typedef WUFFS_BASE__TABLE(uint16_t) wuffs_base__table_u16;
typedef WUFFS_BASE__TABLE(uint32_t) wuffs_base__table_u32;
typedef WUFFS_BASE__TABLE(uint64_t) wuffs_base__table_u64;

static inline wuffs_base__slice_u8  //
wuffs_base__make_slice_u8(uint8_t* ptr, size_t len) {
  wuffs_base__slice_u8 ret;
  ret.ptr = ptr;
  ret.len = len;
  return ret;
}

static inline wuffs_base__slice_u16  //
wuffs_base__make_slice_u16(uint16_t* ptr, size_t len) {
  wuffs_base__slice_u16 ret;
  ret.ptr = ptr;
  ret.len = len;
  return ret;
}

static inline wuffs_base__slice_u32  //
wuffs_base__make_slice_u32(uint32_t* ptr, size_t len) {
  wuffs_base__slice_u32 ret;
  ret.ptr = ptr;
  ret.len = len;
  return ret;
}

static inline wuffs_base__slice_u64  //
wuffs_base__make_slice_u64(uint64_t* ptr, size_t len) {
  wuffs_base__slice_u64 ret;
  ret.ptr = ptr;
  ret.len = len;
  return ret;
}

static inline wuffs_base__slice_u8  //
wuffs_base__make_slice_u8_ij(uint8_t* ptr, size_t i, size_t j) {
  wuffs_base__slice_u8 ret;
  ret.ptr = ptr ? (ptr + i) : NULL;
  ret.len = (j >= i) ? (j - i) : 0;
  return ret;
}

static inline wuffs_base__slice_u16  //
wuffs_base__make_slice_u16_ij(uint16_t* ptr, size_t i, size_t j) {
  wuffs_base__slice_u16 ret;
  ret.ptr = ptr ? (ptr + i) : NULL;
  ret.len = (j >= i) ? (j - i) : 0;
  return ret;
}

static inline wuffs_base__slice_u32  //
wuffs_base__make_slice_u32_ij(uint32_t* ptr, size_t i, size_t j) {
  wuffs_base__slice_u32 ret;
  ret.ptr = ptr ? (ptr + i) : NULL;
  ret.len = (j >= i) ? (j - i) : 0;
  return ret;
}

static inline wuffs_base__slice_u64  //
wuffs_base__make_slice_u64_ij(uint64_t* ptr, size_t i, size_t j) {
  wuffs_base__slice_u64 ret;
  ret.ptr = ptr ? (ptr + i) : NULL;
  ret.len = (j >= i) ? (j - i) : 0;
  return ret;
}

static inline wuffs_base__slice_u8  //
wuffs_base__empty_slice_u8(void) {
  wuffs_base__slice_u8 ret;
  ret.ptr = NULL;
  ret.len = 0;
  return ret;
}

static inline wuffs_base__slice_u16  //
wuffs_base__empty_slice_u16(void) {
  wuffs_base__slice_u16 ret;
  ret.ptr = NULL;
  ret.len = 0;
  return ret;
}

static inline wuffs_base__slice_u32  //
wuffs_base__empty_slice_u32(void) {
  wuffs_base__slice_u32 ret;
  ret.ptr = NULL;
  ret.len = 0;
  return ret;
}

static inline wuffs_base__slice_u64  //
wuffs_base__empty_slice_u64(void) {
  wuffs_base__slice_u64 ret;
  ret.ptr = NULL;
  ret.len = 0;
  return ret;
}

static inline wuffs_base__table_u8  //
wuffs_base__make_table_u8(uint8_t* ptr,
                          size_t width,
                          size_t height,
                          size_t stride) {
  wuffs_base__table_u8 ret;
  ret.ptr = ptr;
  ret.width = width;
  ret.height = height;
  ret.stride = stride;
  return ret;
}

static inline wuffs_base__table_u16  //
wuffs_base__make_table_u16(uint16_t* ptr,
                           size_t width,
                           size_t height,
                           size_t stride) {
  wuffs_base__table_u16 ret;
  ret.ptr = ptr;
  ret.width = width;
  ret.height = height;
  ret.stride = stride;
  return ret;
}

static inline wuffs_base__table_u32  //
wuffs_base__make_table_u32(uint32_t* ptr,
                           size_t width,
                           size_t height,
                           size_t stride) {
  wuffs_base__table_u32 ret;
  ret.ptr = ptr;
  ret.width = width;
  ret.height = height;
  ret.stride = stride;
  return ret;
}

static inline wuffs_base__table_u64  //
wuffs_base__make_table_u64(uint64_t* ptr,
                           size_t width,
                           size_t height,
                           size_t stride) {
  wuffs_base__table_u64 ret;
  ret.ptr = ptr;
  ret.width = width;
  ret.height = height;
  ret.stride = stride;
  return ret;
}

static inline wuffs_base__table_u8  //
wuffs_base__empty_table_u8(void) {
  wuffs_base__table_u8 ret;
  ret.ptr = NULL;
  ret.width = 0;
  ret.height = 0;
  ret.stride = 0;
  return ret;
}

static inline wuffs_base__table_u16  //
wuffs_base__empty_table_u16(void) {
  wuffs_base__table_u16 ret;
  ret.ptr = NULL;
  ret.width = 0;
  ret.height = 0;
  ret.stride = 0;
  return ret;
}

static inline wuffs_base__table_u32  //
wuffs_base__empty_table_u32(void) {
  wuffs_base__table_u32 ret;
  ret.ptr = NULL;
  ret.width = 0;
  ret.height = 0;
  ret.stride = 0;
  return ret;
}

static inline wuffs_base__table_u64  //
wuffs_base__empty_table_u64(void) {
  wuffs_base__table_u64 ret;
  ret.ptr = NULL;
  ret.width = 0;
  ret.height = 0;
  ret.stride = 0;
  return ret;
}

static inline bool  //
wuffs_base__slice_u8__overlaps(wuffs_base__slice_u8 s, wuffs_base__slice_u8 t) {
  return ((s.ptr <= t.ptr) && (t.ptr < (s.ptr + s.len))) ||
         ((t.ptr <= s.ptr) && (s.ptr < (t.ptr + t.len)));
}

// wuffs_base__slice_u8__subslice_i returns s[i:].
//
// It returns an empty slice if i is out of bounds.
static inline wuffs_base__slice_u8  //
wuffs_base__slice_u8__subslice_i(wuffs_base__slice_u8 s, uint64_t i) {
  if ((i <= SIZE_MAX) && (i <= s.len)) {
    return wuffs_base__make_slice_u8(s.ptr + i, ((size_t)(s.len - i)));
  }
  return wuffs_base__empty_slice_u8();
}

// wuffs_base__slice_u8__subslice_j returns s[:j].
//
// It returns an empty slice if j is out of bounds.
static inline wuffs_base__slice_u8  //
wuffs_base__slice_u8__subslice_j(wuffs_base__slice_u8 s, uint64_t j) {
  if ((j <= SIZE_MAX) && (j <= s.len)) {
    return wuffs_base__make_slice_u8(s.ptr, ((size_t)j));
  }
  return wuffs_base__empty_slice_u8();
}

// wuffs_base__slice_u8__subslice_ij returns s[i:j].
//
// It returns an empty slice if i or j is out of bounds.
static inline wuffs_base__slice_u8  //
wuffs_base__slice_u8__subslice_ij(wuffs_base__slice_u8 s,
                                  uint64_t i,
                                  uint64_t j) {
  if ((i <= j) && (j <= SIZE_MAX) && (j <= s.len)) {
    return wuffs_base__make_slice_u8(s.ptr + i, ((size_t)(j - i)));
  }
  return wuffs_base__empty_slice_u8();
}

// wuffs_base__table_u8__subtable_ij returns t[ix:jx, iy:jy].
//
// It returns an empty table if i or j is out of bounds.
static inline wuffs_base__table_u8  //
wuffs_base__table_u8__subtable_ij(wuffs_base__table_u8 t,
                                  uint64_t ix,
                                  uint64_t iy,
                                  uint64_t jx,
                                  uint64_t jy) {
  if ((ix <= jx) && (jx <= SIZE_MAX) && (jx <= t.width) &&  //
      (iy <= jy) && (jy <= SIZE_MAX) && (jy <= t.height)) {
    return wuffs_base__make_table_u8(t.ptr + ix + (iy * t.stride),  //
                                     ((size_t)(jx - ix)),           //
                                     ((size_t)(jy - iy)),           //
                                     t.stride);                     //
  }
  return wuffs_base__make_table_u8(NULL, 0, 0, 0);
}

// wuffs_base__table__flattened_length returns the number of elements covered
// by the 1-dimensional span that backs a 2-dimensional table. This counts the
// elements inside the table and, when width != stride, the elements outside
// the table but between its rows.
//
// For example, consider a width 10, height 4, stride 10 table. Mark its first
// and last (inclusive) elements with 'a' and 'z'. This function returns 40.
//
//    a123456789
//    0123456789
//    0123456789
//    012345678z
//
// Now consider the sub-table of that from (2, 1) inclusive to (8, 4) exclusive.
//
//    a123456789
//    01iiiiiioo
//    ooiiiiiioo
//    ooiiiiii8z
//
// This function (called with width 6, height 3, stride 10) returns 26: 18 'i'
// inside elements plus 8 'o' outside elements. Note that 26 is less than a
// naive (height * stride = 30) computation. Indeed, advancing 29 elements from
// the first 'i' would venture past 'z', out of bounds of the original table.
//
// It does not check for overflow, but if the arguments come from a table that
// exists in memory and each element occupies a positive number of bytes then
// the result should be bounded by the amount of allocatable memory (which
// shouldn't overflow SIZE_MAX).
static inline size_t  //
wuffs_base__table__flattened_length(size_t width,
                                    size_t height,
                                    size_t stride) {
  if (height == 0) {
    return 0;
  }
  return ((height - 1) * stride) + width;
}

// ---------------- Magic Numbers

// wuffs_base__magic_number_guess_fourcc guesses the file format of some data,
// given its starting bytes (the prefix_data argument) and whether or not there
// may be further bytes (the prefix_closed argument; true means that
// prefix_data is the entire data).
//
// It returns a positive FourCC value on success.
//
// It returns zero if nothing matches its hard-coded list of 'magic numbers'.
//
// It returns a negative value if prefix_closed is false and a longer prefix is
// required for a conclusive result. For example, a single 'B' byte (without
// further data) is not enough to discriminate the BMP and BPG image file
// formats. Similarly, a single '\xFF' byte might be the start of JPEG data or
// it might be the start of some other binary data.
//
// It does not do a full validity check. Like any guess made from a short
// prefix of the data, it may return false positives. Data that starts with 99
// bytes of valid JPEG followed by corruption or truncation is an invalid JPEG
// image overall, but this function will still return WUFFS_BASE__FOURCC__JPEG.
//
// Another source of false positives is that some 'magic numbers' are valid
// ASCII data. A file starting with "GIF87a and GIF89a are the two versions of
// GIF" will match GIF's 'magic number' even if it's plain text, not an image.
//
// For modular builds that divide the base module into sub-modules, using this
// function requires the WUFFS_CONFIG__MODULE__BASE__MAGIC sub-module, not just
// WUFFS_CONFIG__MODULE__BASE__CORE.
WUFFS_BASE__MAYBE_STATIC int32_t  //
wuffs_base__magic_number_guess_fourcc(wuffs_base__slice_u8 prefix_data,
                                      bool prefix_closed);

// ---------------- Quirk Values

// These constants are the value half of a key-value pair, where the key is
// WUFFS_BASE__QUIRK_QUALITY.
//
// In the Wuffs API, set_quirk takes a u64 value. These macro definitions are
// likewise unsigned values (uint64_t) but for this particular key, they are
// best interpreted as signed values (int64_t). "Lower-than-default quality"
// and "higher-than-default quality", as signed values, are -1 and +1.
//
// See doc/note/quirks.md for some more discussion about trade-offs.
#define WUFFS_BASE__QUIRK_QUALITY__VALUE__LOWER_QUALITY UINT64_MAX
#define WUFFS_BASE__QUIRK_QUALITY__VALUE__HIGHER_QUALITY ((uint64_t)1)
