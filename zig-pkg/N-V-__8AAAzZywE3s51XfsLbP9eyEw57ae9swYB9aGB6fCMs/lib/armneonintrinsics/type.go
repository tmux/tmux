// Copyright 2021 The Wuffs Authors.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

package armneonintrinsics

import (
	"strings"
)

const (
	baseSize___8Bits = 0
	baseSize__16Bits = 1
	baseSize__32Bits = 2
	baseSize__64Bits = 3
	baseSize_128Bits = 4
)

// Type is a NEON C type. Not all valid C types have an armneonintrinsics.Type,
// only those C types used by "arm_neon.h".
type Type uint8

const (
	TypeBits0_Mask  = 0xF0
	TypeBits0_Shift = 4

	TypeBits0_I008 Type = 0x00
	TypeBits0_I016 Type = 0x10
	TypeBits0_I032 Type = 0x20
	TypeBits0_I064 Type = 0x30
	TypeBits0_U008 Type = 0x40
	TypeBits0_U016 Type = 0x50
	TypeBits0_U032 Type = 0x60
	TypeBits0_U064 Type = 0x70
	TypeBits0_B016 Type = 0x80
	TypeBits0_F016 Type = 0x90
	TypeBits0_F032 Type = 0xA0
	TypeBits0_F064 Type = 0xB0
	TypeBits0_P008 Type = 0xC0
	TypeBits0_P016 Type = 0xD0
	TypeBits0_P064 Type = 0xE0
	TypeBits0_P128 Type = 0xF0
)

const (
	TypeBits1_Mask  = 0x0C
	TypeBits1_Shift = 2

	TypeBits1____Void Type = 0x00 // Only "void" if combined with TypeBits0_P128.
	TypeBits1__Scalar Type = 0x04
	TypeBits1_DVector Type = 0x08 //  64-bit D (double-word) register.
	TypeBits1_QVector Type = 0x0C // 128-bit Q (  quad-word) register.
)

const (
	TypeBits2_Mask  = 0x03
	TypeBits2_Shift = 0

	TypeBits2_ScaRegMutbl Type = 0x00 //       foo
	TypeBits2_ScaRegConst Type = 0x01 // const foo
	TypeBits2_ScaPtrMutbl Type = 0x02 //       foo *
	TypeBits2_ScaPtrConst Type = 0x03 // const foo *

	TypeBits2_VecX1 Type = 0x00
	TypeBits2_VecX2 Type = 0x01
	TypeBits2_VecX3 Type = 0x02
	TypeBits2_VecX4 Type = 0x03
)

