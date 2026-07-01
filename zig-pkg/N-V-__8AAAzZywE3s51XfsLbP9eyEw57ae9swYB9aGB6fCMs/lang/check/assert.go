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

	a "github.com/google/wuffs/lang/ast"
	t "github.com/google/wuffs/lang/token"
)

// otherHandSide returns the operator and other hand side when n is an
// binary-op expression like "thisHS == thatHS" or "thatHS < thisHS" (which is
// equivalent to "thisHS > thatHS"). If not, it returns (0, nil).
func otherHandSide(n *a.Expr, thisHS *a.Expr) (op t.ID, thatHS *a.Expr) {
	op = n.Operator()

	reverseOp := t.ID(0)
	switch op {
	case t.IDXBinaryNotEq:
		reverseOp = t.IDXBinaryNotEq
	case t.IDXBinaryLessThan:
		reverseOp = t.IDXBinaryGreaterThan
	case t.IDXBinaryLessEq:
		reverseOp = t.IDXBinaryGreaterEq
	case t.IDXBinaryEqEq:
		reverseOp = t.IDXBinaryEqEq
	case t.IDXBinaryGreaterEq:
		reverseOp = t.IDXBinaryLessEq
	case t.IDXBinaryGreaterThan:
		reverseOp = t.IDXBinaryLessThan
	}

	if reverseOp != 0 {
		if thisHS.Eq(n.LHS().AsExpr()) {
			return op, n.RHS().AsExpr()
		}
		if thisHS.Eq(n.RHS().AsExpr()) {
			return reverseOp, n.LHS().AsExpr()
		}
	}
	return 0, nil
}

type facts []*a.Expr

func (z *facts) appendBinaryOpFact(op t.ID, lhs *a.Expr, rhs *a.Expr) {
	o := a.NewExpr(0, op, 0, lhs.AsNode(), nil, rhs.AsNode(), nil)
	o.SetMBounds(bounds{zero, one})
	o.SetMType(typeExprBool)
	z.appendFact(o)
}

func (z *facts) appendFact(fact *a.Expr) {
	// TODO: make this faster than O(N) by keeping facts sorted somehow?
	for _, x := range *z {
		if x.Eq(fact) {
			return
		}
	}

	switch fact.Operator() {
	case t.IDXBinaryAnd:
		z.appendFact(fact.LHS().AsExpr())
		z.appendFact(fact.RHS().AsExpr())
		return
	case t.IDXAssociativeAnd:
		for _, a := range fact.Args() {
			z.appendFact(a.AsExpr())
		}
		return
	}

	*z = append(*z, fact)
}

func (z *facts) dropAnyFactsMentioning(n *a.Expr) error {
	return z.update(func(x *a.Expr) (*a.Expr, error) {
		if x.Mentions(n) {
			return nil, nil
		}
		return x, nil
	})
}

// update applies f to each fact, replacing the slice element with the result
// of the function call. The slice is then compacted to remove all nils.
func (z *facts) update(f func(*a.Expr) (*a.Expr, error)) error {
	i := 0
	for _, x := range *z {
		x, err := f(x)
		if err != nil {
			return err
		}
		if x != nil {
			(*z)[i] = x
			i++
		}
	}
	for j := i; j < len(*z); j++ {
		(*z)[j] = nil
	}
	*z = (*z)[:i]
	return nil
}

func (z facts) refine(n *a.Expr, nb bounds, tm *t.Map) (bounds, error) {
	if nb[0] == nil || nb[1] == nil {
		return nb, nil
	}

	for _, x := range z {
		op, other := otherHandSide(x, n)
		if op == 0 {
			continue
		}
		cv := other.ConstValue()
		if cv == nil {
			continue
		}

		originalNB, changed := nb, false
		switch op {
		case t.IDXBinaryNotEq:
			if nb[0].Cmp(cv) == 0 {
				nb[0] = add1(nb[0])
				changed = true
			} else if nb[1].Cmp(cv) == 0 {
				nb[1] = sub1(nb[1])
				changed = true
			}
		case t.IDXBinaryLessThan:
			if nb[1].Cmp(cv) >= 0 {
				nb[1] = sub1(cv)
				changed = true
			}
		case t.IDXBinaryLessEq:
			if nb[1].Cmp(cv) > 0 {
				nb[1] = cv
				changed = true
			}
		case t.IDXBinaryEqEq:
			nb[0], nb[1] = cv, cv
			changed = true
		case t.IDXBinaryGreaterEq:
			if nb[0].Cmp(cv) < 0 {
				nb[0] = cv
				changed = true
			}
		case t.IDXBinaryGreaterThan:
			if nb[0].Cmp(cv) <= 0 {
				nb[0] = add1(cv)
				changed = true
			}
		}

		if changed && nb[0].Cmp(nb[1]) > 0 {
			return bounds{}, fmt.Errorf("check: expression %q bounds %v inconsistent with fact %q",
				n.Str(tm), originalNB, x.Str(tm))
		}
	}

	return nb, nil
}

