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
	"errors"
	"fmt"
	"math/big"
	"strconv"
	"strings"

	"github.com/google/wuffs/lib/interval"

	a "github.com/google/wuffs/lang/ast"
	t "github.com/google/wuffs/lang/token"
)

type bounds = interval.IntRange

var numShiftBounds = [...]bounds{
	t.IDU8:  {zero, big.NewInt(7)},
	t.IDU16: {zero, big.NewInt(15)},
	t.IDU32: {zero, big.NewInt(31)},
	t.IDU64: {zero, big.NewInt(63)},
}

var numTypeBounds = [...]bounds{
	t.IDI8:   {big.NewInt(-1 << 7), big.NewInt(1<<7 - 1)},
	t.IDI16:  {big.NewInt(-1 << 15), big.NewInt(1<<15 - 1)},
	t.IDI32:  {big.NewInt(-1 << 31), big.NewInt(1<<31 - 1)},
	t.IDI64:  {big.NewInt(-1 << 63), big.NewInt(1<<63 - 1)},
	t.IDU8:   {zero, big.NewInt(0).SetUint64(1<<8 - 1)},
	t.IDU16:  {zero, big.NewInt(0).SetUint64(1<<16 - 1)},
	t.IDU32:  {zero, big.NewInt(0).SetUint64(1<<32 - 1)},
	t.IDU64:  {zero, big.NewInt(0).SetUint64(1<<64 - 1)},
	t.IDBool: {zero, one},
}

var (
	minusOne       = big.NewInt(-1)
	zero           = big.NewInt(+0)
	one            = big.NewInt(+1)
	two            = big.NewInt(+2)
	three          = big.NewInt(+3)
	four           = big.NewInt(+4)
	five           = big.NewInt(+5)
	six            = big.NewInt(+6)
	seven          = big.NewInt(+7)
	eight          = big.NewInt(+8)
	sixteen        = big.NewInt(+16)
	thirtyTwo      = big.NewInt(+32)
	sixtyFour      = big.NewInt(+64)
	oneTwentyEight = big.NewInt(+128)
	ffff           = big.NewInt(0xFFFF)

	// maxPointerBounds is the artificial value in the [0 ..= maxPointerBounds]
	// range for bounds-checking pointer-typed values. Its value is arbitrary
	// (but greater than 1).
	//
	// It is positive (not 0) to distinguish nullptr from non-nullptr. More
	// subtly, it is greater than 1 so that two non-nullptr pointer-typed
	// values aren't necessarily equal.
	maxPointerBounds = big.NewInt(1234)

	// funcBounds is the artificial value in the [1 ..= funcBounds] range for
	// bounds-checking function-typed values. Its value is arbitrary (but
	// greater than 1) for the same reasons as maxPointerBounds.
	funcBounds = big.NewInt(5678)

	minIdeal = big.NewInt(0).Lsh(minusOne, 1000)
	maxIdeal = big.NewInt(0).Lsh(one, 1000)

	maxIntBits = big.NewInt(t.MaxIntBits)

	zeroExpr = a.NewExpr(0, 0, t.ID0, nil, nil, nil, nil)
)

func init() {
	zeroExpr.SetConstValue(zero)
	zeroExpr.SetMBounds(bounds{zero, zero})
	zeroExpr.SetMType(typeExprIdeal)
}

func isErrorStatus(literal t.ID, tm *t.Map) bool {
	s := literal.Str(tm)
	return (len(s) >= 2) && (s[0] == '"') && (s[1] == '#')
}

func btoi(b bool) *big.Int {
	if b {
		return one
	}
	return zero
}

func add1(i *big.Int) *big.Int {
	return big.NewInt(0).Add(i, one)
}

func sub1(i *big.Int) *big.Int {
	return big.NewInt(0).Sub(i, one)
}

func neg(i *big.Int) *big.Int {
	return big.NewInt(0).Neg(i)
}

func min(i, j *big.Int) *big.Int {
	if i.Cmp(j) < 0 {
		return i
	}
	return j
}

func max(i, j *big.Int) *big.Int {
	if i.Cmp(j) > 0 {
		return i
	}
	return j
}

// bitMask returns (1<<nBits - 1) as a big integer.
func bitMask(nBits int) *big.Int {
	switch nBits {
	case 0:
		return zero
	case 1:
		return one
	case 8:
		return numTypeBounds[t.IDU8][1]
	case 16:
		return numTypeBounds[t.IDU16][1]
	case 32:
		return numTypeBounds[t.IDU32][1]
	case 64:
		return numTypeBounds[t.IDU64][1]
	}
	z := big.NewInt(0).Lsh(one, uint(nBits))
	return z.Sub(z, one)
}

func invert(tm *t.Map, n *a.Expr) (*a.Expr, error) {
	if !n.MType().IsBool() {
		return nil, fmt.Errorf("check: invert(%q) called on non-bool-typed expression", n.Str(tm))
	}
	if cv := n.ConstValue(); cv != nil {
		return nil, fmt.Errorf("check: invert(%q) called on constant expression", n.Str(tm))
	}
	op, lhs, rhs, args := n.Operator(), n.LHS().AsExpr(), n.RHS().AsExpr(), []*a.Node(nil)
	switch op {
	case t.IDXUnaryNot:
		return rhs, nil
	case t.IDXBinaryNotEq:
		op = t.IDXBinaryEqEq
	case t.IDXBinaryLessThan:
		op = t.IDXBinaryGreaterEq
	case t.IDXBinaryLessEq:
		op = t.IDXBinaryGreaterThan
	case t.IDXBinaryEqEq:
		op = t.IDXBinaryNotEq
	case t.IDXBinaryGreaterEq:
		op = t.IDXBinaryLessThan
	case t.IDXBinaryGreaterThan:
		op = t.IDXBinaryLessEq
	case t.IDXBinaryAnd, t.IDXBinaryOr:
		var err error
		lhs, err = invert(tm, lhs)
		if err != nil {
			return nil, err
		}
		rhs, err = invert(tm, rhs)
		if err != nil {
			return nil, err
		}
		if op == t.IDXBinaryAnd {
			op = t.IDXBinaryOr
		} else {
			op = t.IDXBinaryAnd
		}
	case t.IDXAssociativeAnd, t.IDXAssociativeOr:
		args = make([]*a.Node, 0, len(n.Args()))
		for _, a := range n.Args() {
			v, err := invert(tm, a.AsExpr())
			if err != nil {
				return nil, err
			}
			args = append(args, v.AsNode())
		}
		if op == t.IDXAssociativeAnd {
			op = t.IDXAssociativeOr
		} else {
			op = t.IDXAssociativeAnd
		}
	default:
		op, lhs, rhs = t.IDXUnaryNot, nil, n
	}
	o := a.NewExpr(n.AsNode().AsRaw().Flags(), op, 0, lhs.AsNode(), nil, rhs.AsNode(), args)
	o.SetMType(n.MType())
	return o, nil
}

var errUpdateReturnsNil = errors.New("update returns nil")

func updateFactsForSuspension(x *a.Expr) (*a.Expr, error) {
	// Drop any facts involving args, this or ptr-typed local variables.
	if err := x.AsNode().Walk(func(innerNode *a.Node) error {
		if innerNode.Kind() != a.KExpr {
			return nil
		}
		n := innerNode.AsExpr()
		if n.MType().HasPointers() {
			return errUpdateReturnsNil
		} else if n.Operator() == 0 {
			switch n.Ident() {
			case t.IDArgs, t.IDThis:
				return errUpdateReturnsNil
			}
		}
		return nil
	}); err == errUpdateReturnsNil {
		return nil, nil
	} else if err != nil {
		return nil, err
	}

	return x, nil
}

func (q *checker) bcheckBlock(block []*a.Node) error {
	unreachable := false
	for _, o := range block {
		q.errFilename, q.errLine = o.AsRaw().FilenameLine()
		if unreachable {
			return fmt.Errorf("check: unreachable code")
		}
		if err := q.bcheckStatement(o); err != nil {
			return err
		}

		switch o.Kind() {
		default:
			continue
		case a.KJump:
			// No-op.
		case a.KRet:
			if o.AsRet().Keyword() == t.IDYield {
				if err := q.facts.update(updateFactsForSuspension); err != nil {
					return err
				}
				continue
			}
		}
		unreachable = true
	}
	return nil
}

