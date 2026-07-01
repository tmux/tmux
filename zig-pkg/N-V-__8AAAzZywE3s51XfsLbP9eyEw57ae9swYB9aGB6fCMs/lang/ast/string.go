// Copyright 2017 The Wuffs Authors.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

package ast

import (
	t "github.com/google/wuffs/lang/token"
)

// Str returns a string form of n.
func (n *Expr) Str(tm *t.Map) string {
	if n == nil {
		return "«nilExpr»"
	}
	if n.id0 == 0 && n.id1 == 0 {
		return tm.ByID(n.id2)
	}
	return string(n.appendStr(nil, tm, false, 0))
}

func (n *Expr) appendStr(buf []byte, tm *t.Map, parenthesize bool, depth uint32) []byte {
	if depth > MaxExprDepth {
		return append(buf, "!expr_recursion_depth_too_large!"...)
	}
	depth++

	if n == nil {
		return buf
	}

	if n.id0.IsXOp() {
		switch {
		case n.id0.IsXUnaryOp():
			buf = append(buf, opString(n.id0)...)
			buf = n.rhs.AsExpr().appendStr(buf, tm, true, depth)

		case n.id0.IsXBinaryOp():
			if parenthesize {
				buf = append(buf, '(')
			}
			buf = n.lhs.AsExpr().appendStr(buf, tm, true, depth)
			buf = append(buf, opString(n.id0)...)
			if n.id0 == t.IDXBinaryAs {
				buf = append(buf, n.rhs.AsTypeExpr().Str(tm)...)
			} else {
				buf = n.rhs.AsExpr().appendStr(buf, tm, true, depth)
			}
			if parenthesize {
				buf = append(buf, ')')
			}

		case n.id0.IsXAssociativeOp():
			if parenthesize {
				buf = append(buf, '(')
			}
			op := opString(n.id0)
			for i, o := range n.list0 {
				if i != 0 {
					buf = append(buf, op...)
				}
				buf = o.AsExpr().appendStr(buf, tm, true, depth)
			}
			if parenthesize {
				buf = append(buf, ')')
			}
		}

	} else {
		switch n.id0 {
		case 0:
			if n.id1 != 0 {
				buf = append(buf, tm.ByID(n.id1)...)
				buf = append(buf, '.')
			}
			buf = append(buf, tm.ByID(n.id2)...)

		case t.IDOpenParen:
			buf = n.lhs.AsExpr().appendStr(buf, tm, true, depth)
			buf = append(buf, n.flags.AsEffect().String()...)
			buf = append(buf, '(')
			for i, o := range n.list0 {
				if i != 0 {
					buf = append(buf, ", "...)
				}
				buf = append(buf, tm.ByID(o.AsArg().Name())...)
				buf = append(buf, ": "...)
				buf = o.AsArg().Value().appendStr(buf, tm, false, depth)
			}
			buf = append(buf, ')')

		case t.IDOpenBracket:
			buf = n.lhs.AsExpr().appendStr(buf, tm, true, depth)
			buf = append(buf, '[')
			buf = n.rhs.AsExpr().appendStr(buf, tm, false, depth)
			buf = append(buf, ']')

		case t.IDDotDot:
			buf = n.lhs.AsExpr().appendStr(buf, tm, true, depth)
			buf = append(buf, '[')
			if n.mhs != nil {
				buf = n.mhs.AsExpr().appendStr(buf, tm, false, depth)
				buf = append(buf, ' ')
			}
			buf = append(buf, ".."...)
			if n.rhs != nil {
				buf = append(buf, ' ')
				buf = n.rhs.AsExpr().appendStr(buf, tm, false, depth)
			}
			buf = append(buf, ']')

		case t.IDDot:
			buf = n.lhs.AsExpr().appendStr(buf, tm, true, depth)
			buf = append(buf, '.')
			buf = append(buf, tm.ByID(n.id2)...)

		case t.IDComma:
			buf = append(buf, '[')
			for i, o := range n.list0 {
				if i != 0 {
					buf = append(buf, ", "...)
				}
				buf = o.AsExpr().appendStr(buf, tm, false, depth)
			}
			buf = append(buf, ']')
		}
	}

	return buf
}

func opString(x t.ID) string {
	if x < t.ID(len(opStrings)) {
		if s := opStrings[x]; s != "" {
			return s
		}
	}
	return " no_such_Wuffs_operator "
}