const (
	TypeInvalid Type = 0

	TypeVoid Type = TypeBits0_P128 | TypeBits1____Void

	// Scalar ScaRegMutbl types.
	TypeInt8     Type = TypeBits0_I008 | TypeBits1__Scalar | TypeBits2_ScaRegMutbl
	TypeInt16    Type = TypeBits0_I016 | TypeBits1__Scalar | TypeBits2_ScaRegMutbl
	TypeInt32    Type = TypeBits0_I032 | TypeBits1__Scalar | TypeBits2_ScaRegMutbl
	TypeInt64    Type = TypeBits0_I064 | TypeBits1__Scalar | TypeBits2_ScaRegMutbl
	TypeUint8    Type = TypeBits0_U008 | TypeBits1__Scalar | TypeBits2_ScaRegMutbl
	TypeUint16   Type = TypeBits0_U016 | TypeBits1__Scalar | TypeBits2_ScaRegMutbl
	TypeUint32   Type = TypeBits0_U032 | TypeBits1__Scalar | TypeBits2_ScaRegMutbl
	TypeUint64   Type = TypeBits0_U064 | TypeBits1__Scalar | TypeBits2_ScaRegMutbl
	TypeBfloat16 Type = TypeBits0_B016 | TypeBits1__Scalar | TypeBits2_ScaRegMutbl
	TypeFloat16  Type = TypeBits0_F016 | TypeBits1__Scalar | TypeBits2_ScaRegMutbl
	TypeFloat32  Type = TypeBits0_F032 | TypeBits1__Scalar | TypeBits2_ScaRegMutbl
	TypeFloat64  Type = TypeBits0_F064 | TypeBits1__Scalar | TypeBits2_ScaRegMutbl
	TypePoly8    Type = TypeBits0_P008 | TypeBits1__Scalar | TypeBits2_ScaRegMutbl
	TypePoly16   Type = TypeBits0_P016 | TypeBits1__Scalar | TypeBits2_ScaRegMutbl
	TypePoly64   Type = TypeBits0_P064 | TypeBits1__Scalar | TypeBits2_ScaRegMutbl
	TypePoly128  Type = TypeBits0_P128 | TypeBits1__Scalar | TypeBits2_ScaRegMutbl

	// Scalar ScaRegConst types.
	TypeConstInt8     Type = TypeBits0_I008 | TypeBits1__Scalar | TypeBits2_ScaRegConst
	TypeConstInt16    Type = TypeBits0_I016 | TypeBits1__Scalar | TypeBits2_ScaRegConst
	TypeConstInt32    Type = TypeBits0_I032 | TypeBits1__Scalar | TypeBits2_ScaRegConst
	TypeConstInt64    Type = TypeBits0_I064 | TypeBits1__Scalar | TypeBits2_ScaRegConst
	TypeConstUint8    Type = TypeBits0_U008 | TypeBits1__Scalar | TypeBits2_ScaRegConst
	TypeConstUint16   Type = TypeBits0_U016 | TypeBits1__Scalar | TypeBits2_ScaRegConst
	TypeConstUint32   Type = TypeBits0_U032 | TypeBits1__Scalar | TypeBits2_ScaRegConst
	TypeConstUint64   Type = TypeBits0_U064 | TypeBits1__Scalar | TypeBits2_ScaRegConst
	TypeConstBfloat16 Type = TypeBits0_B016 | TypeBits1__Scalar | TypeBits2_ScaRegConst
	TypeConstFloat16  Type = TypeBits0_F016 | TypeBits1__Scalar | TypeBits2_ScaRegConst
	TypeConstFloat32  Type = TypeBits0_F032 | TypeBits1__Scalar | TypeBits2_ScaRegConst
	TypeConstFloat64  Type = TypeBits0_F064 | TypeBits1__Scalar | TypeBits2_ScaRegConst
	TypeConstPoly8    Type = TypeBits0_P008 | TypeBits1__Scalar | TypeBits2_ScaRegConst
	TypeConstPoly16   Type = TypeBits0_P016 | TypeBits1__Scalar | TypeBits2_ScaRegConst
	TypeConstPoly64   Type = TypeBits0_P064 | TypeBits1__Scalar | TypeBits2_ScaRegConst
	TypeConstPoly128  Type = TypeBits0_P128 | TypeBits1__Scalar | TypeBits2_ScaRegConst

	// DVector VecX1 types.
	TypeInt8x8     Type = TypeBits0_I008 | TypeBits1_DVector | TypeBits2_VecX1
	TypeInt16x4    Type = TypeBits0_I016 | TypeBits1_DVector | TypeBits2_VecX1
	TypeInt32x2    Type = TypeBits0_I032 | TypeBits1_DVector | TypeBits2_VecX1
	TypeInt64x1    Type = TypeBits0_I064 | TypeBits1_DVector | TypeBits2_VecX1
	TypeUint8x8    Type = TypeBits0_U008 | TypeBits1_DVector | TypeBits2_VecX1
	TypeUint16x4   Type = TypeBits0_U016 | TypeBits1_DVector | TypeBits2_VecX1
	TypeUint32x2   Type = TypeBits0_U032 | TypeBits1_DVector | TypeBits2_VecX1
	TypeUint64x1   Type = TypeBits0_U064 | TypeBits1_DVector | TypeBits2_VecX1
	TypeBfloat16x4 Type = TypeBits0_B016 | TypeBits1_DVector | TypeBits2_VecX1
	TypeFloat16x4  Type = TypeBits0_F016 | TypeBits1_DVector | TypeBits2_VecX1
	TypeFloat32x2  Type = TypeBits0_F032 | TypeBits1_DVector | TypeBits2_VecX1
	TypeFloat64x1  Type = TypeBits0_F064 | TypeBits1_DVector | TypeBits2_VecX1
	TypePoly8x8    Type = TypeBits0_P008 | TypeBits1_DVector | TypeBits2_VecX1
	TypePoly16x4   Type = TypeBits0_P016 | TypeBits1_DVector | TypeBits2_VecX1
	TypePoly64x1   Type = TypeBits0_P064 | TypeBits1_DVector | TypeBits2_VecX1

	// DVector VecX2 types.
	TypeInt8x8x2     Type = TypeBits0_I008 | TypeBits1_DVector | TypeBits2_VecX2
	TypeInt16x4x2    Type = TypeBits0_I016 | TypeBits1_DVector | TypeBits2_VecX2
	TypeInt32x2x2    Type = TypeBits0_I032 | TypeBits1_DVector | TypeBits2_VecX2
	TypeInt64x1x2    Type = TypeBits0_I064 | TypeBits1_DVector | TypeBits2_VecX2
	TypeUint8x8x2    Type = TypeBits0_U008 | TypeBits1_DVector | TypeBits2_VecX2
	TypeUint16x4x2   Type = TypeBits0_U016 | TypeBits1_DVector | TypeBits2_VecX2
	TypeUint32x2x2   Type = TypeBits0_U032 | TypeBits1_DVector | TypeBits2_VecX2
	TypeUint64x1x2   Type = TypeBits0_U064 | TypeBits1_DVector | TypeBits2_VecX2
	TypeBfloat16x4x2 Type = TypeBits0_B016 | TypeBits1_DVector | TypeBits2_VecX2
	TypeFloat16x4x2  Type = TypeBits0_F016 | TypeBits1_DVector | TypeBits2_VecX2
	TypeFloat32x2x2  Type = TypeBits0_F032 | TypeBits1_DVector | TypeBits2_VecX2
	TypeFloat64x1x2  Type = TypeBits0_F064 | TypeBits1_DVector | TypeBits2_VecX2
	TypePoly8x8x2    Type = TypeBits0_P008 | TypeBits1_DVector | TypeBits2_VecX2
	TypePoly16x4x2   Type = TypeBits0_P016 | TypeBits1_DVector | TypeBits2_VecX2
	TypePoly64x1x2   Type = TypeBits0_P064 | TypeBits1_DVector | TypeBits2_VecX2

	// DVector VecX3 types.
	TypeInt8x8x3     Type = TypeBits0_I008 | TypeBits1_DVector | TypeBits2_VecX3
	TypeInt16x4x3    Type = TypeBits0_I016 | TypeBits1_DVector | TypeBits2_VecX3
	TypeInt32x2x3    Type = TypeBits0_I032 | TypeBits1_DVector | TypeBits2_VecX3
	TypeInt64x1x3    Type = TypeBits0_I064 | TypeBits1_DVector | TypeBits2_VecX3
	TypeUint8x8x3    Type = TypeBits0_U008 | TypeBits1_DVector | TypeBits2_VecX3
	TypeUint16x4x3   Type = TypeBits0_U016 | TypeBits1_DVector | TypeBits2_VecX3
	TypeUint32x2x3   Type = TypeBits0_U032 | TypeBits1_DVector | TypeBits2_VecX3
	TypeUint64x1x3   Type = TypeBits0_U064 | TypeBits1_DVector | TypeBits2_VecX3
	TypeBfloat16x4x3 Type = TypeBits0_B016 | TypeBits1_DVector | TypeBits2_VecX3
	TypeFloat16x4x3  Type = TypeBits0_F016 | TypeBits1_DVector | TypeBits2_VecX3
	TypeFloat32x2x3  Type = TypeBits0_F032 | TypeBits1_DVector | TypeBits2_VecX3
	TypeFloat64x1x3  Type = TypeBits0_F064 | TypeBits1_DVector | TypeBits2_VecX3
	TypePoly8x8x3    Type = TypeBits0_P008 | TypeBits1_DVector | TypeBits2_VecX3
	TypePoly16x4x3   Type = TypeBits0_P016 | TypeBits1_DVector | TypeBits2_VecX3
	TypePoly64x1x3   Type = TypeBits0_P064 | TypeBits1_DVector | TypeBits2_VecX3

	// DVector VecX4 types.
	TypeInt8x8x4     Type = TypeBits0_I008 | TypeBits1_DVector | TypeBits2_VecX4
	TypeInt16x4x4    Type = TypeBits0_I016 | TypeBits1_DVector | TypeBits2_VecX4
	TypeInt32x2x4    Type = TypeBits0_I032 | TypeBits1_DVector | TypeBits2_VecX4
	TypeInt64x1x4    Type = TypeBits0_I064 | TypeBits1_DVector | TypeBits2_VecX4
	TypeUint8x8x4    Type = TypeBits0_U008 | TypeBits1_DVector | TypeBits2_VecX4
	TypeUint16x4x4   Type = TypeBits0_U016 | TypeBits1_DVector | TypeBits2_VecX4
	TypeUint32x2x4   Type = TypeBits0_U032 | TypeBits1_DVector | TypeBits2_VecX4
	TypeUint64x1x4   Type = TypeBits0_U064 | TypeBits1_DVector | TypeBits2_VecX4
	TypeBfloat16x4x4 Type = TypeBits0_B016 | TypeBits1_DVector | TypeBits2_VecX4
	TypeFloat16x4x4  Type = TypeBits0_F016 | TypeBits1_DVector | TypeBits2_VecX4
	TypeFloat32x2x4  Type = TypeBits0_F032 | TypeBits1_DVector | TypeBits2_VecX4
	TypeFloat64x1x4  Type = TypeBits0_F064 | TypeBits1_DVector | TypeBits2_VecX4
	TypePoly8x8x4    Type = TypeBits0_P008 | TypeBits1_DVector | TypeBits2_VecX4
	TypePoly16x4x4   Type = TypeBits0_P016 | TypeBits1_DVector | TypeBits2_VecX4
	TypePoly64x1x4   Type = TypeBits0_P064 | TypeBits1_DVector | TypeBits2_VecX4

	// QVector VecX1 types.
	TypeInt8x16    Type = TypeBits0_I008 | TypeBits1_QVector | TypeBits2_VecX1
	TypeInt16x8    Type = TypeBits0_I016 | TypeBits1_QVector | TypeBits2_VecX1
	TypeInt32x4    Type = TypeBits0_I032 | TypeBits1_QVector | TypeBits2_VecX1
	TypeInt64x2    Type = TypeBits0_I064 | TypeBits1_QVector | TypeBits2_VecX1
	TypeUint8x16   Type = TypeBits0_U008 | TypeBits1_QVector | TypeBits2_VecX1
	TypeUint16x8   Type = TypeBits0_U016 | TypeBits1_QVector | TypeBits2_VecX1
	TypeUint32x4   Type = TypeBits0_U032 | TypeBits1_QVector | TypeBits2_VecX1
	TypeUint64x2   Type = TypeBits0_U064 | TypeBits1_QVector | TypeBits2_VecX1
	TypeBfloat16x8 Type = TypeBits0_B016 | TypeBits1_QVector | TypeBits2_VecX1
	TypeFloat16x8  Type = TypeBits0_F016 | TypeBits1_QVector | TypeBits2_VecX1
	TypeFloat32x4  Type = TypeBits0_F032 | TypeBits1_QVector | TypeBits2_VecX1
	TypeFloat64x2  Type = TypeBits0_F064 | TypeBits1_QVector | TypeBits2_VecX1
	TypePoly8x16   Type = TypeBits0_P008 | TypeBits1_QVector | TypeBits2_VecX1
	TypePoly16x8   Type = TypeBits0_P016 | TypeBits1_QVector | TypeBits2_VecX1
	TypePoly64x2   Type = TypeBits0_P064 | TypeBits1_QVector | TypeBits2_VecX1

	// QVector VecX2 types.
	TypeInt8x16x2    Type = TypeBits0_I008 | TypeBits1_QVector | TypeBits2_VecX2
	TypeInt16x8x2    Type = TypeBits0_I016 | TypeBits1_QVector | TypeBits2_VecX2
	TypeInt32x4x2    Type = TypeBits0_I032 | TypeBits1_QVector | TypeBits2_VecX2
	TypeInt64x2x2    Type = TypeBits0_I064 | TypeBits1_QVector | TypeBits2_VecX2
	TypeUint8x16x2   Type = TypeBits0_U008 | TypeBits1_QVector | TypeBits2_VecX2
	TypeUint16x8x2   Type = TypeBits0_U016 | TypeBits1_QVector | TypeBits2_VecX2
	TypeUint32x4x2   Type = TypeBits0_U032 | TypeBits1_QVector | TypeBits2_VecX2
	TypeUint64x2x2   Type = TypeBits0_U064 | TypeBits1_QVector | TypeBits2_VecX2
	TypeBfloat16x8x2 Type = TypeBits0_B016 | TypeBits1_QVector | TypeBits2_VecX2
	TypeFloat16x8x2  Type = TypeBits0_F016 | TypeBits1_QVector | TypeBits2_VecX2
	TypeFloat32x4x2  Type = TypeBits0_F032 | TypeBits1_QVector | TypeBits2_VecX2
	TypeFloat64x2x2  Type = TypeBits0_F064 | TypeBits1_QVector | TypeBits2_VecX2
	TypePoly8x16x2   Type = TypeBits0_P008 | TypeBits1_QVector | TypeBits2_VecX2
	TypePoly16x8x2   Type = TypeBits0_P016 | TypeBits1_QVector | TypeBits2_VecX2
	TypePoly64x2x2   Type = TypeBits0_P064 | TypeBits1_QVector | TypeBits2_VecX2

	// QVector VecX3 types.
	TypeInt8x16x3    Type = TypeBits0_I008 | TypeBits1_QVector | TypeBits2_VecX3
	TypeInt16x8x3    Type = TypeBits0_I016 | TypeBits1_QVector | TypeBits2_VecX3
	TypeInt32x4x3    Type = TypeBits0_I032 | TypeBits1_QVector | TypeBits2_VecX3
	TypeInt64x2x3    Type = TypeBits0_I064 | TypeBits1_QVector | TypeBits2_VecX3
	TypeUint8x16x3   Type = TypeBits0_U008 | TypeBits1_QVector | TypeBits2_VecX3
	TypeUint16x8x3   Type = TypeBits0_U016 | TypeBits1_QVector | TypeBits2_VecX3
	TypeUint32x4x3   Type = TypeBits0_U032 | TypeBits1_QVector | TypeBits2_VecX3
	TypeUint64x2x3   Type = TypeBits0_U064 | TypeBits1_QVector | TypeBits2_VecX3
	TypeBfloat16x8x3 Type = TypeBits0_B016 | TypeBits1_QVector | TypeBits2_VecX3
	TypeFloat16x8x3  Type = TypeBits0_F016 | TypeBits1_QVector | TypeBits2_VecX3
	TypeFloat32x4x3  Type = TypeBits0_F032 | TypeBits1_QVector | TypeBits2_VecX3
	TypeFloat64x2x3  Type = TypeBits0_F064 | TypeBits1_QVector | TypeBits2_VecX3
	TypePoly8x16x3   Type = TypeBits0_P008 | TypeBits1_QVector | TypeBits2_VecX3
	TypePoly16x8x3   Type = TypeBits0_P016 | TypeBits1_QVector | TypeBits2_VecX3
	TypePoly64x2x3   Type = TypeBits0_P064 | TypeBits1_QVector | TypeBits2_VecX3

	// QVector VecX4 types.
	TypeInt8x16x4    Type = TypeBits0_I008 | TypeBits1_QVector | TypeBits2_VecX4
	TypeInt16x8x4    Type = TypeBits0_I016 | TypeBits1_QVector | TypeBits2_VecX4
	TypeInt32x4x4    Type = TypeBits0_I032 | TypeBits1_QVector | TypeBits2_VecX4
	TypeInt64x2x4    Type = TypeBits0_I064 | TypeBits1_QVector | TypeBits2_VecX4
	TypeUint8x16x4   Type = TypeBits0_U008 | TypeBits1_QVector | TypeBits2_VecX4
	TypeUint16x8x4   Type = TypeBits0_U016 | TypeBits1_QVector | TypeBits2_VecX4
	TypeUint32x4x4   Type = TypeBits0_U032 | TypeBits1_QVector | TypeBits2_VecX4
	TypeUint64x2x4   Type = TypeBits0_U064 | TypeBits1_QVector | TypeBits2_VecX4
	TypeBfloat16x8x4 Type = TypeBits0_B016 | TypeBits1_QVector | TypeBits2_VecX4
	TypeFloat16x8x4  Type = TypeBits0_F016 | TypeBits1_QVector | TypeBits2_VecX4
	TypeFloat32x4x4  Type = TypeBits0_F032 | TypeBits1_QVector | TypeBits2_VecX4
	TypeFloat64x2x4  Type = TypeBits0_F064 | TypeBits1_QVector | TypeBits2_VecX4
	TypePoly8x16x4   Type = TypeBits0_P008 | TypeBits1_QVector | TypeBits2_VecX4
	TypePoly16x8x4   Type = TypeBits0_P016 | TypeBits1_QVector | TypeBits2_VecX4
	TypePoly64x2x4   Type = TypeBits0_P064 | TypeBits1_QVector | TypeBits2_VecX4
)