func (q *checker) bcheckStatement(n *a.Node) error {
	switch n.Kind() {
	case a.KAssert:
		if err := q.bcheckAssert(n.AsAssert()); err != nil {
			return err
		}

	case a.KAssign:
		n := n.AsAssign()
		if err := q.bcheckAssignment(n.LHS(), n.Operator(), n.RHS()); err != nil {
			return err
		}

	case a.KChoose:
		// No-op.

	case a.KIOManip:
		n := n.AsIOManip()
		if _, err := q.bcheckExpr(n.IO(), 0); err != nil {
			return err
		}
		if arg1 := n.Arg1(); arg1 != nil {
			if _, err := q.bcheckExpr(arg1, 0); err != nil {
				return err
			}
		}
		if histPos := n.HistoryPosition(); histPos != nil {
			if _, err := q.bcheckExpr(histPos, 0); err != nil {
				return err
			}
		}
		q.facts.dropAnyFactsMentioning(n.IO())
		if err := q.bcheckBlock(n.Body()); err != nil {
			return err
		}
		q.facts.dropAnyFactsMentioning(n.IO())

	case a.KIf:
		if err := q.bcheckIf(n.AsIf()); err != nil {
			return err
		}

	case a.KIterate:
		n := n.AsIterate()
		if _, err := q.bcheckExpr(n.UnrollAsExpr(), 0); err != nil {
			return err
		}
		for _, o := range n.Assigns() {
			o := o.AsAssign()
			if err := q.bcheckAssignment(o.LHS(), o.Operator(), o.RHS()); err != nil {
				return err
			}
		}
		// TODO: this isn't right, as the body is a loop, not an
		// execute-exactly-once block. We should have pre / inv / post
		// conditions, a la bcheckWhile.

		assigns := n.Assigns()
		for ; n != nil; n = n.ElseIterate() {
			if _, err := q.bcheckExpr(n.UnrollAsExpr(), 0); err != nil {
				return err
			}
			q.facts = q.facts[:0]
			for _, o := range assigns {
				lhs := o.AsAssign().LHS()
				lhsExpr := a.NewExpr(0, 0, lhs.Ident(), nil, nil, nil, nil)
				lhsExpr.SetMType(lhs.MType())
				q.facts = append(q.facts, q.makeSliceLengthEqEq(lhsExpr, n.Length()))
			}
			if err := q.bcheckBlock(n.Body()); err != nil {
				return err
			}
		}

		q.facts = q.facts[:0]

	case a.KJump:
		n := n.AsJump()
		skip := t.IDPost
		if n.Keyword() == t.IDBreak {
			skip = t.IDPre
		}
		for _, o := range n.JumpTarget().Asserts() {
			if o.AsAssert().Keyword() == skip {
				continue
			}
			if err := q.bcheckAssert(o.AsAssert()); err != nil {
				return err
			}
		}
		q.facts = q.facts[:0]

	case a.KRet:
		n := n.AsRet()
		lTyp := q.astFunc.Out()
		if q.astFunc.Effect().Coroutine() {
			lTyp = typeExprStatus
		} else if lTyp == nil {
			lTyp = typeExprEmptyStruct
		}
		if _, err := q.bcheckAssignment1(nil, lTyp, t.IDEq, n.Value()); err != nil {
			return err
		}

		if lTyp.IsStatus() {
			if v := n.Value(); (v.Operator() == 0) || (v.Operator() == a.ExprOperatorSelector) {
				if id := v.Ident(); (id != t.IDOk) && (q.hasIsErrorFact(id) || isErrorStatus(id, q.tm)) {
					n.SetRetsError()
				}
			}
		}

	case a.KVar:
		if err := q.bcheckVar(n.AsVar()); err != nil {
			return err
		}

	case a.KWhile:
		if err := q.bcheckWhile(n.AsWhile()); err != nil {
			return err
		}

	default:
		return fmt.Errorf("check: unrecognized ast.Kind (%s) for bcheckStatement", n.Kind())
	}
	return nil
}

func (q *checker) hasIsErrorFact(id t.ID) bool {
	for _, x := range q.facts {
		if lhs, meth, args, _ := x.IsMethodCall(); (meth != t.IDIsError) || (len(args) != 0) ||
			(lhs.Operator() != 0) || (lhs.Ident() != id) {
			continue
		}
		return true
	}
	return false
}

func (q *checker) bcheckFuncAssert(n *a.Assert) error {
	if n.IsChooseCPUArch() {
		b := bounds{zero, one}
		cond := n.Condition()
		cond.SetMBounds(b)
		cond.LHS().AsExpr().SetMBounds(b)
		cond.RHS().AsExpr().SetMBounds(b)
		return nil
	}
	return fmt.Errorf("check: function assertions are not supported yet")
}

func (q *checker) bcheckAssert(n *a.Assert) error {
	if err := n.DropExprCachedMBounds(); err != nil {
		return err
	}

	condition := n.Condition()
	if _, err := q.bcheckExpr(condition, 0); err != nil {
		return err
	}
	for _, o := range n.Args() {
		if _, err := q.bcheckExpr(o.AsArg().Value(), 0); err != nil {
			return err
		}
	}

	for _, x := range q.facts {
		if x.Eq(condition) {
			return nil
		}
	}
	err := errFailed

	if cv := condition.ConstValue(); cv != nil {
		if cv.Cmp(one) == 0 {
			err = nil
		}
	} else if reasonID := n.Reason(); reasonID != 0 {
		if reasonFunc := q.reasonMap[reasonID]; reasonFunc != nil {
			err = reasonFunc(q, n)
		} else {
			err = fmt.Errorf("check: no such reason %s", reasonID.Str(q.tm))
		}
	} else if condition.Operator().IsBinaryOp() && condition.Operator() != t.IDAs {
		err = q.proveBinaryOp(condition.Operator(),
			condition.LHS().AsExpr(), condition.RHS().AsExpr())
	}

	if err != nil {
		if err == errFailed {
			return fmt.Errorf("check: cannot prove %q", condition.Str(q.tm))
		}
		return fmt.Errorf("check: cannot prove %q: %v", condition.Str(q.tm), err)
	}
	o, err := simplify(q.tm, condition)
	if err != nil {
		return err
	}
	q.facts.appendFact(o)
	return nil
}

