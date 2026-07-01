// Copyright 2017 The Wuffs Authors.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

package check

import (
	"fmt"
	"math/big"

	a "github.com/google/wuffs/lang/ast"
	t "github.com/google/wuffs/lang/token"
)

type cpuArchBits uint32

const (
	cpuArchBitsARMCRC32 = cpuArchBits(0x00000001)
	cpuArchBitsARMNeon  = cpuArchBits(0x00000002)
	cpuArchBitsX86SSE42 = cpuArchBits(0x00000004)
	cpuArchBitsX86AVX2  = cpuArchBits(0x00000008)
)

func calcCPUArchBits(n *a.Func) (ret cpuArchBits) {
	for _, o := range n.Asserts() {
		o := o.AsAssert()
		if !o.IsChooseCPUArch() {
			continue
		}
		switch o.Condition().RHS().AsExpr().Ident() {
		case t.IDARMCRC32:
			ret |= cpuArchBitsARMCRC32
		case t.IDARMNeon:
			ret |= cpuArchBitsARMNeon
		case t.IDX86SSE42:
			ret |= cpuArchBitsX86SSE42
		case t.IDX86AVX2:
			ret |= cpuArchBitsX86SSE42 | cpuArchBitsX86AVX2
		}
	}
	return ret
}

func (q *checker) tcheckCPUArchBits(cab cpuArchBits, typ *a.TypeExpr) error {
	if qid := typ.Innermost().QID(); (qid[0] == t.IDBase) && qid[1].IsBuiltInCPUArch() {
		need := cpuArchBits(0)
		switch qid[1] {
		case t.IDARMCRC32Utility, t.IDARMCRC32U32:
			need = cpuArchBitsARMCRC32
		case t.IDARMNeonUtility,
			t.IDARMNeonU8x8, t.IDARMNeonU16x4, t.IDARMNeonU32x2, t.IDARMNeonU64x1,
			t.IDARMNeonU8x16, t.IDARMNeonU16x8, t.IDARMNeonU32x4, t.IDARMNeonU64x2:
			need = cpuArchBitsARMNeon
		case t.IDX86SSE42Utility, t.IDX86M128I:
			need = cpuArchBitsX86SSE42
		}
		if (cab & need) != need {
			return fmt.Errorf("check: missing cpu_arch for %q", typ.Innermost().Str(q.tm))
		}
	}
	return nil
}

func (q *checker) tcheckVars(cab cpuArchBits, block []*a.Node) error {
	for _, o := range block {
		if o.Kind() != a.KVar {
			break
		}

		q.errFilename, q.errLine = o.AsRaw().FilenameLine()

		o := o.AsVar()
		name := o.Name()
		if _, ok := q.localVars[name]; ok {
			return fmt.Errorf("check: duplicate var %q", name.Str(q.tm))
		} else if q.c.topLevelNames[name] != 0 {
			return &Error{
				Err:      fmt.Errorf("check: var %q shadows top level name", name.Str(q.tm)),
				Filename: o.Filename(),
				Line:     o.Line(),
			}
		}
		if err := q.tcheckTypeExpr(o.XType(), 0); err != nil {
			return err
		}
		if err := q.tcheckCPUArchBits(cab, o.XType()); err != nil {
			return err
		}
		q.localVars[name] = o.XType()
	}
	return nil
}

