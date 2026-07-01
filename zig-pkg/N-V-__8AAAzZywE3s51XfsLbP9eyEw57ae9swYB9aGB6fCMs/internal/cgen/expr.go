// Copyright 2017 The Wuffs Authors.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

package cgen

import (
	"fmt"
	"math/big"
	"strings"

	a "github.com/google/wuffs/lang/ast"
	t "github.com/google/wuffs/lang/token"
)

func (g *gen) writeExpr(b *buffer, n *a.Expr, sideEffectsOnly bool, depth uint32) error {
	if depth > a.MaxExprDepth {
		return fmt.Errorf("expression recursion depth too large")
	}
	depth++

	if cv := n.ConstValue(); cv != nil {
		if typ := n.MType(); typ.IsNumTypeOrIdeal() {
			b.writes(cv.String())
			if cv.Sign() >= 0 {
				b.writeb('u')
			}
		} else if typ.IsNullptr() {
			b.writes("NULL")
		} else if typ.IsStatus() {
			b.writes("wuffs_base__make_status(NULL)")
		} else if !typ.IsBool() {
			return fmt.Errorf("cannot generate C expression for %v constant of type %q", n.Str(g.tm), n.MType().Str(g.tm))
		} else if cv.Cmp(zero) == 0 {
			b.writes("false")
		} else if cv.Cmp(one) == 0 {
			b.writes("true")
		} else {
			return fmt.Errorf("%v has type bool but constant value %v is neither 0 or 1", n.Str(g.tm), cv)
		}
		return nil
	}

	switch op := n.Operator(); {
	case op.IsXUnaryOp():
		return g.writeExprUnaryOp(b, n, depth)
	case op.IsXBinaryOp():
		return g.writeExprBinaryOp(b, n, depth)
	case op.IsXAssociativeOp():
		return g.writeExprAssociativeOp(b, n, depth)
	}
	return g.writeExprOther(b, n, sideEffectsOnly, depth)
}