func (q *checker) bcheckAssignment(lhs *a.Expr, op t.ID, rhs *a.Expr) error {
	oldFacts := (map[*a.Expr]struct{})(nil)
	if (rhs.Operator() == a.ExprOperatorCall) && rhs.Effect().Impure() {
		oldFacts = map[*a.Expr]struct{}{}
		for _, x := range q.facts {
			oldFacts[x] = struct{}{}
		}
	}

	lTyp := (*a.TypeExpr)(nil)
	if lhs != nil {
		if _, err := q.bcheckExpr(lhs, 0); err != nil {
			return err
		}
		lTyp = lhs.MType()
	}

	if (rhs.Operator() == a.ExprOperatorCall) && rhs.Effect().Coroutine() && (op != t.IDEqQuestion) {
		if err := q.facts.update(updateFactsForSuspension); err != nil {
			return err
		}
	}

	nb, err := q.bcheckAssignment1(lhs, lTyp, op, rhs)
	if err != nil {
		return err
	}

	if (rhs.Operator() == a.ExprOperatorCall) && rhs.Effect().Impure() {
		recv := rhs.LHS().AsExpr().LHS().AsExpr()
		if err := q.facts.update(func(x *a.Expr) (*a.Expr, error) {
			if _, ok := oldFacts[x]; !ok {
				// No-op. Don't drop any newly minted facts.
			} else {
				// Drop any old facts involving the receiver.
				if x.Mentions(recv) {
					return nil, nil
				}
				// Drop any facts involving a pass-by-reference argument.
				for _, arg := range rhs.Args() {
					v := arg.AsArg().Value()
					if typ := v.MType(); typ.IsBool() || typ.IsNullptr() ||
						typ.IsNumTypeOrIdeal() || typ.IsStatus() {
						continue
					}
					// TODO: take extra care if v is a slice? For example,
					// facts involving "v.length()" aren't affected by passing
					// v to an impure function.
					if x.Mentions(v) {
						return nil, nil
					}
				}
			}
			return x, nil
		}); err != nil {
			return err
		}
	}

	if lhs == nil {
		return nil
	}

	if op == t.IDEq {
		if err := q.facts.dropAnyFactsMentioning(lhs); err != nil {
			return err
		}

		if !rhs.Effect().Pure() {
			// No-op.

		} else if lhs.MType().IsNumType() {
			q.facts.appendBinaryOpFact(t.IDXBinaryEqEq, lhs, rhs)

			if rhs.Operator() == a.ExprOperatorCall {
				if lTyp := rhs.LHS().AsExpr().MType(); lTyp.IsFuncType() && lTyp.Receiver().IsNumType() {
					switch fn := lTyp.FuncName(); fn {
					case t.IDMax, t.IDMin:
						if err := q.bcheckAssignmentMaxMin(lhs, fn, rhs); err != nil {
							return err
						}
					}
				}
			}

		} else if lhs.MType().IsPointerType() {
			if cv := rhs.ConstValue(); (cv != nil) && (cv.Sign() == 0) {
				q.facts.appendBinaryOpFact(t.IDXBinaryEqEq, lhs, exprNullptr)
			} else {
				q.facts.appendBinaryOpFact(t.IDXBinaryEqEq, lhs, rhs)
				if (lhs.MType().Decorator() == t.IDNptr) && (rhs.MType().Decorator() == t.IDPtr) {
					q.facts.appendBinaryOpFact(t.IDXBinaryNotEq, lhs, exprNullptr)
				}
			}
		}

		// Look for "lhs = x[i .. j]" where i and j are constants.
		if _, i, j, ok := rhs.IsSlice(); ok {
			icv := (*big.Int)(nil)
			if i == nil {
				icv = zero
			} else if i.ConstValue() != nil {
				icv = i.ConstValue()
			}

			jcv := (*big.Int)(nil)
			if (j != nil) && (j.ConstValue() != nil) {
				jcv = j.ConstValue()
			}

			if (icv != nil) && (jcv != nil) {
				n := big.NewInt(0).Sub(jcv, icv)
				id, err := q.tm.Insert(n.String())
				if err != nil {
					return err
				}
				// TODO: dupe lhs before making a new fact referencing it?
				q.facts = append(q.facts, q.makeSliceLengthEqEq(lhs, id))
			}
		}

	} else {
		// Update any facts involving lhs.
		if err := q.facts.update(func(x *a.Expr) (*a.Expr, error) {
			xOp, xLHS, xRHS := parseBinaryOp(x)
			if xOp == 0 || !xLHS.Eq(lhs) {
				if x.Mentions(lhs) {
					return nil, nil
				}
				return x, nil
			}
			if xRHS.Mentions(lhs) {
				return nil, nil
			}
			switch op {
			case t.IDPlusEq, t.IDMinusEq:
				oRHS := a.NewExpr(0, op.BinaryForm(), 0, xRHS.AsNode(), nil, rhs.AsNode(), nil)
				// TODO: call SetMBounds?
				oRHS.SetMType(typeExprIdeal)
				oRHS, err := simplify(q.tm, oRHS)
				if err != nil {
					return nil, err
				}
				o := a.NewExpr(0, xOp, 0, xLHS.AsNode(), nil, oRHS.AsNode(), nil)
				o.SetMBounds(bounds{zero, one})
				o.SetMType(typeExprBool)
				return o, nil
			}
			return nil, nil
		}); err != nil {
			return err
		}
	}

	if lhs.MType().IsNumType() && ((op != t.IDEq) || (rhs.ConstValue() == nil)) {
		lb, err := q.bcheckTypeExpr(lhs.MType())
		if err != nil {
			return err
		}
		if lb[0].Cmp(nb[0]) < 0 {
			c, err := makeConstValueExpr(q.tm, nb[0])
			if err != nil {
				return err
			}
			q.facts.appendBinaryOpFact(t.IDXBinaryGreaterEq, lhs, c)
		}
		if lb[1].Cmp(nb[1]) > 0 {
			c, err := makeConstValueExpr(q.tm, nb[1])
			if err != nil {
				return err
			}
			q.facts.appendBinaryOpFact(t.IDXBinaryLessEq, lhs, c)
		}
	}

	return nil
}

func (q *checker) bcheckAssignment1(lhs *a.Expr, lTyp *a.TypeExpr, op t.ID, rhs *a.Expr) (bounds, error) {
	if lhs == nil && op != t.IDEq {
		return bounds{}, fmt.Errorf("check: internal error: missing LHS for op key 0x%X", op)
	}

	lb, err := bounds{}, (error)(nil)
	if lTyp != nil {
		lb, err = q.bcheckTypeExpr(lTyp)
		if err != nil {
			return bounds{}, err
		}
	}

	rb := bounds{}
	if op == t.IDEq || op == t.IDEqQuestion {
		rb, err = q.bcheckExpr(rhs, 0)
	} else {
		rb, err = q.bcheckExprBinaryOp(op.BinaryForm(), lhs, rhs, 0)
	}
	if err != nil {
		return bounds{}, err
	}

	if (lTyp != nil) && ((rb[0].Cmp(lb[0]) < 0) || (rb[1].Cmp(lb[1]) > 0)) {
		if op == t.IDEq {
			return bounds{}, fmt.Errorf("check: expression %q bounds %v is not within bounds %v",
				rhs.Str(q.tm), rb, lb)
		} else {
			return bounds{}, fmt.Errorf("check: assignment %q bounds %v is not within bounds %v",
				lhs.Str(q.tm)+" "+op.Str(q.tm)+" "+rhs.Str(q.tm), rb, lb)
		}
	}
	return rb, nil
}

func (q *checker) bcheckAssignmentMaxMin(lhs *a.Expr, funcName t.ID, rhs *a.Expr) error {
	if len(rhs.Args()) != 1 {
		return fmt.Errorf("check: internal error: max/min has unexpected arguments")
	}
	op := t.ID(0)
	switch funcName {
	case t.IDMax:
		op = t.IDXBinaryGreaterEq
	case t.IDMin:
		op = t.IDXBinaryLessEq
	default:
		return fmt.Errorf("check: internal error: max/min has unexpected function name")
	}

	operands := [2]*a.Expr{
		rhs.LHS().AsExpr().LHS().AsExpr(),
		rhs.Args()[0].AsArg().Value(),
	}
	for _, operand := range operands {
		if operand.Mentions(lhs) {
			continue
		}
		o := a.NewExpr(0, op, 0, lhs.AsNode(), nil, operand.AsNode(), nil)
		o.SetMBounds(bounds{zero, one})
		o.SetMType(typeExprBool)
		q.facts.appendFact(o)
	}
	return nil
}

func snapshot(facts []*a.Expr) []*a.Expr {
	return append([]*a.Expr(nil), facts...)
}

func (q *checker) unify(branches [][]*a.Expr) error {
	q.facts = q.facts[:0]
	if len(branches) == 0 {
		return nil
	}
	q.facts = append(q.facts[:0], branches[0]...)
	if len(branches) == 1 {
		return nil
	}
	if len(branches) > 10000 {
		return fmt.Errorf("check: too many if-else branches")
	}

	m := map[string]int{}
	for _, b := range branches {
		for _, f := range b {
			m[f.Str(q.tm)]++
		}
	}

	return q.facts.update(func(n *a.Expr) (*a.Expr, error) {
		if m[n.Str(q.tm)] == len(branches) {
			return n, nil
		}
		return nil, nil
	})
}

func (q *checker) bcheckIf(n *a.If) error {
	branches := [][]*a.Expr(nil)
	for n != nil {
		snap := snapshot(q.facts)
		// Check the if condition.
		if _, err := q.bcheckExpr(n.Condition(), 0); err != nil {
			return err
		}

		// Check the if-true branch, assuming the if condition.
		if n.Condition().ConstValue() == nil {
			q.facts.appendFact(n.Condition())
		}
		if err := q.bcheckBlock(n.BodyIfTrue()); err != nil {
			return err
		}
		if !a.Terminates(n.BodyIfTrue()) {
			branches = append(branches, snapshot(q.facts))
		}

		// Check the if-false branch, assuming the inverted if condition.
		q.facts = append(q.facts[:0], snap...)
		if n.Condition().ConstValue() == nil {
			if inverse, err := invert(q.tm, n.Condition()); err != nil {
				return err
			} else {
				q.facts.appendFact(inverse)
			}
		}
		if bif := n.BodyIfFalse(); len(bif) > 0 {
			if err := q.bcheckBlock(bif); err != nil {
				return err
			}
			if !a.Terminates(bif) {
				branches = append(branches, snapshot(q.facts))
			}
			break
		}
		n = n.ElseIf()
		if n == nil {
			branches = append(branches, snapshot(q.facts))
			break
		}
	}
	return q.unify(branches)
}