// IsValid returns whether t is a valid Type.
func (t Type) IsValid() bool {
	return t != 0
}

// String returns the C type name like "uint16x4x3_t".
func (t Type) String() string {
	return string(t.appendString(nil))
}

func (t Type) appendString(b []byte) []byte {
	switch (t >> 2) & 3 {
	case TypeBits1____Void >> 2:
		if t == TypeVoid {
			return append(b, "void"...)
		}
	case TypeBits1__Scalar >> 2:
		if (t & TypeBits2_ScaRegConst) != 0 {
			b = append(b, "const "...)
		}
		b = append(b, typeBits0Strings[t>>4]...)
		b = append(b, "_t"...)
		if (t & TypeBits2_ScaPtrMutbl) != 0 {
			b = append(b, " *"...)
		}
		return b
	case TypeBits1_DVector >> 2:
		if baseSize := typeBits0BaseSizes[t>>4]; baseSize <= baseSize__64Bits {
			b = append(b, typeBits0Strings[t>>4]...)
			b = append(b, dVectorStrings[baseSize]...)
			b = append(b, vectorXNStrings[t&3]...)
			return append(b, "_t"...)
		}
	case TypeBits1_QVector >> 2:
		if baseSize := typeBits0BaseSizes[t>>4]; baseSize <= baseSize__64Bits {
			b = append(b, typeBits0Strings[t>>4]...)
			b = append(b, qVectorStrings[baseSize]...)
			b = append(b, vectorXNStrings[t&3]...)
			return append(b, "_t"...)
		}
	}
	return append(b, "no_such_type"...)
}