func (g *gen) writeExprOther(b *buffer, n *a.Expr, sideEffectsOnly bool, depth uint32) error {
	switch n.Operator() {
	case 0:
		if ident := n.Ident(); ident == t.IDThis {
			b.writes("self")

		} else if ident == t.IDCoroutineResumed {
			if g.currFunk.astFunc.Effect().Coroutine() {
				b.printf("(self->private_impl.%s%s != 0)",
					pPrefix, g.currFunk.astFunc.FuncName().Str(g.tm))
			} else {
				b.writes("false")
			}

		} else if ident.IsDQStrLiteral(g.tm) {
			if z := g.statusMap[t.QID{0, n.Ident()}]; z.cName != "" {
				b.writes("wuffs_base__make_status(")
				b.writes(z.cName)
				b.writes(")")
				return nil
			}
			return fmt.Errorf("unrecognized status %s", n.Str(g.tm))

		} else if c, ok := g.scalarConstsMap[t.QID{0, n.Ident()}]; ok {
			b.writes(c.Value().ConstValue().String())

		} else {
			if n.GlobalIdent() {
				b.writes(g.PKGPREFIX)
			} else {
				b.writes(vPrefix)
			}
			b.writes(ident.Str(g.tm))
		}
		return nil

	case t.IDOpenParen:
		// n is a function call.
		if err := g.writeBuiltinCall(b, n, sideEffectsOnly, depth); err != errNoSuchBuiltin {
			return err
		}

		if n.LHS().AsExpr().Ident() == t.IDReset {
			method := n.LHS().AsExpr()
			recv := method.LHS().AsExpr()
			recvTyp, addr := recv.MType(), "&"
			if p := recvTyp.Decorator(); p == t.IDNptr || p == t.IDPtr {
				recvTyp, addr = recvTyp.Inner(), ""
			}
			if recvTyp.Decorator() != 0 {
				return fmt.Errorf("cannot generate reset method call %q for receiver type %q",
					n.Str(g.tm), recv.MType().Str(g.tm))
			}
			qid := recvTyp.QID()

			b.printf("wuffs_private_impl__ignore_status("+
				"%s%s__initialize(%s", g.packagePrefix(qid), qid[1].Str(g.tm), addr)
			if err := g.writeExpr(b, recv, false, depth); err != nil {
				return err
			}
			b.printf(",\nsizeof (%s%s), WUFFS_VERSION, ", g.packagePrefix(qid), qid[1].Str(g.tm))
			if recv.IsThisDotFoo() != 0 {
				b.writes("WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED))")
			} else {
				b.writes("0))")
			}

			return nil
		}

		return g.writeExprUserDefinedCall(b, n, depth)

	case t.IDOpenBracket:
		// n is an index.
		if err := g.writeExpr(b, n.LHS().AsExpr(), false, depth); err != nil {
			return err
		}
		if lTyp := n.LHS().AsExpr().MType(); lTyp.IsEitherSliceType() {
			// TODO: don't assume that the slice is a slice of base.u8.
			b.writes(".ptr")
		}
		b.writeb('[')
		if err := g.writeExpr(b, n.RHS().AsExpr(), false, depth); err != nil {
			return err
		}
		b.writeb(']')
		return nil

	case t.IDDotDot:
		// n is a slice.
		//
		// TODO: don't assume that the slice is a slice of base.u8.
		lhs := n.LHS().AsExpr()
		mhs := n.MHS().AsExpr()
		rhs := n.RHS().AsExpr()

		comma := ", "
		if (mhs != nil) && (mhs.Operator() != 0) &&
			(rhs != nil) && (rhs.Operator() != 0) {
			comma = ",\n"
		}

		if lhs.MType().IsEitherArrayType() {
			if mhs == nil {
				b.writes("wuffs_base__make_slice_u8(")
			} else {
				b.writes("wuffs_base__make_slice_u8_ij(")
			}
			lhsIsReadOnly := lhs.MType().IsReadOnly()
			if lhsIsReadOnly {
				b.writes("wuffs_base__strip_const_from_u8_ptr(")
			}
			if err := g.writeExpr(b, lhs, false, depth); err != nil {
				return err
			}
			if lhsIsReadOnly {
				b.writes(")")
			}
			if mhs == nil {
				// No-op.
			} else {
				b.writes(comma)
				if mcv := mhs.ConstValue(); mcv != nil {
					b.writes(mcv.String())
				} else if err := g.writeExpr(b, mhs, false, depth); err != nil {
					return err
				}
			}
			b.writes(comma)
			if rhs == nil {
				b.writes(lhs.MType().ArrayLength().ConstValue().String())
			} else if rcv := rhs.ConstValue(); rcv != nil {
				b.writes(rcv.String())
			} else if err := g.writeExpr(b, rhs, false, depth); err != nil {
				return err
			}
			b.writes(")")
			return nil
		}

		switch {
		case mhs != nil && rhs == nil:
			b.writes("wuffs_base__slice_u8__subslice_i(")
		case mhs == nil && rhs != nil:
			b.writes("wuffs_base__slice_u8__subslice_j(")
		case mhs != nil && rhs != nil:
			b.writes("wuffs_base__slice_u8__subslice_ij(")
		}

		if err := g.writeExpr(b, lhs, false, depth); err != nil {
			return err
		}
		if mhs != nil {
			b.writes(comma)
			if err := g.writeExpr(b, mhs, false, depth); err != nil {
				return err
			}
		}
		if rhs != nil {
			b.writes(comma)
			if err := g.writeExpr(b, rhs, false, depth); err != nil {
				return err
			}
		}
		if mhs != nil || rhs != nil {
			b.writeb(')')
		}
		return nil

	case t.IDDot:
		lhs := n.LHS().AsExpr()
		if lhs.Ident() == t.IDArgs {
			b.writes(aPrefix)
			b.writes(n.Ident().Str(g.tm))
			return nil
		} else if (lhs.Operator() == 0) && n.Ident().IsDQStrLiteral(g.tm) {
			if z := g.statusMap[t.QID{lhs.Ident(), n.Ident()}]; z.cName != "" {
				b.writes("wuffs_base__make_status(")
				b.writes(z.cName)
				b.writes(")")
				return nil
			}
			return fmt.Errorf("unrecognized status %s", n.Str(g.tm))
		}

		if err := g.writeExpr(b, lhs, false, depth); err != nil {
			return err
		}
		if p := lhs.MType().Decorator(); p == t.IDNptr || p == t.IDPtr {
			b.writes("->")
		} else {
			b.writes(".")
		}

		b.writes(g.privateImplData(lhs.MType(), n.Ident()))
		b.writeb('.')
		b.writes(fPrefix)
		b.writes(n.Ident().Str(g.tm))
		return nil
	}
	return fmt.Errorf("unrecognized token (0x%X) for writeExprOther", n.Operator())
}