func (q *checker) bcheckWhile(n *a.While) error {
	// Check the pre and inv conditions on entry.
	for _, o := range n.Asserts() {
		if o.AsAssert().Keyword() == t.IDPost {
			continue
		}
		if err := q.bcheckAssert(o.AsAssert()); err != nil {
			return err
		}
	}

	// Check the while condition.
	if _, err := q.bcheckExpr(n.Condition(), 0); err != nil {
		return err
	}

	// Check the post conditions on exit, assuming only the pre and inv
	// (invariant) conditions and the inverted while condition.
	//
	// We don't need to check the inv conditions, even though we add them to
	// the facts after the while loop, since we have already proven each inv
	// condition on entry, and below, proven them on each explicit continue and
	// on the implicit continue after the body.
	if cv := n.Condition().ConstValue(); cv != nil && cv.Cmp(one) == 0 {
		// We effectively have a "while true { etc }" loop. There's no need to
		// prove the post conditions here, since we won't ever exit the while
		// loop naturally. We only exit on an explicit break.
	} else {
		q.facts = q.facts[:0]
		for _, o := range n.Asserts() {
			if o.AsAssert().Keyword() == t.IDPost {
				continue
			}
			q.facts.appendFact(o.AsAssert().Condition())
		}
		if inverse, err := invert(q.tm, n.Condition()); err != nil {
			return err
		} else {
			q.facts.appendFact(inverse)
		}
		for _, o := range n.Asserts() {
			if o.AsAssert().Keyword() == t.IDPost {
				if err := q.bcheckAssert(o.AsAssert()); err != nil {
					return err
				}
			}
		}
	}

	if cv := n.Condition().ConstValue(); cv != nil && cv.Sign() == 0 {
		// We effectively have a "while false { etc }" loop. There's no need to
		// check the body.
	} else {
		// Assume the pre and inv conditions...
		q.facts = q.facts[:0]
		for _, o := range n.Asserts() {
			if o.AsAssert().Keyword() == t.IDPost {
				continue
			}
			q.facts.appendFact(o.AsAssert().Condition())
		}
		// ...and the while condition, unless it is the redundant "true".
		if cv == nil {
			q.facts.appendFact(n.Condition())
		}
		// Check the body.
		if err := q.bcheckBlock(n.Body()); err != nil {
			return err
		}
		// Check the pre and inv conditions on the implicit continue after the
		// body.
		if !a.Terminates(n.Body()) {
			for _, o := range n.Asserts() {
				if o.AsAssert().Keyword() == t.IDPost {
					continue
				}
				if err := q.bcheckAssert(o.AsAssert()); err != nil {
					return err
				}
			}
		}
	}

	// Assume the inv and post conditions.
	q.facts = q.facts[:0]
	for _, o := range n.Asserts() {
		if o.AsAssert().Keyword() == t.IDPre {
			continue
		}
		q.facts.appendFact(o.AsAssert().Condition())
	}
	return nil
}

func (q *checker) bcheckVar(n *a.Var) error {
	if _, err := q.bcheckTypeExpr(n.XType()); err != nil {
		return err
	}

	lhs := a.NewExpr(0, 0, n.Name(), nil, nil, nil, nil)
	lhs.SetMType(n.XType())
	// "var x T" has an implicit "= 0".
	//
	// TODO: check that T is an integer type.
	rhs := zeroExpr
	return q.bcheckAssignment(lhs, t.IDEq, rhs)
}

func (q *checker) bcheckExpr(n *a.Expr, depth uint32) (bounds, error) {
	if depth > a.MaxExprDepth {
		return bounds{}, fmt.Errorf("check: expression recursion depth too large")
	}
	depth++

	if b := n.MBounds(); b[0] != nil {
		return b, nil
	}
	if n.ConstValue() != nil {
		return bcheckExprConstValue(n)
	}

	nb, err := q.bcheckExpr1(n, depth)
	if err != nil {
		return bounds{}, err
	}
	nb, err = q.facts.refine(n, nb, q.tm)
	if err != nil {
		return bounds{}, err
	}
	tb, err := q.bcheckTypeExpr(n.MType())
	if err != nil {
		return bounds{}, err
	}

	if (nb[0].Cmp(tb[0]) < 0) || (nb[1].Cmp(tb[1]) > 0) {
		return bounds{}, fmt.Errorf("check: expression %q bounds %v is not within bounds %v",
			n.Str(q.tm), nb, tb)
	}

	n.SetMBounds(nb)
	return nb, nil
}

func bcheckExprConstValue(n *a.Expr) (bounds, error) {
	if o := n.LHS(); o != nil {
		if _, err := bcheckExprConstValue(o.AsExpr()); err != nil {
			return bounds{}, err
		}
	}
	if o := n.MHS(); o != nil {
		if _, err := bcheckExprConstValue(o.AsExpr()); err != nil {
			return bounds{}, err
		}
	}
	if o := n.RHS(); o != nil && n.Operator() != t.IDXBinaryAs {
		if _, err := bcheckExprConstValue(o.AsExpr()); err != nil {
			return bounds{}, err
		}
	}
	for _, o := range n.Args() {
		if _, err := bcheckExprConstValue(o.AsExpr()); err != nil {
			return bounds{}, err
		}
	}
	cv := n.ConstValue()
	if cv == nil {
		return bounds{}, fmt.Errorf("check: constant expression has nil ConstValue")
	}
	b := bounds{cv, cv}
	n.SetMBounds(b)
	return b, nil
}

func (q *checker) bcheckExpr1(n *a.Expr, depth uint32) (bounds, error) {
	switch op := n.Operator(); {
	case op.IsXUnaryOp():
		return q.bcheckExprUnaryOp(n, depth)
	case op.IsXBinaryOp():
		if op != t.IDXBinaryAs {
			return q.bcheckExprBinaryOp(op, n.LHS().AsExpr(), n.RHS().AsExpr(), depth)
		}
		b, err := q.bcheckExpr(n.LHS().AsExpr(), depth)
		if err != nil {
			return bounds{}, err
		} else if lhs := n.LHS().AsExpr(); lhs.MType().IsEitherSliceType() {
			cv := (*big.Int)(nil)
			rhs := n.RHS().AsTypeExpr()
			if (rhs.Decorator() == t.IDPtr) && (rhs.Inner().IsEitherArrayType()) {
				cv = rhs.Inner().ArrayLength().ConstValue()
			}
			if cv == nil {
				// TODO: fix this.
				return bounds{}, fmt.Errorf("check: cannot bounds-check conversion to %q", rhs.Str(q.tm))
			}
			if err := q.proveSliceLengthAtLeast(lhs, cv); err != nil {
				return bounds{}, err
			}
			b = bounds{one, maxPointerBounds}
		}
		return b, nil
	case op.IsXAssociativeOp():
		return q.bcheckExprAssociativeOp(n, depth)
	}

	return q.bcheckExprOther(n, depth)
}

func (q *checker) bcheckExprOther(n *a.Expr, depth uint32) (bounds, error) {
	switch n.Operator() {
	case 0:
		// Look for named consts.
		//
		// TODO: look up "foo[i]" const expressions.
		//
		// TODO: allow imported consts, "foo.bar", not just "bar"?
		qid := t.QID{0, n.Ident()}
		if c, ok := q.c.consts[qid]; ok {
			if cv := c.Value().ConstValue(); cv != nil {
				return bounds{cv, cv}, nil
			}
		}

	case t.IDOpenParen:
		lhs := n.LHS().AsExpr()
		if _, err := q.bcheckExpr(lhs, depth); err != nil {
			return bounds{}, err
		}
		if err := q.bcheckExprCall(n, depth); err != nil {
			return bounds{}, err
		}
		if nb, err := q.bcheckExprCallSpecialCases(n, depth); err == nil {
			return nb, nil
		} else if err != errNotASpecialCase {
			return bounds{}, err
		}

	case t.IDOpenBracket:
		lhs := n.LHS().AsExpr()
		if _, err := q.bcheckExpr(lhs, depth); err != nil {
			return bounds{}, err
		}
		rhs := n.RHS().AsExpr()
		if _, err := q.bcheckExpr(rhs, depth); err != nil {
			return bounds{}, err
		}

		lengthExpr := (*a.Expr)(nil)
		if lTyp := lhs.MType(); lTyp.IsEitherArrayType() {
			lengthExpr = lTyp.ArrayLength()
		} else if lTyp.IsPointerType() && lTyp.Inner().IsEitherArrayType() {
			lengthExpr = lTyp.Inner().ArrayLength()
			if lTyp.Decorator() == t.IDPtr {
				// No-op.
			} else if err := q.proveRecvNotEqNullptr(lhs); err != nil {
				return bounds{}, err
			}
		} else {
			lengthExpr = makeSliceLength(lhs)
		}

		if err := proveReasonRequirement(q, t.IDXBinaryLessEq, zeroExpr, rhs); err != nil {
			return bounds{}, err
		}
		if err := proveReasonRequirementForRHSLength(q, t.IDXBinaryLessThan, rhs, lengthExpr); err != nil {
			return bounds{}, err
		}

	case t.IDDotDot:
		lhs := n.LHS().AsExpr()
		if _, err := q.bcheckExpr(lhs, depth); err != nil {
			return bounds{}, err
		}
		mhs := n.MHS().AsExpr()
		if mhs != nil {
			if _, err := q.bcheckExpr(mhs, depth); err != nil {
				return bounds{}, err
			}
		}
		rhs := n.RHS().AsExpr()
		if rhs != nil {
			if _, err := q.bcheckExpr(rhs, depth); err != nil {
				return bounds{}, err
			}
		}

		if mhs == nil && rhs == nil {
			return bounds{zero, zero}, nil
		}

		lengthExpr := (*a.Expr)(nil)
		if lTyp := lhs.MType(); lTyp.IsEitherArrayType() {
			lengthExpr = lTyp.ArrayLength()
		} else {
			lengthExpr = makeSliceLength(lhs)
		}

		if mhs == nil {
			mhs = zeroExpr
		}
		if rhs == nil {
			rhs = lengthExpr
		}

		if mhs != zeroExpr {
			if err := proveReasonRequirement(q, t.IDXBinaryLessEq, zeroExpr, mhs); err != nil {
				return bounds{}, err
			}
		}
		if err := proveReasonRequirement(q, t.IDXBinaryLessEq, mhs, rhs); err != nil {
			return bounds{}, err
		}
		if rhs != lengthExpr {
			if err := proveReasonRequirementForRHSLength(q, t.IDXBinaryLessEq, rhs, lengthExpr); err != nil {
				return bounds{}, err
			}
		}

	case t.IDDot:
		if _, err := q.bcheckExpr(n.LHS().AsExpr(), depth); err != nil {
			return bounds{}, err
		}

		// TODO: delete this hack that only matches "args".
		if n.LHS().AsExpr().Ident() == t.IDArgs {
			for _, o := range q.astFunc.In().Fields() {
				o := o.AsField()
				if o.Name() == n.Ident() {
					return q.bcheckTypeExpr(o.XType())
				}
			}
			lTyp := n.LHS().AsExpr().MType()
			return bounds{}, fmt.Errorf("check: no field named %q found in struct type %q for expression %q",
				n.Ident().Str(q.tm), lTyp.QID().Str(q.tm), n.Str(q.tm))
		}

	case t.IDComma:
		for _, o := range n.Args() {
			if _, err := q.bcheckExpr(o.AsExpr(), depth); err != nil {
				return bounds{}, err
			}
		}

	default:
		return bounds{}, fmt.Errorf("check: unrecognized token (0x%X) for bcheckExprOther", n.Operator())
	}
	return q.bcheckTypeExpr(n.MType())
}