// ParseType parses a C type name like "uint16x4x3_t" as TypeUint16x4x3. It
// returns TypeInvalid (also known as 0) for unrecognized strings.
func ParseType(cTypeName string) (t Type) {
	s := cTypeName
	if s == "void" {
		return TypeVoid
	}

	const const0 = "const "
	const const1 = "__const "
	isConst := false
	if strings.HasPrefix(s, const0) {
		isConst, s = true, trimLeading(s[len(const0):])
	} else if strings.HasPrefix(s, const1) {
		isConst, s = true, trimLeading(s[len(const1):])
	}

	isPointer := false
	if strings.HasSuffix(s, "*") {
		isPointer, s = true, trimTrailing(s[:len(s)-1])
	}

	if s == "int" {
		switch {
		case !isConst && !isPointer:
			return TypeInt32 | TypeBits2_ScaRegMutbl
		case isConst && !isPointer:
			return TypeInt32 | TypeBits2_ScaRegConst
		case !isConst && isPointer:
			return TypeInt32 | TypeBits2_ScaPtrMutbl
		case isConst && isPointer:
			return TypeInt32 | TypeBits2_ScaPtrConst
		}

	} else if strings.HasSuffix(s, "_t") {
		s = s[:len(s)-2]

		for i := 0; ; i++ {
			if i >= len(typeBits0Strings) {
				return TypeInvalid
			} else if prefix := typeBits0Strings[i]; strings.HasPrefix(s, prefix) {
				s = s[len(prefix):]
				t |= Type(i) << 4
				break
			}
		}

		if s == "" {
			switch {
			case !isConst && !isPointer:
				return t | TypeBits1__Scalar | TypeBits2_ScaRegMutbl
			case isConst && !isPointer:
				return t | TypeBits1__Scalar | TypeBits2_ScaRegConst
			case !isConst && isPointer:
				return t | TypeBits1__Scalar | TypeBits2_ScaPtrMutbl
			case isConst && isPointer:
				return t | TypeBits1__Scalar | TypeBits2_ScaPtrConst
			}
		}

		if isConst || isPointer {
			return TypeInvalid
		}

		if baseSize := typeBits0BaseSizes[t>>4]; baseSize <= baseSize__64Bits {
			if dv := dVectorStrings[baseSize]; strings.HasPrefix(s, dv) {
				s = s[len(dv):]
				t |= TypeBits1_DVector
			} else if qv := qVectorStrings[baseSize]; strings.HasPrefix(s, qv) {
				s = s[len(qv):]
				t |= TypeBits1_QVector
			} else {
				return TypeInvalid
			}
			for i, xn := range vectorXNStrings {
				if s == xn {
					return t | Type(i)
				}
			}
		}
	}
	return TypeInvalid
}