func (g *gen) privateImplData(typ *a.TypeExpr, fieldName t.ID) string {
	if p := typ.Decorator(); p == t.IDNptr || p == t.IDPtr {
		typ = typ.Inner()
	}
	if s := g.structMap[typ.QID()]; s != nil {
		qid := s.QID()
		if _, ok := g.privateDataFields[t.QQID{qid[0], qid[1], fieldName}]; ok {
			return "private_data"
		}
	}
	return "private_impl"
}

func (g *gen) writeExprUnaryOp(b *buffer, n *a.Expr, depth uint32) error {
	op := n.Operator()
	opName := cOpName(op)
	if opName == "" {
		return fmt.Errorf("unrecognized operator %q", op.AmbiguousForm().Str(g.tm))
	}

	b.writes(opName)
	return g.writeExpr(b, n.RHS().AsExpr(), false, depth)
}

func (g *gen) writeExprBinaryOp(b *buffer, n *a.Expr, depth uint32) error {
	opName, lhsCast, overallCast := "", false, n.MType().IsSmallInteger()

	op := n.Operator()
	switch op {
	case t.IDXBinaryTildeSatPlus, t.IDXBinaryTildeSatMinus:
		uBits := uintBits(n.MType().QID())
		if uBits == 0 {
			return fmt.Errorf("unsupported tilde-operator type %q", n.MType().Str(g.tm))
		}
		uOp := "add"
		if op != t.IDXBinaryTildeSatPlus {
			uOp = "sub"
		}
		b.printf("wuffs_base__u%d__sat_%s", uBits, uOp)
		opName = ", "

	case t.IDXBinaryAs:
		return g.writeExprAs(b, n.LHS().AsExpr(), n.RHS().AsTypeExpr(), depth)

	case t.IDXBinaryTildeModPlus, t.IDXBinaryTildeModMinus, t.IDXBinaryTildeModStar:
		overallCast = true

	case t.IDXBinaryTildeModShiftL:
		overallCast = true
		fallthrough

	case t.IDXBinaryShiftL, t.IDXBinaryShiftR:
		if lhs := n.LHS().AsExpr(); lhs.ConstValue() != nil {
			lhsCast = true
		}
	}

	if opName == "" {
		opName = cOpName(op)
		if opName == "" {
			return fmt.Errorf("unrecognized operator %q", op.AmbiguousForm().Str(g.tm))
		}
	}

	b.writeb('(')
	if overallCast {
		b.writeb('(')
		if err := g.writeCTypeName(b, n.MType(), "", ""); err != nil {
			return err
		}
		b.writes(")(")
	}

	if lhsCast {
		b.writes("((")
		if err := g.writeCTypeName(b, n.LHS().AsExpr().MType(), "", ""); err != nil {
			return err
		}
		b.writes(")(")
	}
	if err := g.writeExprRepr(b, n.LHS().AsExpr(), depth); err != nil {
		return err
	}
	if lhsCast {
		b.writes("))")
	}

	b.writes(opName)

	if err := g.writeExprRepr(b, n.RHS().AsExpr(), depth); err != nil {
		return err
	}

	if overallCast {
		b.writeb(')')
	}
	b.writeb(')')
	return nil
}

func (g *gen) writeExprRepr(b *buffer, n *a.Expr, depth uint32) error {
	isStatus := n.MType().IsStatus()
	if isStatus {
		if op := n.Operator(); ((op == 0) || (op == a.ExprOperatorSelector)) && n.Ident().IsDQStrLiteral(g.tm) {
			qid := t.QID{0, n.Ident()}
			if op == t.IDDot {
				qid[0] = n.LHS().AsExpr().Ident()
			}
			if z := g.statusMap[qid]; z.cName != "" {
				b.writes(z.cName)
				return nil
			}
		}
	}
	if err := g.writeExpr(b, n, false, depth); err != nil {
		return err
	}
	if isStatus {
		b.writes(".repr")
	}
	return nil
}