func (q *checker) bcheckExprCall(n *a.Expr, depth uint32) error {
	// TODO: handle func pre/post conditions.
	lhs := n.LHS().AsExpr()
	f, err := q.c.resolveFunc(lhs.MType())
	if err != nil {
		return err
	}
	inFields := f.In().Fields()
	if len(inFields) != len(n.Args()) {
		return fmt.Errorf("check: %q has %d arguments but %d were given",
			lhs.MType().Str(q.tm), len(inFields), len(n.Args()))
	}
	for i, o := range n.Args() {
		if _, err := q.bcheckAssignment1(nil, inFields[i].AsField().XType(), t.IDEq, o.AsArg().Value()); err != nil {
			return err
		}
	}

	recv := lhs.LHS().AsExpr()
	if recv.MType().Decorator() != t.IDNptr {
		return nil
	}
	return q.proveRecvNotEqNullptr(recv)
}

var errNotASpecialCase = errors.New("not a special case")

func (q *checker) bcheckExprCallSpecialCases(n *a.Expr, depth uint32) (bounds, error) {
	lhs := n.LHS().AsExpr()
	recv := lhs.LHS().AsExpr()
	method := lhs.Ident()

	actualAdvanceIsAlwaysPositive, advance, advanceExpr, update := false, (*big.Int)(nil), (*a.Expr)(nil), false

	if recvTyp := recv.MType(); recvTyp == nil {
		return bounds{}, errNotASpecialCase

	} else if recvTyp.IsNumType() {
		// For a numeric type's low_bits, etc. methods. The bound on the output
		// is dependent on bound on the input, similar to dependent types, and
		// isn't expressible in Wuffs' function syntax and type system.
		switch method {
		case t.IDLowBits, t.IDHighBits:
			ab, err := q.bcheckExpr(n.Args()[0].AsArg().Value(), depth)
			if err != nil {
				return bounds{}, err
			}
			return bounds{
				zero,
				bitMask(int(ab[1].Int64())),
			}, nil

		case t.IDMin, t.IDMax:
			// TODO: lhs has already been bcheck'ed. There should be no
			// need to bcheck lhs.LHS().Expr() twice.
			lb, err := q.bcheckExpr(lhs.LHS().AsExpr(), depth)
			if err != nil {
				return bounds{}, err
			}
			ab, err := q.bcheckExpr(n.Args()[0].AsArg().Value(), depth)
			if err != nil {
				return bounds{}, err
			}
			if method == t.IDMin {
				return bounds{
					min(lb[0], ab[0]),
					min(lb[1], ab[1]),
				}, nil
			} else {
				return bounds{
					max(lb[0], ab[0]),
					max(lb[1], ab[1]),
				}, nil
			}
		}

	} else if recvTyp.IsIOTokenType() {
		if (method == t.IDUndoByte) || (method == t.IDPeekUndoByte) {
			if err := q.canUndoByte(recv, method == t.IDPeekUndoByte); err != nil {
				return bounds{}, err
			}

		} else if (method == t.IDLimitedCopyU32FromHistory8ByteChunksDistance1Fast) ||
			(method == t.IDLimitedCopyU32FromHistory8ByteChunksDistance1FastReturnCusp) {
			if err := q.canLimitedCopyU32FromHistoryFast(recv, n.Args(), eight, nil, one); err != nil {
				return bounds{}, err
			}

		} else if (method == t.IDLimitedCopyU32FromHistory8ByteChunksFast) ||
			(method == t.IDLimitedCopyU32FromHistory8ByteChunksFastReturnCusp) {
			if err := q.canLimitedCopyU32FromHistoryFast(recv, n.Args(), eight, eight, nil); err != nil {
				return bounds{}, err
			}

		} else if (method == t.IDLimitedCopyU32FromHistoryFast) ||
			(method == t.IDLimitedCopyU32FromHistoryFastReturnCusp) {
			if err := q.canLimitedCopyU32FromHistoryFast(recv, n.Args(), nil, one, nil); err != nil {
				return bounds{}, err
			}

		} else if method == t.IDSkipU32Fast {
			args := n.Args()
			if len(args) != 2 {
				return bounds{}, fmt.Errorf("check: internal error: bad skip_fast arguments")
			}
			actual := args[0].AsArg().Value()
			worstCase := args[1].AsArg().Value()
			if actual.Eq(worstCase) {
				// No-op. Proving "x <= x" is trivial.
			} else if err := q.proveBinaryOp(t.IDXBinaryLessEq, actual, worstCase); err == errFailed {
				return bounds{}, fmt.Errorf("check: could not prove skip_fast pre-condition: %s <= %s",
					actual.Str(q.tm), worstCase.Str(q.tm))
			} else if err != nil {
				return bounds{}, err
			}
			if cv := actual.ConstValue(); (cv != nil) && (cv.Sign() > 0) {
				actualAdvanceIsAlwaysPositive = true
			}
			if cv := worstCase.ConstValue(); cv != nil {
				advance, update = cv, true
			} else {
				advanceExpr, update = actual, true
			}

		} else if (method == t.IDPeekU8At) || (method == t.IDPeekU64LEAt) {
			args := n.Args()
			if len(args) != 1 {
				return bounds{}, fmt.Errorf("check: internal error: bad peek_uxx_at arguments")
			}
			offset := args[0].AsArg().Value()
			if offset.ConstValue() == nil {
				return bounds{}, fmt.Errorf("check: peek_uxx_at offset is not a constant value")
			}
			adv := int64(1)
			if method == t.IDPeekU64LEAt {
				adv = 8
			}
			advance, update = big.NewInt(adv), false
			advance.Add(advance, offset.ConstValue())

		} else if method >= t.IDPeekU8 {
			if m := method - t.IDPeekU8; m < t.ID(len(ioMethodAdvances)) {
				au := ioMethodAdvances[m]
				advance, update = au.advance, au.update
			}
		}

	} else if recvTyp.Eq(typeExprSliceU8) || recvTyp.Eq(typeExprRosliceU8) {
		if method >= t.IDPeekU8 {
			if m := method - t.IDPeekU8; m < t.ID(len(ioMethodAdvances)) {
				au := ioMethodAdvances[m]
				advance, update = au.advance, au.update
			}
		}

	} else if recvTyp.IsCPUArchType() {
		if s := method.Str(q.tm); strings.HasPrefix(s, "make_") || strings.HasPrefix(s, "store_") {
			switch {
			case strings.HasSuffix(s, "_slice64"): //   64 bits is  8 bytes.
				advance = eight
			case strings.HasSuffix(s, "_slice128"): // 128 bits is 16 bytes.
				advance = sixteen
			case strings.HasSuffix(s, "_slice256"): // 256 bits is 32 bytes.
				advance = thirtyTwo
			case strings.HasSuffix(s, "_slice512"): // 512 bits is 64 bytes.
				advance = sixtyFour
			case strings.HasSuffix(s, "_slice_u16lex4"): // 4 u16 values is 8 bytes.
				advance = four
			case strings.HasSuffix(s, "_slice_u16lex8"): // 8 u16 values is 16 bytes.
				advance = eight
			case strings.HasSuffix(s, "_slice_u16lex16"): // 16 u16 values is 32 bytes.
				advance = sixteen
			case strings.HasSuffix(s, "_slice_u16lex32"): // 32 u16 values is 64 bytes.
				advance = thirtyTwo
			case strings.Contains(s, "_slice"):
				return bounds{}, fmt.Errorf("check: internal error: unrecognized %s method", s)
			}
		}
	}

	if (advance != nil) || (advanceExpr != nil) {
		subject := recv
		if recv.MType().IsCPUArchType() {
			subject = n.Args()[0].AsArg().Value()
		}
		if ok, err := q.optimizeIOMethodAdvance(subject, advance, advanceExpr, update); err != nil {
			return bounds{}, err
		} else if !ok {
			adv := ""
			if advance != nil {
				adv = advance.String()
			} else {
				adv = advanceExpr.Str(q.tm)
			}
			return bounds{}, fmt.Errorf("check: could not prove %s pre-condition: %s.length() >= %s",
				method.Str(q.tm), subject.Str(q.tm), adv)
		}
		if actualAdvanceIsAlwaysPositive {
			q.facts = append(q.facts, makeCanUndoByte(recv))
		}
		// TODO: drop other subject-related facts?
	}

	return bounds{}, errNotASpecialCase
}