// simplify returns a simplified form of n. For example, (x - x) becomes 0.
func simplify(tm *t.Map, n *a.Expr) (*a.Expr, error) {
	// TODO: be rigorous about this, not ad hoc.
	op, lhs, rhs := parseBinaryOp(n)
	if lhs != nil && rhs != nil {
		if lcv, rcv := lhs.ConstValue(), rhs.ConstValue(); lcv != nil && rcv != nil {
			ncv, err := evalConstValueBinaryOp(tm, n, lcv, rcv)
			if err != nil {
				return nil, err
			}
			return makeConstValueExpr(tm, ncv)
		}
	}

	switch op {
	case t.IDXBinaryPlus:
		// TODO: more constant folding, so ((x + 1) + 1) becomes (x + 2).

	case t.IDXBinaryMinus:
		if lhs.Eq(rhs) {
			return zeroExpr, nil
		}
		if lOp, lLHS, lRHS := parseBinaryOp(lhs); lOp == t.IDXBinaryPlus {
			if lLHS.Eq(rhs) {
				return lRHS, nil
			}
			if lRHS.Eq(rhs) {
				return lLHS, nil
			}
		}

	case t.IDXBinaryNotEq, t.IDXBinaryLessThan, t.IDXBinaryLessEq,
		t.IDXBinaryEqEq, t.IDXBinaryGreaterEq, t.IDXBinaryGreaterThan:

		l, err := simplify(tm, lhs)
		if err != nil {
			return nil, err
		}
		r, err := simplify(tm, rhs)
		if err != nil {
			return nil, err
		}
		if l != lhs || r != rhs {
			o := a.NewExpr(0, op, 0, l.AsNode(), nil, r.AsNode(), nil)
			o.SetConstValue(n.ConstValue())
			o.SetMType(n.MType())
			return o, nil
		}
	}
	return n, nil
}

func argValue(tm *t.Map, args []*a.Node, name string) *a.Expr {
	if x := tm.ByName(name); x != 0 {
		for _, a := range args {
			if a.AsArg().Name() == x {
				return a.AsArg().Value()
			}
		}
	}
	return nil
}

// parseBinaryOp parses n as "lhs op rhs".
func parseBinaryOp(n *a.Expr) (op t.ID, lhs *a.Expr, rhs *a.Expr) {
	if !n.Operator().IsBinaryOp() {
		return 0, nil, nil
	}
	op = n.Operator()
	if op == t.IDAs {
		return 0, nil, nil
	}
	return op, n.LHS().AsExpr(), n.RHS().AsExpr()
}

func proveBinaryOpConstValues(op t.ID, lb bounds, rb bounds) (ok bool) {
	switch op {
	case t.IDXBinaryNotEq:
		return lb[1].Cmp(rb[0]) < 0 || lb[0].Cmp(rb[1]) > 0
	case t.IDXBinaryLessThan:
		return lb[1].Cmp(rb[0]) < 0
	case t.IDXBinaryLessEq:
		return lb[1].Cmp(rb[0]) <= 0
	case t.IDXBinaryEqEq:
		return lb[0].Cmp(rb[1]) == 0 && lb[1].Cmp(rb[0]) == 0
	case t.IDXBinaryGreaterEq:
		return lb[0].Cmp(rb[1]) >= 0
	case t.IDXBinaryGreaterThan:
		return lb[0].Cmp(rb[1]) > 0
	}
	return false
}