func (q *checker) tcheckStatement(n *a.Node) error {
	q.errFilename, q.errLine = n.AsRaw().FilenameLine()

	switch n.Kind() {
	case a.KAssert:
		if err := q.tcheckAssert(n.AsAssert()); err != nil {
			return err
		}

	case a.KAssign:
		if err := q.tcheckAssign(n.AsAssign()); err != nil {
			return err
		}

	case a.KChoose:
		if err := q.tcheckChoose(n.AsChoose()); err != nil {
			return err
		}

	case a.KIf:
		for n := n.AsIf(); n != nil; n = n.ElseIf() {
			if (n.ElseIf() != nil) && (len(n.BodyIfFalse()) > 0) {
				return fmt.Errorf("check: if has an else-if and an else-not-if")
			}
		}
		for n := n.AsIf(); n != nil; n = n.ElseIf() {
			cond := n.Condition()
			if cond.Effect() != 0 {
				return fmt.Errorf("check: internal error: if-condition is not effect-free")
			}
			if err := q.tcheckExpr(cond, 0); err != nil {
				return err
			}
			if !cond.MType().IsBool() {
				return fmt.Errorf("check: if condition %q, of type %q, does not have a boolean type",
					cond.Str(q.tm), cond.MType().Str(q.tm))
			}
			for _, o := range n.BodyIfTrue() {
				if err := q.tcheckStatement(o); err != nil {
					return err
				}
			}
			for _, o := range n.BodyIfFalse() {
				if err := q.tcheckStatement(o); err != nil {
					return err
				}
			}
		}
		for n := n.AsIf(); n != nil; n = n.ElseIf() {
			setPlaceholderMBoundsMType(n.AsNode())
		}
		return nil

	case a.KIOManip:
		n := n.AsIOManip()
		if err := q.tcheckExpr(n.IO(), 0); err != nil {
			return err
		}
		if typ := n.IO().MType(); !typ.IsIOType() {
			return fmt.Errorf("check: %s expression %q, of type %q, does not have an I/O type",
				n.Keyword().Str(q.tm), n.IO().Str(q.tm), typ.Str(q.tm))
		}

		arg1Typ := (*a.TypeExpr)(nil)
		switch n.Keyword() {
		case t.IDIOBind:
			arg1Typ = typeExprRosliceU8
			if n.IO().MType().Eq(typeExprIOWriter) {
				arg1Typ = typeExprSliceU8
			}
		case t.IDIOForgetHistory:
			// No-op.
		case t.IDIOLimit:
			arg1Typ = typeExprU64
		}
		if arg1Typ != nil {
			if err := q.tcheckExpr(n.Arg1(), 0); err != nil {
				return err
			}
			if typ := n.Arg1().MType(); !arg1Typ.EqIgnoringRefinementsLHSReadOnly(typ) {
				return fmt.Errorf("check: %s expression %q, of type %q, does not have type %q",
					n.Keyword().Str(q.tm), n.Arg1().Str(q.tm), typ.Str(q.tm), arg1Typ.Str(q.tm))
			}
		}

		if n.Keyword() == t.IDIOBind {
			if err := q.tcheckExpr(n.HistoryPosition(), 0); err != nil {
				return err
			}
			if typ := n.HistoryPosition().MType(); !typ.IsIdeal() && !typ.EqIgnoringRefinements(typeExprU64) {
				return fmt.Errorf("check: %s expression %q, of type %q, does not have type %q",
					n.Keyword().Str(q.tm), n.HistoryPosition().Str(q.tm), typ.Str(q.tm), typeExprU64.Str(q.tm))
			}
		}

		for _, o := range n.Body() {
			// TODO: prohibit jumps (breaks, continues), rets (returns, yields)
			// and retry-calling ? methods while inside an io_bind body.
			if err := q.tcheckStatement(o); err != nil {
				return err
			}
		}

	case a.KIterate:
		for n := n.AsIterate(); n != nil; n = n.ElseIterate() {
			if err := q.tcheckExpr(n.UnrollAsExpr(), 0); err != nil {
				return err
			}
			for _, o := range n.Assigns() {
				if err := q.tcheckStatement(o); err != nil {
					return err
				}
				o := o.AsAssign()
				if typ := o.LHS().MType(); !typ.IsEitherSliceType() {
					return fmt.Errorf("check: iterate assignment to %q, of type %q, does not have slice type",
						o.LHS().Str(q.tm), typ.Str(q.tm))
				}
			}
			// TODO: prohibit jumps (breaks, continues), rets (returns, yields) and
			// retry-calling ? methods while inside an iterate body.
			if err := q.tcheckLoop(n); err != nil {
				return err
			}
		}
		for n := n.AsIterate(); n != nil; n = n.ElseIterate() {
			setPlaceholderMBoundsMType(n.AsNode())
		}
		return nil

	case a.KJump:
		// No-op.

	case a.KRet:
		n := n.AsRet()
		lTyp := q.astFunc.Out()
		if q.astFunc.Effect().Coroutine() {
			lTyp = typeExprStatus
		} else if lTyp == nil {
			lTyp = typeExprEmptyStruct
		}
		value := n.Value()
		if err := q.tcheckExpr(value, 0); err != nil {
			return err
		}
		rTyp := value.MType()
		if !(rTyp.IsIdeal() && lTyp.IsNumType()) && !lTyp.EqIgnoringRefinementsLHSReadOnly(rTyp) {
			return fmt.Errorf("check: cannot return %q (of type %q) as type %q",
				value.Str(q.tm), rTyp.Str(q.tm), lTyp.Str(q.tm))
		}
		if rTyp.IsEitherSliceType() || rTyp.IsPointerType() {
			// TODO: just reject all "return x" when x has slice or pointer
			// type, after removing the (deprecated) lzw.decoder.flush! method.
			ok := false
			if value.Operator() != t.IDDotDot {
				// No-op.
			} else if lhs := value.LHS().AsExpr(); lhs.Operator() != t.IDDot {
				// No-op.
			} else if llhs := lhs.LHS().AsExpr(); true {
				ok = (llhs.Operator() == 0) && (llhs.Ident() == t.IDThis)
			}
			if !ok {
				return fmt.Errorf("check: cannot return %q, need something of the form this.etc[i .. j]",
					value.Str(q.tm))
			}
		}

	case a.KVar:
		n := n.AsVar()
		if n.XType().AsNode().MType() == nil {
			return fmt.Errorf("check: internal error: unchecked type expression %q", n.XType().Str(q.tm))
		}
		// TODO: check that the default zero value is assignable to n.XType().

	case a.KWhile:
		n := n.AsWhile()
		cond := n.Condition()
		if cond.Effect() != 0 {
			return fmt.Errorf("check: internal error: while-condition is not effect-free")
		}
		if err := q.tcheckExpr(cond, 0); err != nil {
			return err
		}
		if !cond.MType().IsBool() {
			return fmt.Errorf("check: for-loop condition %q, of type %q, does not have a boolean type",
				cond.Str(q.tm), cond.MType().Str(q.tm))
		}
		if err := q.tcheckLoop(n); err != nil {
			return err
		}

	default:
		return fmt.Errorf("check: unrecognized ast.Kind (%s) for tcheckStatement", n.Kind())
	}

	setPlaceholderMBoundsMType(n)
	return nil
}

func (q *checker) tcheckFuncAssert(n *a.Assert) error {
	if n.IsChooseCPUArch() {
		cond := n.Condition()
		cond.SetMType(typeExprBool)
		cond.LHS().AsExpr().SetMType(typeExprU32)
		cond.RHS().AsExpr().SetMType(typeExprU32)
		return nil
	}
	return fmt.Errorf("check: function assertions are not supported yet")
}

func (q *checker) tcheckAssert(n *a.Assert) error {
	cond := n.Condition()
	if err := q.tcheckExpr(cond, 0); err != nil {
		return err
	}
	if !cond.MType().IsBool() {
		return fmt.Errorf("check: assert condition %q, of type %q, does not have a boolean type",
			cond.Str(q.tm), cond.MType().Str(q.tm))
	}
	for _, o := range n.Args() {
		if err := q.tcheckExpr(o.AsArg().Value(), 0); err != nil {
			return err
		}
		setPlaceholderMBoundsMType(o)
	}
	// TODO: check that there are no side effects.
	return nil
}

func (q *checker) tcheckEq(lID t.ID, lhs *a.Expr, lTyp *a.TypeExpr, rhs *a.Expr, rTyp *a.TypeExpr) error {
	if (rTyp.IsIdeal() && lTyp.IsNumType()) ||
		(lTyp.EqIgnoringRefinementsLHSReadOnly(rTyp)) ||
		(rTyp.IsNullptr() && lTyp.Decorator() == t.IDNptr) {
		return nil
	}
	lStr := "???"
	if lID != 0 {
		lStr = lID.Str(q.tm)
	} else if lhs != nil {
		lStr = lhs.Str(q.tm)
	}
	return fmt.Errorf("check: cannot assign %q of type %q to %q of type %q",
		rhs.Str(q.tm), rTyp.Str(q.tm), lStr, lTyp.Str(q.tm))
}

