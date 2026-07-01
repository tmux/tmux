// Copyright 2017 The Wuffs Authors.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

#ifndef WUFFS_INCLUDE_GUARD__BASE
#define WUFFS_INCLUDE_GUARD__BASE

#if defined(WUFFS_IMPLEMENTATION) && !defined(WUFFS_CONFIG__MODULES)
#define WUFFS_CONFIG__MODULES
#define WUFFS_CONFIG__MODULE__BASE
#endif

// ¡ WUFFS MONOLITHIC RELEASE DISCARDS EVERYTHING ABOVE.

// ¡ INSERT base/copyright

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
#if (__cplusplus >= 201103L) || defined(_MSC_VER)
#include <memory>
#define WUFFS_BASE__HAVE_EQ_DELETE
#define WUFFS_BASE__HAVE_UNIQUE_PTR
// The "defined(__clang__)" isn't redundant. While vanilla clang defines
// __GNUC__, clang-cl (which mimics MSVC's cl.exe) does not.
#elif defined(__GNUC__) || defined(__clang__)
#warning "Wuffs' C++ code expects -std=c++11 or later"
#endif

extern "C" {
#endif

// ¡ INSERT base/all-public.h.

// ¡ INSERT InterfaceDeclarations.

// ----------------

#ifdef __cplusplus
}  // extern "C"
#endif

// ‼ WUFFS C HEADER ENDS HERE.
#ifdef WUFFS_IMPLEMENTATION

#ifdef __cplusplus
extern "C" {
#endif

// ¡ INSERT base/all-private.h.

// ----------------

#if !defined(WUFFS_CONFIG__MODULES) || defined(WUFFS_CONFIG__MODULE__BASE) || \
    defined(WUFFS_CONFIG__MODULE__BASE__CORE)

const uint8_t wuffs_private_impl__low_bits_mask__u8[8] = {
    0x00, 0x01, 0x03, 0x07, 0x0F, 0x1F, 0x3F, 0x7F,
};

const uint16_t wuffs_private_impl__low_bits_mask__u16[16] = {
    0x0000, 0x0001, 0x0003, 0x0007, 0x000F, 0x001F, 0x003F, 0x007F,
    0x00FF, 0x01FF, 0x03FF, 0x07FF, 0x0FFF, 0x1FFF, 0x3FFF, 0x7FFF,
};

const uint32_t wuffs_private_impl__low_bits_mask__u32[32] = {
    0x00000000, 0x00000001, 0x00000003, 0x00000007, 0x0000000F, 0x0000001F,
    0x0000003F, 0x0000007F, 0x000000FF, 0x000001FF, 0x000003FF, 0x000007FF,
    0x00000FFF, 0x00001FFF, 0x00003FFF, 0x00007FFF, 0x0000FFFF, 0x0001FFFF,
    0x0003FFFF, 0x0007FFFF, 0x000FFFFF, 0x001FFFFF, 0x003FFFFF, 0x007FFFFF,
    0x00FFFFFF, 0x01FFFFFF, 0x03FFFFFF, 0x07FFFFFF, 0x0FFFFFFF, 0x1FFFFFFF,
    0x3FFFFFFF, 0x7FFFFFFF,
};

const uint64_t wuffs_private_impl__low_bits_mask__u64[64] = {
    0x0000000000000000, 0x0000000000000001, 0x0000000000000003,
    0x0000000000000007, 0x000000000000000F, 0x000000000000001F,
    0x000000000000003F, 0x000000000000007F, 0x00000000000000FF,
    0x00000000000001FF, 0x00000000000003FF, 0x00000000000007FF,
    0x0000000000000FFF, 0x0000000000001FFF, 0x0000000000003FFF,
    0x0000000000007FFF, 0x000000000000FFFF, 0x000000000001FFFF,
    0x000000000003FFFF, 0x000000000007FFFF, 0x00000000000FFFFF,
    0x00000000001FFFFF, 0x00000000003FFFFF, 0x00000000007FFFFF,
    0x0000000000FFFFFF, 0x0000000001FFFFFF, 0x0000000003FFFFFF,
    0x0000000007FFFFFF, 0x000000000FFFFFFF, 0x000000001FFFFFFF,
    0x000000003FFFFFFF, 0x000000007FFFFFFF, 0x00000000FFFFFFFF,
    0x00000001FFFFFFFF, 0x00000003FFFFFFFF, 0x00000007FFFFFFFF,
    0x0000000FFFFFFFFF, 0x0000001FFFFFFFFF, 0x0000003FFFFFFFFF,
    0x0000007FFFFFFFFF, 0x000000FFFFFFFFFF, 0x000001FFFFFFFFFF,
    0x000003FFFFFFFFFF, 0x000007FFFFFFFFFF, 0x00000FFFFFFFFFFF,
    0x00001FFFFFFFFFFF, 0x00003FFFFFFFFFFF, 0x00007FFFFFFFFFFF,
    0x0000FFFFFFFFFFFF, 0x0001FFFFFFFFFFFF, 0x0003FFFFFFFFFFFF,
    0x0007FFFFFFFFFFFF, 0x000FFFFFFFFFFFFF, 0x001FFFFFFFFFFFFF,
    0x003FFFFFFFFFFFFF, 0x007FFFFFFFFFFFFF, 0x00FFFFFFFFFFFFFF,
    0x01FFFFFFFFFFFFFF, 0x03FFFFFFFFFFFFFF, 0x07FFFFFFFFFFFFFF,
    0x0FFFFFFFFFFFFFFF, 0x1FFFFFFFFFFFFFFF, 0x3FFFFFFFFFFFFFFF,
    0x7FFFFFFFFFFFFFFF,
};

const uint32_t wuffs_private_impl__pixel_format__bits_per_channel[16] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x0A, 0x0C, 0x10, 0x18, 0x20, 0x30, 0x40,
};