func (q *checker) canUndoByte(recv *a.Expr, justPeeking bool) error {
	for _, x := range q.facts {
		if lhs, meth, args, _ := x.IsMethodCall(); (meth != t.IDCanUndoByte) || (len(args) != 0) ||
			!lhs.Eq(recv) {
			continue
		} else if justPeeking {
			return nil
		}
		return q.facts.update(func(o *a.Expr) (*a.Expr, error) {
			if o.Mentions(recv) {
				return nil, nil
			}
			return o, nil
		})
	}
	return fmt.Errorf("check: could not prove %s.can_undo_byte()", recv.Str(q.tm))
}

func (q *checker) canLimitedCopyU32FromHistoryFast(recv *a.Expr, args []*a.Node, adj *big.Int, minDistance *big.Int, exactDistance *big.Int) error {
	// As per cgen's io-private.h, there are three pre-conditions:
	//  - upTo >= one
	//  - (upTo + adj) <= this.length()
	//  - either (distance >= minDistance) or (distance == exactDistance),
	//    depending on which of minDistance and exactDistance is non-nil.
	//  - distance <= this.history_length()
	//
	// adj may be nil, in which case (upTo + adj) is just upTo.

	if len(args) != 2 {
		return fmt.Errorf("check: internal error: inconsistent limited_copy_u32_from_history_fast arguments")
	}
	upTo := args[0].AsArg().Value()
	distance := args[1].AsArg().Value()

	// Check "upto >= one".
check0:
	for {
		for _, x := range q.facts {
			if x.Operator() != t.IDXBinaryGreaterEq {
				continue
			}
			if lhs := x.LHS().AsExpr(); !lhs.Eq(upTo) {
				continue
			}
			if rcv := x.RHS().AsExpr().ConstValue(); (rcv == nil) || (rcv.Sign() <= 0) {
				continue
			}
			break check0
		}
		return fmt.Errorf("check: could not prove %s >= 1", upTo.Str(q.tm))
	}

	// Check "upTo <= this.length()".
check1:
	for {
		for _, x := range q.facts {
			if x.Operator() != t.IDXBinaryLessEq {
				continue
			}

			// Check that the LHS is "(upTo + adj) as base.u64".
			lhs := x.LHS().AsExpr()
			if lhs.Operator() != t.IDXBinaryAs {
				continue
			}
			llhs, lrhs := lhs.LHS().AsExpr(), lhs.RHS().AsTypeExpr()
			if !lrhs.Eq(typeExprU64) {
				continue
			}
			if adj == nil {
				if !llhs.Eq(upTo) {
					continue
				}
			} else {
				if (llhs.Operator() != t.IDXBinaryPlus) || !llhs.LHS().AsExpr().Eq(upTo) {
					continue
				} else if cv := llhs.RHS().AsExpr().ConstValue(); (cv == nil) || (cv.Cmp(adj) != 0) {
					continue
				}
			}

			// Check that the RHS is "recv.length()".
			y, method, yArgs := splitReceiverMethodArgs(x.RHS().AsExpr())
			if method != t.IDLength || len(yArgs) != 0 {
				continue
			}
			if !y.Eq(recv) {
				continue
			}

			break check1
		}
		if adj == nil {
			return fmt.Errorf("check: could not prove (%s as base.u64) <= %s.length()",
				upTo.Str(q.tm), recv.Str(q.tm))
		}
		return fmt.Errorf("check: could not prove ((%s + %v) as base.u64) <= %s.length()",
			upTo.Str(q.tm), adj, recv.Str(q.tm))
	}

	// Check "distance >= minDistance".
check2a:
	for minDistance != nil {
		for _, x := range q.facts {
			if x.Operator() != t.IDXBinaryGreaterEq {
				continue
			}
			if lhs := x.LHS().AsExpr(); !lhs.Eq(distance) {
				continue
			}
			if rcv := x.RHS().AsExpr().ConstValue(); (rcv == nil) || (rcv.Cmp(minDistance) < 0) {
				continue
			}
			break check2a
		}
		return fmt.Errorf("check: could not prove %s >= %v", distance.Str(q.tm), minDistance)
	}

	// Check "distance == exactDistance".
check2b:
	for exactDistance != nil {
		for _, x := range q.facts {
			if x.Operator() != t.IDXBinaryEqEq {
				continue
			}
			if lhs := x.LHS().AsExpr(); !lhs.Eq(distance) {
				continue
			}
			if rcv := x.RHS().AsExpr().ConstValue(); (rcv == nil) || (rcv.Cmp(exactDistance) != 0) {
				continue
			}
			break check2b
		}
		return fmt.Errorf("check: could not prove %s == %v", distance.Str(q.tm), exactDistance)
	}

	// Check "distance <= this.history_length()".
check3:
	for {
		for _, x := range q.facts {
			if x.Operator() != t.IDXBinaryLessEq {
				continue
			}

			// Check that the LHS is "distance as base.u64".
			lhs := x.LHS().AsExpr()
			if lhs.Operator() != t.IDXBinaryAs {
				continue
			}
			llhs, lrhs := lhs.LHS().AsExpr(), lhs.RHS().AsTypeExpr()
			if !llhs.Eq(distance) || !lrhs.Eq(typeExprU64) {
				continue
			}

			// Check that the RHS is "recv.history_length()".
			y, method, yArgs := splitReceiverMethodArgs(x.RHS().AsExpr())
			if method != t.IDHistoryLength || len(yArgs) != 0 {
				continue
			}
			if !y.Eq(recv) {
				continue
			}

			break check3
		}
		return fmt.Errorf("check: could not prove %s <= %s.history_length()",
			distance.Str(q.tm), recv.Str(q.tm))
	}

	return nil
}