func (g *gen) writeExprXMinusY(b *buffer, x *a.Expr, y *a.Expr, depth uint32) error {
	if x.Operator() == t.IDXBinaryPlus {
		if x.LHS().AsExpr().Eq(y) {
			return g.writeExpr(b, x.RHS().AsExpr(), false, depth)
		} else if x.RHS().AsExpr().Eq(y) {
			return g.writeExpr(b, x.LHS().AsExpr(), false, depth)
		}
	}

	b.writes("(")
	if err := g.writeExpr(b, x, false, depth); err != nil {
		return err
	}
	b.writes(" - ")
	if err := g.writeExpr(b, y, false, depth); err != nil {
		return err
	}
	b.writes(")")
	return nil
}

func (g *gen) writeExprAs(b *buffer, lhs *a.Expr, rhs *a.TypeExpr, depth uint32) error {
	if rhs.IsPointerType() {
		if !lhs.MType().IsEitherSliceType() {
			// TODO: fix this.
			return fmt.Errorf("cannot convert Wuffs conversion from %q to %q to C", lhs.Str(g.tm), rhs.Str(g.tm))
		}
		if (lhs.Operator() == t.IDDotDot) && (lhs.MHS() == nil) && (lhs.RHS() == nil) {
			b.writes("&")
			if err := g.writeExpr(b, lhs.LHS().AsExpr(), false, depth); err != nil {
				return err
			}
			b.writes("[0u]")
			return nil
		}
		if err := g.writeExpr(b, lhs, false, depth); err != nil {
			return err
		}
		b.writes(".ptr")
		return nil
	}

	// Drop the "& redundantMask" in "(foo & redundantMask) as base.uxx". It's
	// redundant in C/C++ (but not in Wuffs).
	if rhs.IsNumType() {
		redundantMask := (*big.Int)(nil)
		switch rhs.QID()[1] {
		case t.IDU8:
			redundantMask = maxUint8
		case t.IDU16:
			redundantMask = maxUint16
		case t.IDU32:
			redundantMask = maxUint32

			// We don't have a case for t.IDU64. the "& redundantMask" in Wuffs
			// is typically for a wide-to-narrow conversion and base.u64 is
			// already the widest type.
		}

		if (redundantMask == nil) || (lhs.Operator() != t.IDXBinaryAmp) {
			// No-op.
		} else if lcv := lhs.LHS().AsExpr().ConstValue(); (lcv != nil) && (lcv.Cmp(redundantMask) == 0) {
			lhs = lhs.RHS().AsExpr()
		} else if rcv := lhs.RHS().AsExpr().ConstValue(); (rcv != nil) && (rcv.Cmp(redundantMask) == 0) {
			lhs = lhs.LHS().AsExpr()
		}
	}

	b.writes("((")
	// TODO: watch for passing an array type to writeCTypeName? In C, an array
	// type can decay into a pointer.
	if err := g.writeCTypeName(b, rhs, "", ""); err != nil {
		return err
	}
	b.writes(")(")
	if err := g.writeExpr(b, lhs, false, depth); err != nil {
		return err
	}
	b.writes("))")
	return nil
}

func (g *gen) writeExprAssociativeOp(b *buffer, n *a.Expr, depth uint32) error {
	op := n.Operator()
	opName := cOpName(op)
	if opName == "" {
		return fmt.Errorf("unrecognized operator %q", op.AmbiguousForm().Str(g.tm))
	}
	if len(n.Args()) > 3 {
		opName = strings.TrimRight(opName, " ") + "\n"
	}

	b.writeb('(')
	for i, o := range n.Args() {
		if i != 0 {
			b.writes(opName)
		}
		if err := g.writeExpr(b, o.AsExpr(), false, depth); err != nil {
			return err
		}
	}
	b.writeb(')')
	return nil
}

func (g *gen) writeExprUserDefinedCall(b *buffer, n *a.Expr, depth uint32) error {
	method := n.LHS().AsExpr()
	recv := method.LHS().AsExpr()
	recvTyp, addr := recv.MType(), "&"
	if p := recvTyp.Decorator(); p == t.IDNptr || p == t.IDPtr {
		recvTyp, addr = recvTyp.Inner(), ""
	}
	if recvTyp.Decorator() != 0 {
		return fmt.Errorf("cannot generate user-defined method call %q for receiver type %q",
			n.Str(g.tm), recv.MType().Str(g.tm))
	}
	qid := recvTyp.QID()
	b.printf("%s%s__%s(", g.packagePrefix(qid), qid[1].Str(g.tm), method.Ident().Str(g.tm))
	if !recvTyp.IsEtcUtilityType() {
		b.writes(addr)
		if err := g.writeExpr(b, recv, false, depth); err != nil {
			return err
		}
		if len(n.Args()) > 0 {
			b.writes(", ")
		}
	}
	return g.writeArgs(b, n.Args(), depth)
}