func (q *checker) tcheckAssign(n *a.Assign) error {
	rhs := n.RHS()
	if err := q.tcheckExpr(rhs, 0); err != nil {
		return err
	}
	lhs := n.LHS()
	if lhs == nil {
		return nil
	}
	if err := q.tcheckExpr(lhs, 0); err != nil {
		return err
	}
	for l := lhs; l != nil; l = l.LHS().AsExpr() {
		if l.Operator() != t.IDOpenBracket {
			// No-op.
		} else if lTyp := l.LHS().MType(); lTyp.IsRecursivelyReadOnly() {
			return fmt.Errorf("check: assignment %q: assignee fragment %q, of type %q, has read-only type",
				n.Operator().Str(q.tm), l.LHS().AsExpr().Str(q.tm), lTyp.Str(q.tm))
		}
	}
	lTyp := lhs.MType()
	rTyp := rhs.MType()

	if op := n.Operator(); op == t.IDEq || op == t.IDEqQuestion {
		if (op == t.IDEqQuestion) && !rTyp.Eq(typeExprStatus) {
			return fmt.Errorf("check: =? only works with the base.status type")
		}
		return q.tcheckEq(0, lhs, lTyp, rhs, rTyp)
	}

	if !lTyp.IsNumType() {
		return fmt.Errorf("check: assignment %q: assignee %q, of type %q, does not have numeric type",
			n.Operator().Str(q.tm), lhs.Str(q.tm), lTyp.Str(q.tm))
	}

	switch n.Operator() {
	case t.IDShiftLEq, t.IDShiftREq, t.IDTildeModShiftLEq:
		if !rTyp.IsNumTypeOrIdeal() {
			return fmt.Errorf("check: assignment %q: shift %q, of type %q, does not have numeric type",
				n.Operator().Str(q.tm), rhs.Str(q.tm), rTyp.Str(q.tm))
		}
		return nil

	case t.IDTildeModPlusEq, t.IDTildeModMinusEq, t.IDTildeModStarEq,
		t.IDTildeSatPlusEq, t.IDTildeSatMinusEq:

		if !lTyp.IsUnsignedInteger() {
			return fmt.Errorf("check: assignment %q: %q, of type %q, does not have unsigned integer type",
				n.Operator().Str(q.tm), lhs.Str(q.tm), lTyp.Str(q.tm))
		}
	}

	if !(rTyp.IsIdeal() && lTyp.IsNumType()) && !lTyp.EqIgnoringRefinementsLHSReadOnly(rTyp) {
		return fmt.Errorf("check: assignment %q: %q and %q, of types %q and %q, do not have compatible types",
			n.Operator().Str(q.tm),
			lhs.Str(q.tm), rhs.Str(q.tm),
			lTyp.Str(q.tm), rTyp.Str(q.tm),
		)
	}
	return nil
}

func (q *checker) tcheckLoop(n a.Loop) error {
	for _, o := range n.Asserts() {
		if err := q.tcheckAssert(o.AsAssert()); err != nil {
			return err
		}
		setPlaceholderMBoundsMType(o)
	}
	for _, o := range n.Body() {
		if err := q.tcheckStatement(o); err != nil {
			return err
		}
	}
	return nil
}

func (q *checker) tcheckExpr(n *a.Expr, depth uint32) error {
	if depth > a.MaxExprDepth {
		return fmt.Errorf("check: expression recursion depth too large")
	}
	depth++

	if n.MType() != nil {
		return nil
	}

	switch op := n.Operator(); {
	case op.IsXUnaryOp():
		return q.tcheckExprUnaryOp(n, depth)
	case op.IsXBinaryOp():
		return q.tcheckExprBinaryOp(n, depth)
	case op.IsXAssociativeOp():
		return q.tcheckExprAssociativeOp(n, depth)
	}
	return q.tcheckExprOther(n, depth)
}