// ¡ INSERT wuffs_base__status strings.

// ¡ INSERT vtable names.

#endif  // !defined(WUFFS_CONFIG__MODULES) ||
        // defined(WUFFS_CONFIG__MODULE__BASE)  ||
        // defined(WUFFS_CONFIG__MODULE__BASE__CORE)

#if !defined(WUFFS_CONFIG__MODULES) || defined(WUFFS_CONFIG__MODULE__BASE) || \
    defined(WUFFS_CONFIG__MODULE__BASE__INTERFACES)

// ¡ INSERT InterfaceDefinitions.

#endif  // !defined(WUFFS_CONFIG__MODULES) ||
        // defined(WUFFS_CONFIG__MODULE__BASE) ||
        // defined(WUFFS_CONFIG__MODULE__BASE__INTERFACES)

#if !defined(WUFFS_CONFIG__MODULES) || defined(WUFFS_CONFIG__MODULE__BASE) || \
    defined(WUFFS_CONFIG__MODULE__BASE__FLOATCONV)

// ¡ INSERT base/floatconv-submodule.c.

#endif  // !defined(WUFFS_CONFIG__MODULES) ||
        // defined(WUFFS_CONFIG__MODULE__BASE) ||
        // defined(WUFFS_CONFIG__MODULE__BASE__FLOATCONV)

#if !defined(WUFFS_CONFIG__MODULES) || defined(WUFFS_CONFIG__MODULE__BASE) || \
    defined(WUFFS_CONFIG__MODULE__BASE__INTCONV)

// ¡ INSERT base/intconv-submodule.c.

#endif  // !defined(WUFFS_CONFIG__MODULES) ||
        // defined(WUFFS_CONFIG__MODULE__BASE) ||
        // defined(WUFFS_CONFIG__MODULE__BASE__INTCONV)

#if !defined(WUFFS_CONFIG__MODULES) || defined(WUFFS_CONFIG__MODULE__BASE) || \
    defined(WUFFS_CONFIG__MODULE__BASE__MAGIC)

// ¡ INSERT base/magic-submodule.c.

#endif  // !defined(WUFFS_CONFIG__MODULES) ||
        // defined(WUFFS_CONFIG__MODULE__BASE) ||
        // defined(WUFFS_CONFIG__MODULE__BASE__MAGIC)

#if !defined(WUFFS_CONFIG__MODULES) || defined(WUFFS_CONFIG__MODULE__BASE) || \
    defined(WUFFS_CONFIG__MODULE__BASE__PIXCONV)

// ¡ INSERT base/pixconv-submodule-regular.c.

// ¡ INSERT base/pixconv-submodule-ycck.c.

// ¡ INSERT base/pixconv-submodule-x86-avx2.c.

#endif  // !defined(WUFFS_CONFIG__MODULES) ||
        // defined(WUFFS_CONFIG__MODULE__BASE) ||
        // defined(WUFFS_CONFIG__MODULE__BASE__PIXCONV)

#if !defined(WUFFS_CONFIG__MODULES) || defined(WUFFS_CONFIG__MODULE__BASE) || \
    defined(WUFFS_CONFIG__MODULE__BASE__UTF8)

// ¡ INSERT base/utf8-submodule.c.

#endif  // !defined(WUFFS_CONFIG__MODULES) ||
        // defined(WUFFS_CONFIG__MODULE__BASE) ||
        // defined(WUFFS_CONFIG__MODULE__BASE__UTF8)

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // WUFFS_IMPLEMENTATION

// ¡ WUFFS MONOLITHIC RELEASE DISCARDS EVERYTHING BELOW.

#endif  // WUFFS_INCLUDE_GUARD__BASE