func (g *gen) writeCTypeName(b *buffer, n *a.TypeExpr, varNamePrefix string, varName string) error {
	// It may help to refer to http://unixwiz.net/techtips/reading-cdecl.html

	// TODO: fix this, allow slices of all types, not just of base.u8's. Also
	// allow arrays of slices, slices of pointers, etc.
	if n.IsEitherSliceType() {
		o := n.Inner()
		if o.Decorator() == 0 && o.QID() == (t.QID{t.IDBase, t.IDU8}) && !o.IsRefined() {
			b.writes("wuffs_base__slice_u8")
			if varNamePrefix != "" {
				b.writeb(' ')
				b.writes(varNamePrefix)
				b.writes(varName)
			}
			return nil
		}
		return fmt.Errorf("cannot convert Wuffs type %q to C", n.Str(g.tm))
	}
	if n.IsEitherTableType() {
		o := n.Inner()
		if o.Decorator() == 0 && o.QID() == (t.QID{t.IDBase, t.IDU8}) && !o.IsRefined() {
			b.writes("wuffs_base__table_u8")
			if varNamePrefix != "" {
				b.writeb(' ')
				b.writes(varNamePrefix)
				b.writes(varName)
			}
			return nil
		}
		return fmt.Errorf("cannot convert Wuffs type %q to C", n.Str(g.tm))
	}

	// maxNumPointers is an arbitrary implementation restriction.
	const maxNumPointers = 16

	x := n
	for ; x != nil && x.IsEitherArrayType(); x = x.Inner() {
	}

	numPointers, innermost, isPointerToArray := 0, x, false
	if x.IsPointerType() && x.Inner().IsEitherArrayType() && x.Inner().IsBulkNumType() {
		isPointerToArray = true
		innermost = x.Inner()
		for ; innermost != nil && innermost.Inner() != nil; innermost = innermost.Inner() {
		}
	} else {
		for ; innermost != nil && innermost.Inner() != nil; innermost = innermost.Inner() {
			if innermost.IsPointerType() {
				if numPointers == maxNumPointers {
					return fmt.Errorf("cannot convert Wuffs type %q to C: too many ptr's", n.Str(g.tm))
				}
				numPointers++
				continue
			}
			// TODO: fix this.
			return fmt.Errorf("cannot convert Wuffs type %q to C", n.Str(g.tm))
		}
	}

	fallback := true
	if qid := innermost.QID(); qid[0] == t.IDBase {
		if key := qid[1]; key < t.ID(len(cTypeNames)) {
			if s := cTypeNames[key]; s != "" {
				b.writes(s)
				fallback = false
			}
		}
	}
	if fallback {
		qid := innermost.QID()
		b.printf("%s%s", g.packagePrefix(qid), qid[1].Str(g.tm))
	}

	if isPointerToArray {
		if x.Inner().Inner().IsNumType() {
			b.writes("*")
		} else {
			b.writes("(*")
		}
	} else {
		for i := 0; i < numPointers; i++ {
			b.writeb('*')
		}
	}

	if varNamePrefix != "" {
		b.writeb(' ')
		b.writes(varNamePrefix)
		b.writes(varName)
	}

	x = n
	if isPointerToArray {
		x = x.Inner().Inner()
		if !x.IsNumType() {
			b.writes(")")
		}
	}
	for ; x != nil && x.IsEitherArrayType(); x = x.Inner() {
		b.writeb('[')
		b.writes(x.ArrayLength().ConstValue().String())
		b.writeb(']')
	}

	return nil
}

func (g *gen) packagePrefix(qid t.QID) string {
	if qid[0] != 0 {
		otherPkg := g.tm.ByID(qid[0])
		// TODO: map the "deflate" in "deflate.decoder" to the "deflate" in
		// `use "std/deflate"`, and use the latter "deflate".
		//
		// This is pretty academic at the moment, since they're the same
		// "deflate", but in the future, we might be able to rename used
		// packages, e.g. `use "foo/bar" as "baz"`, so "baz.qux" would map
		// to generating "wuffs_bar__qux".
		//
		// TODO: sanitize or validate otherPkg, e.g. that it's ASCII only?
		//
		// See gen.writeInitializerImpl for a similar use of otherPkg.
		return "wuffs_" + otherPkg + "__"
	}
	return g.pkgPrefix
}