func (q *checker) tcheckExprOther(n *a.Expr, depth uint32) error {
	switch n.Operator() {
	case 0:
		id1 := n.Ident()
		if id1.IsNumLiteral(q.tm) {
			z := big.NewInt(0)
			s := id1.Str(q.tm)
			if _, ok := z.SetString(s, 0); !ok {
				return fmt.Errorf("check: invalid numeric literal %q", s)
			}
			n.SetConstValue(z)
			n.SetMType(typeExprIdeal)
			return nil

		} else if id1.IsSQStrLiteral(q.tm) {
			s := id1.Str(q.tm)
			unescaped, ok := t.Unescape(id1.Str(q.tm))
			if !ok {
				return fmt.Errorf("check: invalid '-string literal %q", s)
			}

			z := big.NewInt(0)
			i, iEnd, iDelta := 0, len(unescaped), +1 // Big-endian.
			if (len(s) > 2) && (s[len(s)-2] == 'l') {
				i, iEnd, iDelta = len(unescaped)-1, -1, -1 // Little-endian.
			}
			for ; i != iEnd; i += iDelta {
				z.Lsh(z, 8)
				z.Or(z, big.NewInt(int64(unescaped[i])))
			}

			n.SetConstValue(z)
			n.SetMType(typeExprIdeal)
			return nil

		} else if id1.IsDQStrLiteral(q.tm) {
			if _, ok := q.c.statuses[t.QID{0, n.Ident()}]; !ok {
				return fmt.Errorf("check: unrecognized status %s", n.Ident().Str(q.tm))
			}
			n.SetMType(typeExprStatus)
			return nil

		} else if id1.IsIdent(q.tm) {
			if q.localVars != nil {
				if typ, ok := q.localVars[id1]; ok {
					n.SetMType(typ)
					return nil
				}
			}
			if q.c.topLevelNames[id1] == a.KUse {
				n.SetConstValue(zero)
				n.SetMType(typeExprPackage)
				return nil
			}
			return q.tcheckExprXDotY(n, 0, id1)
		}

		switch id1 {
		case t.IDFalse:
			n.SetConstValue(zero)
			n.SetMType(typeExprBool)
			return nil

		case t.IDTrue:
			n.SetConstValue(one)
			n.SetMType(typeExprBool)
			return nil

		case t.IDNothing:
			n.SetConstValue(zero)
			n.SetMType(typeExprEmptyStruct)
			return nil

		case t.IDNullptr:
			n.SetConstValue(zero)
			n.SetMType(typeExprNullptr)
			return nil

		case t.IDOk:
			n.SetConstValue(zero)
			n.SetMType(typeExprStatus)
			return nil
		}

	case t.IDOpenParen:
		// n is a function call.
		return q.tcheckExprCall(n, depth)

	case t.IDOpenBracket:
		// n is an index.
		lhs := n.LHS().AsExpr()
		if err := q.tcheckExpr(lhs, depth); err != nil {
			return err
		}
		rhs := n.RHS().AsExpr()
		if err := q.tcheckExpr(rhs, depth); err != nil {
			return err
		}
		lTyp := lhs.MType()
		if lTyp.IsEitherArrayType() || lTyp.IsEitherSliceType() {
			// No-op.
		} else if lTyp.IsPointerType() && lTyp.Inner().IsEitherArrayType() {
			// No-op.
		} else {
			return fmt.Errorf("check: %s is an index expression but %s has type %s, not an array, slice or pointer-to-array type",
				n.Str(q.tm), lhs.Str(q.tm), lTyp.Str(q.tm))
		}
		rTyp := rhs.MType()
		if !rTyp.IsNumTypeOrIdeal() {
			return fmt.Errorf("check: %s is an index expression but %s has type %s, not a numeric type",
				n.Str(q.tm), rhs.Str(q.tm), rTyp.Str(q.tm))
		}
		if lTyp.IsPointerType() {
			n.SetMType(lTyp.Inner().Inner())
		} else {
			n.SetMType(lTyp.Inner())
		}
		return nil

	case t.IDDotDot:
		// n is a slice.
		// TODO: require that the i and j in a[i .. j] are *unsigned* (or
		// non-negative constants)?
		if mhs := n.MHS().AsExpr(); mhs != nil {
			if err := q.tcheckExpr(mhs, depth); err != nil {
				return err
			}
			mTyp := mhs.MType()
			if !mTyp.IsNumTypeOrIdeal() {
				return fmt.Errorf("check: %s is a slice expression but %s has type %s, not a numeric type",
					n.Str(q.tm), mhs.Str(q.tm), mTyp.Str(q.tm))
			}
		}
		if rhs := n.RHS().AsExpr(); rhs != nil {
			if err := q.tcheckExpr(rhs, depth); err != nil {
				return err
			}
			rTyp := rhs.MType()
			if !rTyp.IsNumTypeOrIdeal() {
				return fmt.Errorf("check: %s is a slice expression but %s has type %s, not a numeric type",
					n.Str(q.tm), rhs.Str(q.tm), rTyp.Str(q.tm))
			}
		}
		lhs := n.LHS().AsExpr()
		if err := q.tcheckExpr(lhs, depth); err != nil {
			return err
		}
		lTyp := lhs.MType()
		switch lTyp.Decorator() {
		default:
			return fmt.Errorf("check: %s is a slice expression but %s has type %s, not an array or slice type",
				n.Str(q.tm), lhs.Str(q.tm), lTyp.Str(q.tm))
		case t.IDArray:
			n.SetMType(a.NewTypeExpr(t.IDSlice, 0, 0, nil, nil, lTyp.Inner()))
		case t.IDRoarray:
			n.SetMType(a.NewTypeExpr(t.IDRoslice, 0, 0, nil, nil, lTyp.Inner()))
		case t.IDRoslice, t.IDSlice:
			n.SetMType(lTyp)
		}
		return nil

	case t.IDDot:
		return q.tcheckDot(n, depth)

	case t.IDComma:
		for _, o := range n.Args() {
			o := o.AsExpr()
			if err := q.tcheckExpr(o, depth); err != nil {
				return err
			}
		}
		n.SetMType(typeExprList)
		return nil
	}

	return fmt.Errorf("check: unrecognized token (0x%X) in expression %q for tcheckExprOther",
		n.Operator(), n.Str(q.tm))
}

func (q *checker) tcheckExprXDotY(n *a.Expr, x t.ID, y t.ID) error {
	qid := t.QID{x, y}
	if c, ok := q.c.consts[qid]; ok {
		// TODO: check somewhere that a global ident (i.e. a const) is
		// not directly in the LHS of an assignment.
		n.SetGlobalIdent()
		n.SetConstValue(c.Value().ConstValue())
		n.SetMType(c.XType())
		return nil
	}
	if _, ok := q.c.statuses[t.QID{x, y}]; ok {
		n.SetMType(typeExprStatus)
		return nil
	}
	// TODO: look in q.c.structs.
	return fmt.Errorf("check: unrecognized name %q", qid.Str(q.tm))
}