var ioMethodAdvances = [...]struct {
	advance *big.Int
	update  bool
}{
	t.IDPeekU8 - t.IDPeekU8: {one, false},

	t.IDPeekU16BE - t.IDPeekU8: {two, false},
	t.IDPeekU16LE - t.IDPeekU8: {two, false},

	t.IDPeekU8AsU32 - t.IDPeekU8:    {one, false},
	t.IDPeekU16BEAsU32 - t.IDPeekU8: {two, false},
	t.IDPeekU16LEAsU32 - t.IDPeekU8: {two, false},
	t.IDPeekU24BEAsU32 - t.IDPeekU8: {three, false},
	t.IDPeekU24LEAsU32 - t.IDPeekU8: {three, false},
	t.IDPeekU32BE - t.IDPeekU8:      {four, false},
	t.IDPeekU32LE - t.IDPeekU8:      {four, false},

	t.IDPeekU8AsU64 - t.IDPeekU8:    {one, false},
	t.IDPeekU16BEAsU64 - t.IDPeekU8: {two, false},
	t.IDPeekU16LEAsU64 - t.IDPeekU8: {two, false},
	t.IDPeekU24BEAsU64 - t.IDPeekU8: {three, false},
	t.IDPeekU24LEAsU64 - t.IDPeekU8: {three, false},
	t.IDPeekU32BEAsU64 - t.IDPeekU8: {four, false},
	t.IDPeekU32LEAsU64 - t.IDPeekU8: {four, false},
	t.IDPeekU40BEAsU64 - t.IDPeekU8: {five, false},
	t.IDPeekU40LEAsU64 - t.IDPeekU8: {five, false},
	t.IDPeekU48BEAsU64 - t.IDPeekU8: {six, false},
	t.IDPeekU48LEAsU64 - t.IDPeekU8: {six, false},
	t.IDPeekU56BEAsU64 - t.IDPeekU8: {seven, false},
	t.IDPeekU56LEAsU64 - t.IDPeekU8: {seven, false},
	t.IDPeekU64BE - t.IDPeekU8:      {eight, false},
	t.IDPeekU64LE - t.IDPeekU8:      {eight, false},

	t.IDPokeU8 - t.IDPeekU8:    {one, false},
	t.IDPokeU16BE - t.IDPeekU8: {two, false},
	t.IDPokeU16LE - t.IDPeekU8: {two, false},
	t.IDPokeU24BE - t.IDPeekU8: {three, false},
	t.IDPokeU24LE - t.IDPeekU8: {three, false},
	t.IDPokeU32BE - t.IDPeekU8: {four, false},
	t.IDPokeU32LE - t.IDPeekU8: {four, false},
	t.IDPokeU40BE - t.IDPeekU8: {five, false},
	t.IDPokeU40LE - t.IDPeekU8: {five, false},
	t.IDPokeU48BE - t.IDPeekU8: {six, false},
	t.IDPokeU48LE - t.IDPeekU8: {six, false},
	t.IDPokeU56BE - t.IDPeekU8: {seven, false},
	t.IDPokeU56LE - t.IDPeekU8: {seven, false},
	t.IDPokeU64BE - t.IDPeekU8: {eight, false},
	t.IDPokeU64LE - t.IDPeekU8: {eight, false},

	t.IDWriteU8Fast - t.IDPeekU8:    {one, true},
	t.IDWriteU16BEFast - t.IDPeekU8: {two, true},
	t.IDWriteU16LEFast - t.IDPeekU8: {two, true},
	t.IDWriteU24BEFast - t.IDPeekU8: {three, true},
	t.IDWriteU24LEFast - t.IDPeekU8: {three, true},
	t.IDWriteU32BEFast - t.IDPeekU8: {four, true},
	t.IDWriteU32LEFast - t.IDPeekU8: {four, true},
	t.IDWriteU40BEFast - t.IDPeekU8: {five, true},
	t.IDWriteU40LEFast - t.IDPeekU8: {five, true},
	t.IDWriteU48BEFast - t.IDPeekU8: {six, true},
	t.IDWriteU48LEFast - t.IDPeekU8: {six, true},
	t.IDWriteU56BEFast - t.IDPeekU8: {seven, true},
	t.IDWriteU56LEFast - t.IDPeekU8: {seven, true},
	t.IDWriteU64BEFast - t.IDPeekU8: {eight, true},
	t.IDWriteU64LEFast - t.IDPeekU8: {eight, true},

	t.IDWriteSimpleTokenFast - t.IDPeekU8:   {one, true},
	t.IDWriteExtendedTokenFast - t.IDPeekU8: {one, true},
}

func makeConstValueExpr(tm *t.Map, cv *big.Int) (*a.Expr, error) {
	id, err := tm.Insert(cv.String())
	if err != nil {
		return nil, err
	}
	o := a.NewExpr(0, 0, id, nil, nil, nil, nil)
	o.SetConstValue(cv)
	o.SetMBounds(bounds{cv, cv})
	o.SetMType(typeExprIdeal)
	return o, nil
}

// makeCanUndoByte returns "x.can_undo_byte()".
func makeCanUndoByte(x *a.Expr) *a.Expr {
	ret := a.NewExpr(0, t.IDDot, t.IDCanUndoByte, x.AsNode(), nil, nil, nil)
	ret.SetMBounds(bounds{funcBounds, funcBounds})
	ret.SetMType(a.NewTypeExpr(t.IDFunc, 0, t.IDCanUndoByte, x.MType().AsNode(), nil, nil))
	ret = a.NewExpr(0, t.IDOpenParen, 0, ret.AsNode(), nil, nil, nil)
	// TODO: call SetMBounds?
	ret.SetMType(typeExprBool)
	return ret
}

// makeSliceLength returns "x.length()".
func makeSliceLength(x *a.Expr) *a.Expr {
	ret := a.NewExpr(0, t.IDDot, t.IDLength, x.AsNode(), nil, nil, nil)
	ret.SetMBounds(bounds{funcBounds, funcBounds})
	ret.SetMType(a.NewTypeExpr(t.IDFunc, 0, t.IDLength, x.MType().AsNode(), nil, nil))
	ret = a.NewExpr(0, t.IDOpenParen, 0, ret.AsNode(), nil, nil, nil)
	// TODO: call SetMBounds?
	ret.SetMType(typeExprU64)
	return ret
}

// makeSliceLengthEqEq returns "x.length() == n".
func (q *checker) makeSliceLengthEqEq(x *a.Expr, n t.ID) *a.Expr {
	lhs := makeSliceLength(x)

	nValue, err := strconv.Atoi(n.Str(q.tm))
	if err != nil {
		panic("check: internal error: makeSliceLengthEqEq called but not with a small integer")
	}
	cv := big.NewInt(int64(nValue))

	rhs := a.NewExpr(0, 0, n, nil, nil, nil, nil)
	rhs.SetConstValue(cv)
	rhs.SetMBounds(bounds{cv, cv})
	rhs.SetMType(typeExprIdeal)

	ret := a.NewExpr(0, t.IDXBinaryEqEq, 0, lhs.AsNode(), nil, rhs.AsNode(), nil)
	ret.SetMBounds(bounds{zero, one})
	ret.SetMType(typeExprBool)
	return ret
}

func (q *checker) bcheckExprUnaryOp(n *a.Expr, depth uint32) (bounds, error) {
	rb, err := q.bcheckExpr(n.RHS().AsExpr(), depth)
	if err != nil {
		return bounds{}, err
	}

	switch n.Operator() {
	case t.IDXUnaryPlus:
		return rb, nil
	case t.IDXUnaryMinus:
		return bounds{neg(rb[1]), neg(rb[0])}, nil
	case t.IDXUnaryNot:
		return bounds{zero, one}, nil
	}

	return bounds{}, fmt.Errorf("check: unrecognized token (0x%X) for bcheckExprUnaryOp", n.Operator())
}

func (q *checker) bcheckExprXBinaryPlus(lhs *a.Expr, lb bounds, rhs *a.Expr, rb bounds) (bounds, error) {
	return lb.Add(rb), nil
}

func (q *checker) bcheckExprXBinaryMinus(lhs *a.Expr, lb bounds, rhs *a.Expr, rb bounds) (bounds, error) {
	nb := lb.Sub(rb)
	for _, x := range q.facts {
		xOp, xLHS, xRHS := parseBinaryOp(x)
		if !lhs.Eq(xLHS) || !rhs.Eq(xRHS) {
			continue
		}
		switch xOp {
		case t.IDXBinaryLessThan:
			nb[1] = min(nb[1], minusOne)
		case t.IDXBinaryLessEq:
			nb[1] = min(nb[1], zero)
		case t.IDXBinaryGreaterEq:
			nb[0] = max(nb[0], zero)
		case t.IDXBinaryGreaterThan:
			nb[0] = max(nb[0], one)
		}
	}
	return nb, nil
}

func (q *checker) bcheckExprBinaryOp(op t.ID, lhs *a.Expr, rhs *a.Expr, depth uint32) (bounds, error) {
	lb, err := q.bcheckExpr(lhs, depth)
	if err != nil {
		return bounds{}, err
	}
	return q.bcheckExprBinaryOp1(op, lhs, lb, rhs, depth)
}