func isBaseRangeType(qid t.QID) bool {
	if qid[0] == t.IDBase {
		switch qid[1] {
		case t.IDRangeIEU32, t.IDRangeIIU32, t.IDRangeIEU64, t.IDRangeIIU64:
			return true
		}
	}
	return false
}

var cTypeNames = [...]string{
	t.IDI8:   "int8_t",
	t.IDI16:  "int16_t",
	t.IDI32:  "int32_t",
	t.IDI64:  "int64_t",
	t.IDU8:   "uint8_t",
	t.IDU16:  "uint16_t",
	t.IDU32:  "uint32_t",
	t.IDU64:  "uint64_t",
	t.IDBool: "bool",

	t.IDIOReader:    "wuffs_base__io_buffer*",
	t.IDIOWriter:    "wuffs_base__io_buffer*",
	t.IDTokenReader: "wuffs_base__token_buffer*",
	t.IDTokenWriter: "wuffs_base__token_buffer*",

	t.IDARMCRC32U32:  "uint32_t",
	t.IDARMNeonU8x8:  "uint8x8_t",
	t.IDARMNeonU16x4: "uint16x4_t",
	t.IDARMNeonU32x2: "uint32x2_t",
	t.IDARMNeonU64x1: "uint64x1_t",
	t.IDARMNeonU8x16: "uint8x16_t",
	t.IDARMNeonU16x8: "uint16x8_t",
	t.IDARMNeonU32x4: "uint32x4_t",
	t.IDARMNeonU64x2: "uint64x2_t",
	t.IDX86M128I:     "__m128i",
	t.IDX86M256I:     "__m256i",
}

const noSuchCOperator = " no_such_C_operator "

func cOpName(x t.ID) string {
	if x < t.ID(len(cOpNames)) {
		if s := cOpNames[x]; s != "" {
			return s
		}
	}
	return noSuchCOperator
}

var cOpNames = [...]string{
	t.IDPlusEq:           " += ",
	t.IDMinusEq:          " -= ",
	t.IDStarEq:           " *= ",
	t.IDSlashEq:          " /= ",
	t.IDShiftLEq:         " <<= ",
	t.IDShiftREq:         " >>= ",
	t.IDAmpEq:            " &= ",
	t.IDPipeEq:           " |= ",
	t.IDHatEq:            " ^= ",
	t.IDPercentEq:        " %= ",
	t.IDTildeModPlusEq:   " += ",
	t.IDTildeModMinusEq:  " -= ",
	t.IDTildeModStarEq:   " *= ",
	t.IDTildeModShiftLEq: " <<= ",
	t.IDTildeSatPlusEq:   noSuchCOperator,
	t.IDTildeSatMinusEq:  noSuchCOperator,

	t.IDEq:         " = ",
	t.IDEqQuestion: " = ",

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
	t.IDXBinaryTildeModPlus:   " + ",
	t.IDXBinaryTildeModMinus:  " - ",
	t.IDXBinaryTildeModStar:   " * ",
	t.IDXBinaryTildeModShiftL: " << ",
	t.IDXBinaryTildeSatPlus:   noSuchCOperator,
	t.IDXBinaryTildeSatMinus:  noSuchCOperator,
	t.IDXBinaryNotEq:          " != ",
	t.IDXBinaryLessThan:       " < ",
	t.IDXBinaryLessEq:         " <= ",
	t.IDXBinaryEqEq:           " == ",
	t.IDXBinaryGreaterEq:      " >= ",
	t.IDXBinaryGreaterThan:    " > ",
	t.IDXBinaryAnd:            " && ",
	t.IDXBinaryOr:             " || ",
	t.IDXBinaryAs:             noSuchCOperator,

	t.IDXAssociativePlus: " + ",
	t.IDXAssociativeStar: " * ",
	t.IDXAssociativeAmp:  " & ",
	t.IDXAssociativePipe: " | ",
	t.IDXAssociativeHat:  " ^ ",
	t.IDXAssociativeAnd:  " && ",
	t.IDXAssociativeOr:   " || ",

	t.IDXUnaryPlus:  " + ",
	t.IDXUnaryMinus: " - ",
	t.IDXUnaryNot:   " ! ",
}