func (q *checker) tcheckExprCall(n *a.Expr, depth uint32) error {
	lhs := n.LHS().AsExpr()
	if err := q.tcheckExpr(lhs, depth); err != nil {
		return err
	}
	f, err := q.c.resolveFunc(lhs.MType())
	if err != nil {
		return err
	}
	if ne, fe := n.Effect(), f.Effect(); ne != fe {
		return fmt.Errorf("check: %q has effect %q but %q has effect %q",
			n.Str(q.tm), ne, f.QQID().Str(q.tm), fe)
	}
	if f.HasChooseCPUArch() {
		return fmt.Errorf(`check: cannot call cpu_arch function %q directly, only via "choose"`,
			f.QQID().Str(q.tm))
	}

	genericType1 := (*a.TypeExpr)(nil)
	genericType2 := (*a.TypeExpr)(nil)
	genericTypeRo1 := (*a.TypeExpr)(nil)
	if recv := f.Receiver(); recv[0] == t.IDBase {
		switch recv[1] {
		case t.IDDagger1:
			decorator := t.ID(0)
			genericType1 = lhs.MType().Receiver()
			if gt1Dec := genericType1.Decorator(); (gt1Dec == t.IDRoslice) || (gt1Dec == t.IDSlice) {
				decorator = t.IDRoslice
			} else {
				return fmt.Errorf("check: internal error: %q is not a generic slice", genericType1.Str(q.tm))
			}
			genericTypeRo1 = a.NewTypeExpr(decorator, 0, 0, nil, nil, genericType1.Inner())
		case t.IDDagger2:
			decorator := t.ID(0)
			genericType2 = lhs.MType().Receiver()
			if gt2Dec := genericType2.Decorator(); gt2Dec == t.IDRotable {
				decorator = t.IDRoslice
			} else if gt2Dec == t.IDTable {
				decorator = t.IDSlice
			} else {
				return fmt.Errorf("check: internal error: %q is not a generic table", genericType2.Str(q.tm))
			}
			genericType1 = a.NewTypeExpr(decorator, 0, 0, nil, nil, genericType2.Inner())
		}
	}

	// Check that the func's in type matches the arguments.
	inFields := f.In().Fields()
	if len(inFields) != len(n.Args()) {
		return fmt.Errorf("check: %q has %d arguments but %d were given",
			lhs.MType().Str(q.tm), len(inFields), len(n.Args()))
	}
	for i, o := range n.Args() {
		o := o.AsArg()
		if err := q.tcheckExpr(o.Value(), depth); err != nil {
			return err
		}

		inField := inFields[i].AsField()
		if o.Name() != inField.Name() {
			return fmt.Errorf("check: argument name: got %q, want %q", o.Name().Str(q.tm), inField.Name().Str(q.tm))
		}

		inFieldTyp := inField.XType()
		if genericType1 != nil && inFieldTyp.Eq(typeExprGeneric1) {
			inFieldTyp = genericType1
		} else if genericType2 != nil && inFieldTyp.Eq(typeExprGeneric2) {
			inFieldTyp = genericType2
		} else if genericTypeRo1 != nil && inFieldTyp.Eq(typeExprGenericRo1) {
			inFieldTyp = genericTypeRo1
		}
		if err := q.tcheckEq(inField.Name(), nil, inFieldTyp, o.Value(), o.Value().MType()); err != nil {
			return err
		}
		setPlaceholderMBoundsMType(o.AsNode())
	}

	oTyp := f.Out()
	if oTyp == nil {
		if n.Effect().Coroutine() {
			n.SetMType(typeExprStatus)
		} else {
			n.SetMType(typeExprEmptyStruct)
		}
	} else if genericType1 != nil && oTyp.Eq(typeExprGeneric1) {
		n.SetMType(genericType1)
	} else if genericType2 != nil && oTyp.Eq(typeExprGeneric2) {
		n.SetMType(genericType2)
	} else {
		n.SetMType(oTyp)
	}
	return nil
}

func (c *Checker) isBuiltInSliceFunc(qqid t.QQID, typ *a.TypeExpr) bool {
	if typ.Decorator() == t.IDRoslice {
		return (c.builtInRosliceFuncs[qqid] != nil) ||
			(typ.Eq(typeExprRosliceU8) && c.builtInRosliceU8Funcs[qqid] != nil)
	}
	return (c.builtInSliceFuncs[qqid] != nil) ||
		(typ.Eq(typeExprSliceU8) && c.builtInSliceU8Funcs[qqid] != nil)
}

func (c *Checker) isBuiltInTableFunc(qqid t.QQID, typ *a.TypeExpr) bool {
	if typ.Decorator() == t.IDRotable {
		return c.builtInRotableFuncs[qqid] != nil
	}
	return c.builtInTableFuncs[qqid] != nil
}

func (q *checker) tcheckDot(n *a.Expr, depth uint32) error {
	lhs := n.LHS().AsExpr()
	if err := q.tcheckExpr(lhs, depth); err != nil {
		return err
	}
	if lhs.MType() == typeExprPackage {
		return q.tcheckExprXDotY(n, lhs.Ident(), n.Ident())
	}
	lTyp := lhs.MType().Pointee()
	lQID := lTyp.QID()
	qqid := t.QQID{lQID[0], lQID[1], n.Ident()}

	if lTyp.IsEitherSliceType() {
		qqid[0] = t.IDBase
		qqid[1] = t.IDDagger1
		if q.c.isBuiltInSliceFunc(qqid, lTyp) {
			bulky := false
			switch qqid[2] {
			case t.IDBulkLoadHostEndian, t.IDBulkMemset, t.IDBulkSaveHostEndian:
				bulky = true
				if lTyp.Innermost().IsRefined() {
					return fmt.Errorf("check: cannot apply bulk methods to slices of refined numeric types")
				}
			}

			if !bulky || lTyp.Inner().IsBulkNumType() {
				n.SetMType(a.NewTypeExpr(t.IDFunc, 0, n.Ident(), lTyp.AsNode(), nil, nil))
				return nil
			}
		}
		return fmt.Errorf("check: no %s method %q", lTyp.Decorator().Str(q.tm), n.Ident().Str(q.tm))

	} else if lTyp.IsEitherTableType() {
		qqid[0] = t.IDBase
		qqid[1] = t.IDDagger2
		if q.c.isBuiltInTableFunc(qqid, lTyp) {
			n.SetMType(a.NewTypeExpr(t.IDFunc, 0, n.Ident(), lTyp.AsNode(), nil, nil))
			return nil
		}
		return fmt.Errorf("check: no table method %q", n.Ident().Str(q.tm))

	} else if lTyp.Decorator() != 0 {
		return fmt.Errorf("check: invalid type %q for dot-expression LHS %q", lTyp.Str(q.tm), lhs.Str(q.tm))
	}

	if f := q.c.funcs[qqid]; f != nil {
		n.SetMType(a.NewTypeExpr(t.IDFunc, 0, n.Ident(), lTyp.AsNode(), nil, nil))
		return nil
	}

	s := (*a.Struct)(nil)
	if q.astFunc != nil && lQID[0] == 0 && lQID[1] == t.IDArgs {
		s = q.astFunc.In()
	}
	if s == nil {
		s = q.c.structs[lQID]
		if s == nil {
			if lQID[0] == t.IDBase {
				return fmt.Errorf("check: no built-in function %q found", qqid.Str(q.tm))
			}
			return fmt.Errorf("check: no struct type %q found for expression %q", lTyp.Str(q.tm), lhs.Str(q.tm))
		}
	}

	for _, field := range s.Fields() {
		f := field.AsField()
		if f.Name() == n.Ident() {
			if q.astFunc.Effect().Pure() {
				n.SetMType(f.XType().CloneReadOnly())
			} else {
				n.SetMType(f.XType())
			}
			return nil
		}
	}

	return fmt.Errorf("check: no field or method named %q found in type %q for expression %q",
		n.Ident().Str(q.tm), lTyp.Str(q.tm), n.Str(q.tm))
}

