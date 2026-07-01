// Copyright 2021 The Wuffs Authors.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

package armneonintrinsics_test

import (
	"fmt"

	ani "github.com/google/wuffs/lib/armneonintrinsics"
)

func Example() {
	{
		const cTypeName = "uint64x2x3_t"
		fmt.Printf("armneonintrinsics.TypeUint32x2 is %q\n", ani.TypeUint32x2)
		fmt.Printf("%q is armneonintrinsics.Type(0x%02X)\n",
			cTypeName, uint8(ani.ParseType(cTypeName)))
		fmt.Println()
	}

	{
		fmt.Println("First 5 functions:")
		for i, f := 0, ani.FirstFunction(); (i < 5) && f.IsValid(); i, f = i+1, f.NextFunction() {
			fmt.Println(f)
		}
		fmt.Println()
	}

	for _, cFuncName := range []string{"vsetq_lane_u8", "rumpelstiltskin"} {
		f := ani.FindFunction(cFuncName)
		fmt.Printf("armneonintrinsics.FindFunction(%q) is:\n%s\n", cFuncName, f)
		fmt.Printf("f.IsValid():    %t\n", f.IsValid())
		fmt.Printf("f.Name():       %q\n", f.Name())
		fmt.Printf("f.ReturnType(): %v\n", f.ReturnType())
		fmt.Println()
	}

	{
		const typ = ani.TypeBfloat16x4
		fmt.Printf("Functions whose first argument has type %q:\n", typ)
		for f := ani.FirstFunction(); f.IsValid(); f = f.NextFunction() {
			if f.FirstArgument().Type() == typ {
				fmt.Printf("%-24s has %d arguments\n", f.Name(), f.NumArguments())
			}
		}
		fmt.Println()
	}

	// Output:
	// armneonintrinsics.TypeUint32x2 is "uint32x2_t"
	// "uint64x2x3_t" is armneonintrinsics.Type(0x7E)
	//
	// First 5 functions:
	// int16x4_t vaba_s16(int16x4_t __a, int16x4_t __b, int16x4_t __c)
	// int32x2_t vaba_s32(int32x2_t __a, int32x2_t __b, int32x2_t __c)
	// int8x8_t vaba_s8(int8x8_t __a, int8x8_t __b, int8x8_t __c)
	// uint16x4_t vaba_u16(uint16x4_t __a, uint16x4_t __b, uint16x4_t __c)
	// uint32x2_t vaba_u32(uint32x2_t __a, uint32x2_t __b, uint32x2_t __c)
	//
	// armneonintrinsics.FindFunction("vsetq_lane_u8") is:
	// uint8x16_t vsetq_lane_u8(uint8_t __elem, uint8x16_t __vec, const int32_t __index)
	// f.IsValid():    true
	// f.Name():       "vsetq_lane_u8"
	// f.ReturnType(): uint8x16_t
	//
	// armneonintrinsics.FindFunction("rumpelstiltskin") is:
	// no_such_function
	// f.IsValid():    false
	// f.Name():       ""
	// f.ReturnType(): no_such_type
	//
	// Functions whose first argument has type "bfloat16x4_t":
	// vcombine_bf16            has 2 arguments
	// vcopy_lane_bf16          has 4 arguments
	// vcopy_laneq_bf16         has 4 arguments
	// vcvt_f32_bf16            has 1 arguments
	// vdup_lane_bf16           has 2 arguments
	// vduph_lane_bf16          has 2 arguments
	// vdupq_lane_bf16          has 2 arguments
	// vget_lane_bf16           has 2 arguments
	// vreinterpret_f16_bf16    has 1 arguments
	// vreinterpret_f32_bf16    has 1 arguments
	// vreinterpret_f64_bf16    has 1 arguments
	// vreinterpret_p16_bf16    has 1 arguments
	// vreinterpret_p64_bf16    has 1 arguments
	// vreinterpret_p8_bf16     has 1 arguments
	// vreinterpret_s16_bf16    has 1 arguments
	// vreinterpret_s32_bf16    has 1 arguments
	// vreinterpret_s64_bf16    has 1 arguments
	// vreinterpret_s8_bf16     has 1 arguments
	// vreinterpret_u16_bf16    has 1 arguments
	// vreinterpret_u32_bf16    has 1 arguments
	// vreinterpret_u64_bf16    has 1 arguments
	// vreinterpret_u8_bf16     has 1 arguments
}