var opStrings = [...]string{
	t.IDXBinaryPlus:           " + ",
	t.IDXBinaryMinus:          " - ",
	t.IDXBinaryStar:           " * ",
	t.IDXBinarySlash:          " / ",
	t.IDXBinaryShiftL:         " << ",
	t.IDXBinaryShiftR:         " >> ",
	t.IDXBinaryAmp:            " & ",
	t.IDXBinaryPipe:           " | ",
	t.IDXBinaryHat:            " ^ ",
	t.IDXBinaryPercent:        " % ",
	t.IDXBinaryTildeModPlus:   " ~mod+ ",
	t.IDXBinaryTildeModMinus:  " ~mod- ",
	t.IDXBinaryTildeModStar:   " ~mod* ",
	t.IDXBinaryTildeModShiftL: " ~mod<< ",
	t.IDXBinaryTildeSatPlus:   " ~sat+ ",
	t.IDXBinaryTildeSatMinus:  " ~sat- ",
	t.IDXBinaryNotEq:          " <> ",
	t.IDXBinaryLessThan:       " < ",
	t.IDXBinaryLessEq:         " <= ",
	t.IDXBinaryEqEq:           " == ",
	t.IDXBinaryGreaterEq:      " >= ",
	t.IDXBinaryGreaterThan:    " > ",
	t.IDXBinaryAnd:            " and ",
	t.IDXBinaryOr:             " or ",
	t.IDXBinaryAs:             " as ",

	t.IDXAssociativePlus: " + ",
	t.IDXAssociativeStar: " * ",
	t.IDXAssociativeAmp:  " & ",
	t.IDXAssociativePipe: " | ",
	t.IDXAssociativeHat:  " ^ ",
	t.IDXAssociativeAnd:  " and ",
	t.IDXAssociativeOr:   " or ",

	t.IDXUnaryPlus:  "+",
	t.IDXUnaryMinus: "-",
	t.IDXUnaryNot:   "not ",
}

// Str returns a string form of n.
func (n *TypeExpr) Str(tm *t.Map) string {
	if n == nil {
		return "«nilTypeExpr»"
	}
	if n.Decorator() == 0 && n.Min() == nil && n.Max() == nil {
		return n.QID().Str(tm)
	}
	return string(n.appendStr(nil, tm, 0))
}

func (n *TypeExpr) appendStr(buf []byte, tm *t.Map, depth uint32) []byte {
	if depth > MaxTypeExprDepth {
		return append(buf, "!type_expr_recursion_depth_too_large!"...)
	}
	depth++
	if n == nil {
		return append(buf, "!invalid_type!"...)
	}

	switch dec := n.Decorator(); dec {
	case 0:
		buf = append(buf, n.QID().Str(tm)...)
	case t.IDNptr:
		buf = append(buf, "nptr "...)
		return n.Inner().appendStr(buf, tm, depth)
	case t.IDPtr:
		buf = append(buf, "ptr "...)
		return n.Inner().appendStr(buf, tm, depth)
	case t.IDArray, t.IDRoarray:
		if dec == t.IDRoarray {
			buf = append(buf, "ro"...)
		}
		buf = append(buf, "array["...)
		buf = n.ArrayLength().appendStr(buf, tm, false, 0)
		buf = append(buf, "] "...)
		return n.Inner().appendStr(buf, tm, depth)
	case t.IDRoslice, t.IDSlice:
		if dec == t.IDRoslice {
			buf = append(buf, "ro"...)
		}
		buf = append(buf, "slice "...)
		return n.Inner().appendStr(buf, tm, depth)
	case t.IDRotable, t.IDTable:
		if dec == t.IDRotable {
			buf = append(buf, "ro"...)
		}
		buf = append(buf, "table "...)
		return n.Inner().appendStr(buf, tm, depth)
	case t.IDFunc:
		buf = append(buf, "func "...)
		if r := n.Receiver(); r != nil {
			buf = append(buf, '(')
			buf = r.appendStr(buf, tm, depth)
			buf = append(buf, ")."...)
		}
		return append(buf, n.FuncName().Str(tm)...)
	default:
		return append(buf, "!invalid_type!"...)
	}
	if n.Min() != nil || n.Max() != nil {
		buf = append(buf, '[')
		if n.Min() != nil {
			buf = n.Min().appendStr(buf, tm, false, 0)
			buf = append(buf, ' ')
		}
		buf = append(buf, "..="...)
		if n.Max() != nil {
			buf = append(buf, ' ')
			buf = n.Max().appendStr(buf, tm, false, 0)
		}
		buf = append(buf, ']')
	}
	return buf
}