func (q *checker) tcheckExprUnaryOp(n *a.Expr, depth uint32) error {
	rhs := n.RHS().AsExpr()
	if err := q.tcheckExpr(rhs, depth); err != nil {
		return err
	}
	rTyp := rhs.MType()

	switch n.Operator() {
	case t.IDXUnaryPlus, t.IDXUnaryMinus:
		if !rTyp.IsNumTypeOrIdeal() {
			return fmt.Errorf("check: unary %q: %q, of type %q, does not have a numeric type",
				n.Operator().AmbiguousForm().Str(q.tm), rhs.Str(q.tm), rTyp.Str(q.tm))
		}
		if cv := rhs.ConstValue(); cv != nil {
			if n.Operator() == t.IDXUnaryMinus {
				cv = neg(cv)
			}
			n.SetConstValue(cv)
		}
		n.SetMType(rTyp.Unrefined())
		return nil

	case t.IDXUnaryNot:
		if !rTyp.IsBool() {
			return fmt.Errorf("check: unary %q: %q, of type %q, does not have a boolean type",
				n.Operator().AmbiguousForm().Str(q.tm), rhs.Str(q.tm), rTyp.Str(q.tm))
		}
		if cv := rhs.ConstValue(); cv != nil {
			n.SetConstValue(btoi(cv.Sign() == 0))
		}
		n.SetMType(typeExprBool)
		return nil
	}
	return fmt.Errorf("check: unrecognized token (0x%X) for tcheckExprUnaryOp", n.Operator())
}

func (q *checker) tcheckExprBinaryOp(n *a.Expr, depth uint32) error {
	lhs := n.LHS().AsExpr()
	if err := q.tcheckExpr(lhs, depth); err != nil {
		return err
	}
	lTyp := lhs.MType()
	op := n.Operator()
	if op == t.IDXBinaryAs {
		rhs := n.RHS().AsTypeExpr()
		if err := q.tcheckTypeExpr(rhs, 0); err != nil {
			return err
		}
		if lTyp.IsNumTypeOrIdeal() && rhs.IsNumType() {
			n.SetMType(rhs)
			return nil
		} else if lTyp.IsEitherSliceType() &&
			(rhs.Decorator() == t.IDPtr) &&
			rhs.Inner().IsEitherArrayType() &&
			lTyp.Inner().Eq(rhs.Inner().Inner()) {

			if !lTyp.IsReadOnly() && rhs.Innermost().IsNumType() {
				n.SetMType(rhs)
				return nil
			}
			for r := rhs.Inner(); ; r = r.Inner() {
				if r.IsNumType() {
					n.SetMType(rhs)
					return nil
				} else if r.Decorator() != t.IDRoarray {
					break
				}
			}
		}
		return fmt.Errorf("check: cannot convert expression %q, of type %q, as type %q",
			lhs.Str(q.tm), lTyp.Str(q.tm), rhs.Str(q.tm))
	}
	rhs := n.RHS().AsExpr()
	if err := q.tcheckExpr(rhs, depth); err != nil {
		return err
	}
	rTyp := rhs.MType()

	pointerComparison := false
	switch op {
	case t.IDXBinaryAnd, t.IDXBinaryOr:
		if !lTyp.IsBool() {
			return fmt.Errorf("check: binary %q: %q, of type %q, does not have a boolean type",
				op.AmbiguousForm().Str(q.tm), lhs.Str(q.tm), lTyp.Str(q.tm))
		}
		if !rTyp.IsBool() {
			return fmt.Errorf("check: binary %q: %q, of type %q, does not have a boolean type",
				op.AmbiguousForm().Str(q.tm), rhs.Str(q.tm), rTyp.Str(q.tm))
		}
	default:
		bad := (*a.Expr)(nil)
		if !lTyp.IsNumTypeOrIdeal() {
			bad = lhs
		} else if !rTyp.IsNumTypeOrIdeal() {
			bad = rhs
		}
		if op == t.IDXBinaryNotEq || op == t.IDXBinaryEqEq {
			if lTyp.Eq(typeExprStatus) && rTyp.Eq(typeExprStatus) {
				break
			}
			lNullptr := lTyp.Eq(typeExprNullptr)
			rNullptr := rTyp.Eq(typeExprNullptr)
			if (lNullptr && rNullptr) ||
				(lNullptr && rTyp.IsPointerType()) ||
				(rNullptr && lTyp.IsPointerType()) {
				pointerComparison = true
				break
			}
		}
		if bad != nil {
			return fmt.Errorf("check: binary %q: %q, of type %q, does not have a numeric type",
				op.AmbiguousForm().Str(q.tm), bad.Str(q.tm), bad.MType().Str(q.tm))
		}
	}

	switch op {
	default:
		if pointerComparison {
			break
		}
		if !lTyp.EqIgnoringRefinements(rTyp) && !lTyp.IsIdeal() && !rTyp.IsIdeal() {
			return fmt.Errorf("check: binary %q: %q and %q, of types %q and %q, do not have compatible types",
				op.AmbiguousForm().Str(q.tm),
				lhs.Str(q.tm), rhs.Str(q.tm),
				lTyp.Str(q.tm), rTyp.Str(q.tm),
			)
		}
	case t.IDXBinaryShiftL, t.IDXBinaryShiftR, t.IDXBinaryTildeModShiftL:
		if lTyp.IsIdeal() && !rTyp.IsIdeal() {
			return fmt.Errorf("check: binary %q: %q and %q, of types %q and %q; "+
				"cannot shift an ideal number by a non-ideal number",
				op.AmbiguousForm().Str(q.tm),
				lhs.Str(q.tm), rhs.Str(q.tm),
				lTyp.Str(q.tm), rTyp.Str(q.tm),
			)
		}
	}

	switch op {
	case t.IDXBinaryTildeModPlus, t.IDXBinaryTildeModMinus, t.IDXBinaryTildeModStar,
		t.IDXBinaryTildeSatPlus, t.IDXBinaryTildeSatMinus:

		typ := lTyp
		if typ.IsIdeal() {
			typ = rTyp
			if typ.IsIdeal() {
				return fmt.Errorf("check: binary %q: %q and %q, of types %q and %q, do not have non-ideal types",
					op.AmbiguousForm().Str(q.tm),
					lhs.Str(q.tm), rhs.Str(q.tm),
					lTyp.Str(q.tm), rTyp.Str(q.tm),
				)
			}
		}
		if !typ.IsUnsignedInteger() {
			return fmt.Errorf("check: binary %q: %q and %q, of types %q and %q, do not have unsigned integer types",
				op.AmbiguousForm().Str(q.tm),
				lhs.Str(q.tm), rhs.Str(q.tm),
				lTyp.Str(q.tm), rTyp.Str(q.tm),
			)
		}
	}

	if lcv, rcv := lhs.ConstValue(), rhs.ConstValue(); lcv != nil && rcv != nil {
		ncv, err := evalConstValueBinaryOp(q.tm, n, lcv, rcv)
		if err != nil {
			return err
		}
		n.SetConstValue(ncv)
	}

	if (op < t.ID(len(comparisonOps))) && comparisonOps[op] {
		n.SetMType(typeExprBool)
	} else if !lTyp.IsIdeal() {
		n.SetMType(lTyp.Unrefined())
	} else {
		n.SetMType(rTyp.Unrefined())
	}

	return nil
}