func trimLeading(s string) string {
	for (len(s) > 0) && (s[0] <= ' ') {
		s = s[1:]
	}
	return s
}

func trimTrailing(s string) string {
	for (len(s) > 0) && (s[len(s)-1] <= ' ') {
		s = s[:len(s)-1]
	}
	return s
}

var typeBits0BaseSizes = [16]uint8{
	TypeBits0_I008 >> 4: baseSize___8Bits,
	TypeBits0_I016 >> 4: baseSize__16Bits,
	TypeBits0_I032 >> 4: baseSize__32Bits,
	TypeBits0_I064 >> 4: baseSize__64Bits,
	TypeBits0_U008 >> 4: baseSize___8Bits,
	TypeBits0_U016 >> 4: baseSize__16Bits,
	TypeBits0_U032 >> 4: baseSize__32Bits,
	TypeBits0_U064 >> 4: baseSize__64Bits,
	TypeBits0_B016 >> 4: baseSize__16Bits,
	TypeBits0_F016 >> 4: baseSize__16Bits,
	TypeBits0_F032 >> 4: baseSize__32Bits,
	TypeBits0_F064 >> 4: baseSize__64Bits,
	TypeBits0_P008 >> 4: baseSize___8Bits,
	TypeBits0_P016 >> 4: baseSize__16Bits,
	TypeBits0_P064 >> 4: baseSize__64Bits,
	TypeBits0_P128 >> 4: baseSize_128Bits,
}