func (q *checker) proveBinaryOp(op t.ID, lhs *a.Expr, rhs *a.Expr) error {
	lcv := lhs.ConstValue()
	if lcv != nil {
		rb, err := q.bcheckExpr(rhs, 0)
		if err != nil {
			return err
		}
		if proveBinaryOpConstValues(op, bounds{lcv, lcv}, rb) {
			return nil
		}
	}
	rcv := rhs.ConstValue()
	if rcv != nil {
		lb, err := q.bcheckExpr(lhs, 0)
		if err != nil {
			return err
		}
		if proveBinaryOpConstValues(op, lb, bounds{rcv, rcv}) {
			return nil
		}
	}

	for _, x := range q.facts {
		if !x.LHS().AsExpr().Eq(lhs) {
			continue
		}
		factOp := x.Operator()
		if opImpliesOp(factOp, op) && x.RHS().AsExpr().Eq(rhs) {
			return nil
		}

		if factOp == t.IDXBinaryEqEq && rcv != nil {
			if factCV := x.RHS().AsExpr().ConstValue(); factCV != nil {
				switch op {
				case t.IDXBinaryNotEq:
					return errFailedOrNil(factCV.Cmp(rcv) != 0)
				case t.IDXBinaryLessThan:
					return errFailedOrNil(factCV.Cmp(rcv) < 0)
				case t.IDXBinaryLessEq:
					return errFailedOrNil(factCV.Cmp(rcv) <= 0)
				case t.IDXBinaryEqEq:
					return errFailedOrNil(factCV.Cmp(rcv) == 0)
				case t.IDXBinaryGreaterEq:
					return errFailedOrNil(factCV.Cmp(rcv) >= 0)
				case t.IDXBinaryGreaterThan:
					return errFailedOrNil(factCV.Cmp(rcv) > 0)
				}
			}
		}
	}
	return errFailed
}

// opImpliesOp returns whether the first op implies the second. For example,
// knowing "x < y" implies that "x != y" and "x <= y".
func opImpliesOp(op0 t.ID, op1 t.ID) bool {
	if op0 == op1 {
		return true
	}
	switch op0 {
	case t.IDXBinaryLessThan:
		return op1 == t.IDXBinaryNotEq || op1 == t.IDXBinaryLessEq
	case t.IDXBinaryGreaterThan:
		return op1 == t.IDXBinaryNotEq || op1 == t.IDXBinaryGreaterEq
	}
	return false
}

func errFailedOrNil(ok bool) error {
	if ok {
		return nil
	}
	return errFailed
}

var errFailed = errors.New("failed")

func proveReasonRequirement(q *checker, op t.ID, lhs *a.Expr, rhs *a.Expr) error {
	if !op.IsXBinaryOp() {
		return fmt.Errorf(
			"check: internal error: proveReasonRequirement token (0x%X) is not an XBinaryOp", op)
	}
	if err := q.proveBinaryOp(op, lhs, rhs); err != nil {
		n := a.NewExpr(0, op, 0, lhs.AsNode(), nil, rhs.AsNode(), nil)
		return fmt.Errorf("cannot prove %q: %v", n.Str(q.tm), err)
	}
	return nil
}

func proveReasonRequirementForRHSLength(q *checker, op t.ID, lhs *a.Expr, rhs *a.Expr) error {
	if err := proveReasonRequirement(q, op, lhs, rhs); err != nil {
		if (op == t.IDXBinaryLessThan) || (op == t.IDXBinaryLessEq) {
			for _, x := range q.facts {
				// Try to prove "lhs op rhs" by proving "lhs op const", given a
				// fact x of the form "rhs >= const".
				if (x.Operator() == t.IDXBinaryGreaterEq) && x.LHS().AsExpr().Eq(rhs) &&
					(x.RHS().AsExpr().ConstValue() != nil) &&
					(proveReasonRequirement(q, op, lhs, x.RHS().AsExpr()) == nil) {

					return nil
				}
			}
		}
		return err
	}
	return nil
}

func (q *checker) proveRecvNotEqNullptr(recv *a.Expr) error {
	for _, x := range q.facts {
		if x.Operator() != t.IDXBinaryNotEq {
			continue
		}
		xLHS := x.LHS().AsExpr()
		xRHS := x.RHS().AsExpr()
		if (xLHS.Eq(exprNullptr) && xRHS.Eq(recv)) ||
			(xRHS.Eq(exprNullptr) && xLHS.Eq(recv)) {
			return nil
		}
	}
	return fmt.Errorf("check: cannot prove %q", recv.Str(q.tm)+" <> nullptr")
}

func (q *checker) proveSliceLengthAtLeast(n *a.Expr, minInclLength *big.Int) error {
	if (n.Operator() == t.IDDotDot) && (n.MHS() == nil) && (n.RHS() == nil) {
		if lTyp := n.LHS().AsExpr().MType(); lTyp.IsEitherArrayType() &&
			lTyp.ArrayLength().ConstValue().Cmp(minInclLength) >= 0 {
			return nil
		}
	}
	// TODO: the general case, where n isn't "foo[..]" and foo isn't an array
	// of length >= minInclLength.
	return fmt.Errorf("check: cannot prove %q", n.Str(q.tm)+".length() >= "+minInclLength.String())
}