func evalConstValueBinaryOp(tm *t.Map, n *a.Expr, l *big.Int, r *big.Int) (*big.Int, error) {
	switch n.Operator() {
	case t.IDXBinaryPlus:
		return big.NewInt(0).Add(l, r), nil
	case t.IDXBinaryMinus:
		return big.NewInt(0).Sub(l, r), nil
	case t.IDXBinaryStar:
		return big.NewInt(0).Mul(l, r), nil
	case t.IDXBinarySlash:
		if r.Sign() == 0 {
			return nil, fmt.Errorf("check: division by zero in const expression %q", n.Str(tm))
		}
		// TODO: decide on Euclidean division vs other definitions. See "go doc
		// math/big int.divmod" for details.
		return big.NewInt(0).Div(l, r), nil
	case t.IDXBinaryShiftL:
		if r.Sign() < 0 || r.Cmp(ffff) > 0 {
			return nil, fmt.Errorf("check: shift %q out of range in const expression %q",
				n.RHS().AsExpr().Str(tm), n.Str(tm))
		}
		return big.NewInt(0).Lsh(l, uint(r.Uint64())), nil
	case t.IDXBinaryShiftR:
		if r.Sign() < 0 || r.Cmp(ffff) > 0 {
			return nil, fmt.Errorf("check: shift %q out of range in const expression %q",
				n.RHS().AsExpr().Str(tm), n.Str(tm))
		}
		return big.NewInt(0).Rsh(l, uint(r.Uint64())), nil
	case t.IDXBinaryAmp:
		return big.NewInt(0).And(l, r), nil
	case t.IDXBinaryPipe:
		return big.NewInt(0).Or(l, r), nil
	case t.IDXBinaryHat:
		return big.NewInt(0).Xor(l, r), nil
	case t.IDXBinaryPercent:
		if r.Sign() == 0 {
			return nil, fmt.Errorf("check: division by zero in const expression %q", n.Str(tm))
		}
		return big.NewInt(0).Mod(l, r), nil
	case t.IDXBinaryNotEq:
		return btoi(l.Cmp(r) != 0), nil
	case t.IDXBinaryLessThan:
		return btoi(l.Cmp(r) < 0), nil
	case t.IDXBinaryLessEq:
		return btoi(l.Cmp(r) <= 0), nil
	case t.IDXBinaryEqEq:
		return btoi(l.Cmp(r) == 0), nil
	case t.IDXBinaryGreaterEq:
		return btoi(l.Cmp(r) >= 0), nil
	case t.IDXBinaryGreaterThan:
		return btoi(l.Cmp(r) > 0), nil
	case t.IDXBinaryAnd:
		return btoi((l.Sign() != 0) && (r.Sign() != 0)), nil
	case t.IDXBinaryOr:
		return btoi((l.Sign() != 0) || (r.Sign() != 0)), nil

	case t.IDXBinaryTildeModPlus, t.IDXBinaryTildeModMinus,
		t.IDXBinaryTildeModStar, t.IDXBinaryTildeModShiftL,
		t.IDXBinaryTildeSatPlus, t.IDXBinaryTildeSatMinus:

		return nil, fmt.Errorf("check: cannot apply tilde-operators to ideal numbers")
	}
	return nil, fmt.Errorf("check: unrecognized token (0x%X) for evalConstValueBinaryOp", n.Operator())
}

func (q *checker) tcheckExprAssociativeOp(n *a.Expr, depth uint32) error {
	switch n.Operator() {
	case t.IDXAssociativePlus, t.IDXAssociativeStar,
		t.IDXAssociativeAmp, t.IDXAssociativePipe, t.IDXAssociativeHat:

		expr, typ := (*a.Expr)(nil), (*a.TypeExpr)(nil)
		for _, o := range n.Args() {
			o := o.AsExpr()
			if err := q.tcheckExpr(o, depth); err != nil {
				return err
			}
			oTyp := o.MType()
			if oTyp.IsIdeal() {
				continue
			}
			if !oTyp.IsNumType() {
				return fmt.Errorf("check: associative %q: %q, of type %q, does not have a numeric type",
					n.Operator().AmbiguousForm().Str(q.tm), o.Str(q.tm), oTyp.Str(q.tm))
			}
			if typ == nil {
				expr, typ = o, oTyp.Unrefined()
				continue
			}
			if !typ.EqIgnoringRefinements(oTyp) {
				return fmt.Errorf("check: associative %q: %q and %q, of types %q and %q, "+
					"do not have compatible types",
					n.Operator().AmbiguousForm().Str(q.tm),
					expr.Str(q.tm), o.Str(q.tm),
					expr.MType().Str(q.tm), o.MType().Str(q.tm))
			}
		}
		if typ == nil {
			typ = typeExprIdeal
		}
		n.SetMType(typ)

	case t.IDXAssociativeAnd, t.IDXAssociativeOr:
		for _, o := range n.Args() {
			o := o.AsExpr()
			if err := q.tcheckExpr(o, depth); err != nil {
				return err
			}
			if !o.MType().IsBool() {
				return fmt.Errorf("check: associative %q: %q, of type %q, does not have a boolean type",
					n.Operator().AmbiguousForm().Str(q.tm), o.Str(q.tm), o.MType().Str(q.tm))
			}
		}
		n.SetMType(typeExprBool)

	default:
		return fmt.Errorf("check: unrecognized token (0x%X) for tcheckExprAssociativeOp", n.Operator())
	}

	ncv, err := evalConstValueAssociativeOp(q.tm, n)
	n.SetConstValue(ncv)
	return err
}