var typeBits0Strings = [16]string{
	TypeBits0_I008 >> 4: "int8",
	TypeBits0_I016 >> 4: "int16",
	TypeBits0_I032 >> 4: "int32",
	TypeBits0_I064 >> 4: "int64",
	TypeBits0_U008 >> 4: "uint8",
	TypeBits0_U016 >> 4: "uint16",
	TypeBits0_U032 >> 4: "uint32",
	TypeBits0_U064 >> 4: "uint64",
	TypeBits0_B016 >> 4: "bfloat16",
	TypeBits0_F016 >> 4: "float16",
	TypeBits0_F032 >> 4: "float32",
	TypeBits0_F064 >> 4: "float64",
	TypeBits0_P008 >> 4: "poly8",
	TypeBits0_P016 >> 4: "poly16",
	TypeBits0_P064 >> 4: "poly64",
	TypeBits0_P128 >> 4: "poly128",
}

var dVectorStrings = [4]string{
	baseSize___8Bits: "x8",
	baseSize__16Bits: "x4",
	baseSize__32Bits: "x2",
	baseSize__64Bits: "x1",
}

var qVectorStrings = [4]string{
	baseSize___8Bits: "x16",
	baseSize__16Bits: "x8",
	baseSize__32Bits: "x4",
	baseSize__64Bits: "x2",
}

var vectorXNStrings = [4]string{
	0: "",
	1: "x2",
	2: "x3",
	3: "x4",
}