func (q *checker) bcheckExprBinaryOp1(op t.ID, lhs *a.Expr, lb bounds, rhs *a.Expr, depth uint32) (bounds, error) {
	rb, err := q.bcheckExpr(rhs, depth)
	if err != nil {
		return bounds{}, err
	}

	switch op {
	case t.IDXBinaryPlus:
		return q.bcheckExprXBinaryPlus(lhs, lb, rhs, rb)

	case t.IDXBinaryMinus:
		return q.bcheckExprXBinaryMinus(lhs, lb, rhs, rb)

	case t.IDXBinaryStar:
		return lb.Mul(rb), nil

	case t.IDXBinarySlash, t.IDXBinaryPercent:
		// Prohibit division by zero.
		if lb[0].Sign() < 0 {
			return bounds{}, fmt.Errorf("check: divide/modulus op argument %q is possibly negative", lhs.Str(q.tm))
		}
		if rb[0].Sign() <= 0 {
			return bounds{}, fmt.Errorf("check: divide/modulus op argument %q is possibly non-positive", rhs.Str(q.tm))
		}
		if op == t.IDXBinarySlash {
			nb, _ := lb.TryQuo(rb)
			return nb, nil
		}
		return bounds{
			zero,
			big.NewInt(0).Sub(rb[1], one),
		}, nil

	case t.IDXBinaryShiftL, t.IDXBinaryTildeModShiftL, t.IDXBinaryShiftR:
		shiftBounds := bounds{}
		typeBounds := bounds{}
		if lTyp := lhs.MType(); lTyp.IsNumType() {
			id := int(lTyp.QID()[1])
			if id < len(numShiftBounds) {
				shiftBounds = numShiftBounds[id]
			}
			if id < len(numTypeBounds) {
				typeBounds = numTypeBounds[id]
			}
		}
		if shiftBounds[0] == nil {
			return bounds{}, fmt.Errorf("check: shift op argument %q of type %q does not have unsigned integer type",
				lhs.Str(q.tm), lhs.MType().Str(q.tm))
		} else if !shiftBounds.ContainsIntRange(rb) {
			return bounds{}, fmt.Errorf("check: shift op argument %q is outside the range %s", rhs.Str(q.tm), shiftBounds)
		}

		switch op {
		case t.IDXBinaryShiftL:
			nb, _ := lb.TryLsh(rb)
			return nb, nil
		case t.IDXBinaryTildeModShiftL:
			nb, _ := lb.TryLsh(rb)
			nb[1] = min(nb[1], typeBounds[1])
			return nb, nil
		case t.IDXBinaryShiftR:
			nb, _ := lb.TryRsh(rb)
			return nb, nil
		}

	case t.IDXBinaryAmp, t.IDXBinaryPipe, t.IDXBinaryHat:
		// TODO: should type-checking ensure that bitwise ops only apply to
		// *unsigned* integer types?
		if lb[0].Sign() < 0 {
			return bounds{}, fmt.Errorf("check: bitwise op argument %q is possibly negative", lhs.Str(q.tm))
		}
		if rb[0].Sign() < 0 {
			return bounds{}, fmt.Errorf("check: bitwise op argument %q is possibly negative", rhs.Str(q.tm))
		}
		switch op {
		case t.IDXBinaryAmp:
			return lb.And(rb), nil
		case t.IDXBinaryPipe:
			return lb.Or(rb), nil
		case t.IDXBinaryHat:
			z := max(lb[1], rb[1])
			// Return [0, z rounded up to the next power-of-2-minus-1]. This is
			// conservative, but works fine in practice.
			return bounds{
				zero,
				bitMask(z.BitLen()),
			}, nil
		}

	case t.IDXBinaryTildeModPlus, t.IDXBinaryTildeModMinus, t.IDXBinaryTildeModStar:
		typ := lhs.MType()
		if typ.IsIdeal() {
			typ = rhs.MType()
		}
		if qid := typ.QID(); qid[0] == t.IDBase {
			return numTypeBounds[qid[1]], nil
		}

	case t.IDXBinaryTildeSatPlus, t.IDXBinaryTildeSatMinus:
		typ := lhs.MType()
		if typ.IsIdeal() {
			typ = rhs.MType()
		}
		if qid := typ.QID(); qid[0] == t.IDBase {
			b := numTypeBounds[qid[1]]

			nFunc := (*checker).bcheckExprXBinaryPlus
			if op != t.IDXBinaryTildeSatPlus {
				nFunc = (*checker).bcheckExprXBinaryMinus
			}
			nb, err := nFunc(q, lhs, lb, rhs, rb)
			if err != nil {
				return bounds{}, err
			}

			if op == t.IDXBinaryTildeSatPlus {
				nb[0] = min(nb[0], b[1])
				nb[1] = min(nb[1], b[1])
			} else {
				nb[0] = max(nb[0], b[0])
				nb[1] = max(nb[1], b[0])
			}
			return nb, nil
		}

	case t.IDXBinaryNotEq, t.IDXBinaryLessThan, t.IDXBinaryLessEq, t.IDXBinaryEqEq,
		t.IDXBinaryGreaterEq, t.IDXBinaryGreaterThan, t.IDXBinaryAnd, t.IDXBinaryOr:
		return bounds{zero, one}, nil

	case t.IDXBinaryAs:
		// Unreachable, as this is checked by the caller.
	}
	return bounds{}, fmt.Errorf("check: unrecognized token (0x%X) for bcheckExprBinaryOp", op)
}

func (q *checker) bcheckExprAssociativeOp(n *a.Expr, depth uint32) (bounds, error) {
	op := n.Operator().AmbiguousForm().BinaryForm()
	if op == 0 {
		return bounds{}, fmt.Errorf(
			"check: unrecognized token (0x%X) for bcheckExprAssociativeOp", n.Operator())
	}
	args := n.Args()
	if len(args) < 1 {
		return bounds{}, fmt.Errorf("check: associative op has no arguments")
	}
	lb, err := q.bcheckExpr(args[0].AsExpr(), depth)
	if err != nil {
		return bounds{}, err
	}
	for i, o := range args {
		if i == 0 {
			continue
		}
		lhs := a.NewExpr(n.AsNode().AsRaw().Flags(),
			n.Operator(), n.Ident(), n.LHS(), n.MHS(), n.RHS(), args[:i])
		lb, err = q.bcheckExprBinaryOp1(op, lhs, lb, o.AsExpr(), depth)
		if err != nil {
			return bounds{}, err
		}
	}
	return lb, nil
}

func (q *checker) bcheckTypeExpr(typ *a.TypeExpr) (bounds, error) {
	if b := typ.AsNode().MBounds(); b[0] != nil {
		return b, nil
	}
	b, err := q.bcheckTypeExpr1(typ)
	if err != nil {
		return bounds{}, err
	}
	typ.AsNode().SetMBounds(b)
	return b, nil
}

func (q *checker) bcheckTypeExpr1(typ *a.TypeExpr) (bounds, error) {
	if typ.IsIdeal() {
		return bounds{minIdeal, maxIdeal}, nil
	}

	if innTyp := typ.Inner(); innTyp != nil {
		if _, err := q.bcheckTypeExpr(innTyp); err != nil {
			return bounds{}, err
		}
	}

	switch typ.Decorator() {
	case 0:
		// No-op.
	case t.IDArray, t.IDRoarray:
		if _, err := q.bcheckExpr(typ.ArrayLength(), 0); err != nil {
			return bounds{}, err
		}
		return bounds{zero, zero}, nil
	case t.IDFunc:
		if _, err := q.bcheckTypeExpr(typ.Receiver()); err != nil {
			return bounds{}, err
		}
		return bounds{one, funcBounds}, nil
	case t.IDNptr:
		return bounds{zero, maxPointerBounds}, nil
	case t.IDPtr:
		return bounds{one, maxPointerBounds}, nil
	case t.IDRoslice, t.IDRotable, t.IDSlice, t.IDTable:
		return bounds{zero, zero}, nil
	default:
		return bounds{}, fmt.Errorf("check: internal error: unrecognized decorator")
	}

	b := bounds{zero, zero}

	if qid := typ.QID(); qid[0] == t.IDBase {
		if qid[1] == t.IDDagger1 || qid[1] == t.IDDagger2 {
			return bounds{zero, zero}, nil
		} else if qid[1] < t.ID(len(numTypeBounds)) {
			if x := numTypeBounds[qid[1]]; x[0] != nil {
				b = x
			}
		}
	}

	if typ.IsRefined() {
		if x := typ.Min(); x != nil {
			if _, err := q.bcheckExpr(x, 0); err != nil {
				return bounds{}, err
			}
			if cv := x.ConstValue(); cv == nil {
				return bounds{}, fmt.Errorf("check: internal error: refinement has no const-value")
			} else if cv.Cmp(b[0]) < 0 {
				return bounds{}, fmt.Errorf("check: type refinement %v for %q is out of bounds", cv, typ.Str(q.tm))
			} else {
				b[0] = cv
			}
		}

		if x := typ.Max(); x != nil {
			if _, err := q.bcheckExpr(x, 0); err != nil {
				return bounds{}, err
			}
			if cv := x.ConstValue(); cv == nil {
				return bounds{}, fmt.Errorf("check: internal error: refinement has no const-value")
			} else if cv.Cmp(b[1]) > 0 {
				return bounds{}, fmt.Errorf("check: type refinement %v for %q is out of bounds", cv, typ.Str(q.tm))
			} else {
				b[1] = cv
			}
		}
	}

	return b, nil
}