func evalConstValueAssociativeOp(tm *t.Map, n *a.Expr) (*big.Int, error) {
	args := n.Args()
	if len(args) == 0 {
		return nil, fmt.Errorf("check: no operands for associative operator")
	}
	cv0 := args[0].AsExpr().ConstValue()
	if cv0 == nil {
		return nil, nil
	}
	args = args[1:]
	ncv := big.NewInt(0).Set(cv0)

	switch n.Operator() {
	case t.IDXAssociativePlus:
		for _, o := range args {
			if cv := o.AsExpr().ConstValue(); cv == nil {
				return nil, nil
			} else {
				ncv.Add(ncv, cv)
			}
		}

	case t.IDXAssociativeStar:
		for _, o := range args {
			if cv := o.AsExpr().ConstValue(); cv == nil {
				return nil, nil
			} else {
				ncv.Mul(ncv, cv)
			}
		}

	case t.IDXAssociativeAmp, t.IDXAssociativeAnd:
		for _, o := range args {
			if cv := o.AsExpr().ConstValue(); cv == nil {
				return nil, nil
			} else {
				ncv.And(ncv, cv)
			}
		}

	case t.IDXAssociativePipe, t.IDXAssociativeOr:
		for _, o := range args {
			if cv := o.AsExpr().ConstValue(); cv == nil {
				return nil, nil
			} else {
				ncv.Or(ncv, cv)
			}
		}

	case t.IDXAssociativeHat:
		for _, o := range args {
			if cv := o.AsExpr().ConstValue(); cv == nil {
				return nil, nil
			} else {
				ncv.Xor(ncv, cv)
			}
		}

	default:
		return nil, fmt.Errorf("check: unrecognized token (0x%X) for evalConstValueAssociativeOp", n.Operator())
	}

	return ncv, nil
}

func (q *checker) tcheckTypeExpr(typ *a.TypeExpr, depth uint32) error {
	if depth > a.MaxTypeExprDepth {
		return fmt.Errorf("check: type expression recursion depth too large")
	}
	depth++

swtch:
	switch typ.Decorator() {
	// TODO: also check t.IDFunc.
	case 0:
		qid := typ.QID()
		if qid[0] == t.IDBase && qid[1].IsNumType() {
			for _, b := range typ.Bounds() {
				if b == nil {
					continue
				}
				if err := q.tcheckExpr(b, 0); err != nil {
					return err
				}
				if b.ConstValue() == nil {
					return fmt.Errorf("check: %q is not constant", b.Str(q.tm))
				}
			}
			break
		}
		if typ.Min() != nil || typ.Max() != nil {
			// TODO: reject. You can only refine numeric types.
		}
		if qid[0] == t.IDBase {
			if _, ok := builtInTypeMap[qid[1]]; ok ||
				qid[1] == t.IDDagger1 || qid[1] == t.IDDagger2 || qid[1] == t.IDRho1 {
				break swtch
			}
		}
		for _, s := range q.c.structs {
			if s.QID() == qid {
				break swtch
			}
		}
		return fmt.Errorf("check: %q is not a type", typ.Str(q.tm))

	case t.IDArray, t.IDRoarray:
		aLen := typ.ArrayLength()
		if err := q.tcheckExpr(aLen, 0); err != nil {
			return err
		}
		if cv := aLen.ConstValue(); cv == nil {
			return fmt.Errorf("check: %q is not constant", aLen.Str(q.tm))
		} else if cv.Sign() <= 0 {
			return fmt.Errorf("check: array length %q is not positive", aLen.Str(q.tm))
		}
		fallthrough

	case t.IDNptr, t.IDPtr, t.IDRoslice, t.IDRotable, t.IDSlice, t.IDTable:
		if err := q.tcheckTypeExpr(typ.Inner(), depth); err != nil {
			return err
		}

	default:
		return fmt.Errorf("check: %q is not a type", typ.Str(q.tm))
	}
	typ.AsNode().SetMType(typeExprTypeExpr)
	return nil
}

func (q *checker) tcheckChoose(n *a.Choose) error {
	qqid := q.astFunc.QQID()
	fQQID := t.QQID{qqid[0], qqid[1], n.Name()}
	f := q.c.funcs[fQQID]
	if f == nil {
		return fmt.Errorf("check: no function named %q", fQQID.Str(q.tm))
	} else if !f.Choosy() {
		return fmt.Errorf("check: choose assignee %q is not choosy", fQQID[2].Str(q.tm))
	}
	for _, o := range n.Args() {
		o := o.AsExpr()
		gQQID := t.QQID{qqid[0], qqid[1], o.Ident()}
		g := q.c.funcs[gQQID]
		if g == nil {
			return fmt.Errorf("check: no function named %q", gQQID.Str(q.tm))
		} else if err := f.CheckChooseCompatible(g); err != nil {
			return fmt.Errorf("check: incompatible choose functions %q and %q: %v",
				fQQID.Str(q.tm), gQQID.Str(q.tm), err)
		}
		o.SetMBounds(bounds{one, maxPointerBounds})
		o.SetMType(typeExprNonNullptr)
	}
	return nil
}

var comparisonOps = [...]bool{
	t.IDXBinaryNotEq:       true,
	t.IDXBinaryLessThan:    true,
	t.IDXBinaryLessEq:      true,
	t.IDXBinaryEqEq:        true,
	t.IDXBinaryGreaterEq:   true,
	t.IDXBinaryGreaterThan: true,
}
