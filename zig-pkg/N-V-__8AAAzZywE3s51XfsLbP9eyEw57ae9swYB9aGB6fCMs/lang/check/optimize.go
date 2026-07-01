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

// TODO: should bounds checking even be responsible for selecting between
// optimized implementations of e.g. read_u8? Instead, the Wuffs code could
// explicitly call either read_u8 or read_u8_fast, with the latter having
// stronger preconditions.
//
// Doing so might need some syntactical distinction (not just a question mark)
// between "foo?" and "bar?" if one of those methods can still return an error
// code but never actually suspend.

import (
	"math/big"

	a "github.com/google/wuffs/lang/ast"
	t "github.com/google/wuffs/lang/token"
)

// splitReceiverMethodArgs returns the "receiver", "method" and "args" in the
// expression "receiver.method(args)".
func splitReceiverMethodArgs(n *a.Expr) (receiver *a.Expr, method t.ID, args []*a.Node) {
	if n.Operator() != a.ExprOperatorCall {
		return nil, 0, nil
	}
	args = n.Args()
	n = n.LHS().AsExpr()
	if n.Operator() != a.ExprOperatorSelector {
		return nil, 0, nil
	}
	return n.LHS().AsExpr(), n.Ident(), args
}

func (q *checker) optimizeIOMethodAdvance(receiver *a.Expr, advance *big.Int, advanceExpr *a.Expr, update bool) (retOK bool, retErr error) {
	// TODO: do two passes? The first one to non-destructively check retOK. The
	// second one to drop facts if indeed retOK? Otherwise, an advance
	// precondition failure will lose some of the facts in its error message.

	if advanceExpr != nil {
		return q.optimizeIOMethodAdvanceExpr(receiver, advanceExpr, update)
	}

	// Check if the receiver looks like "a[i .. j]".
	if _, i, j, ok := receiver.IsSlice(); ok {
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

		switch {
		case (icv != nil) && (jcv != nil):
			// OK if i and j are constants and ((j - i) >= advance).
			n := big.NewInt(0).Sub(jcv, icv)
			if n.Cmp(advance) >= 0 {
				retOK = true
			}

		case (i != nil) && (j != nil) && (j.Operator() == t.IDXBinaryPlus):
			// OK if j is (i + constant) and (constant >= advance).
			n := (*big.Int)(nil)
			if j.LHS().AsExpr().Eq(i) {
				n = j.RHS().AsExpr().ConstValue()
			} else if j.RHS().AsExpr().Eq(i) {
				n = j.LHS().AsExpr().ConstValue()
			}
			if n != nil && (n.Cmp(advance) >= 0) {
				retOK = true
			}
		}
	}

	retErr = q.facts.update(func(x *a.Expr) (*a.Expr, error) {
		// TODO: update (discard?) any facts that merely mention
		// receiver.length(), even if they aren't an exact match.

		op := x.Operator()
		if (op != t.IDXBinaryGreaterEq) &&
			(op != t.IDXBinaryGreaterThan) &&
			(op != t.IDXBinaryEqEq) {
			return x, nil
		}

		rcv := x.RHS().AsExpr().ConstValue()
		if rcv == nil {
			return x, nil
		}

		// Check that lhs is "receiver.length()".
		lhs := x.LHS().AsExpr()
		if (lhs.Operator() != a.ExprOperatorCall) || (len(lhs.Args()) != 0) {
			return x, nil
		}
		lhs = lhs.LHS().AsExpr()
		if (lhs.Operator() != a.ExprOperatorSelector) || (lhs.Ident() != t.IDLength) {
			return x, nil
		}
		lhs = lhs.LHS().AsExpr()
		if !lhs.Eq(receiver) {
			return x, nil
		}

		// Check if the bytes available is >= the bytes needed. If so, update
		// rcv to be the bytes remaining. If not, discard the fact x.
		if op == t.IDXBinaryGreaterThan {
			op = t.IDXBinaryGreaterEq
			rcv = big.NewInt(0).Add(rcv, one)
		}
		if rcv.Cmp(advance) < 0 {
			if !update {
				return x, nil
			}
			return nil, nil
		}

		retOK = true

		if !update {
			return x, nil
		}

		if rcv.Cmp(advance) == 0 {
			// TODO: delete the (adjusted) fact, as newRCV will be zero, and
			// "foo.length() >= 0" is redundant.
		}

		// Create a new a.Expr to hold the adjusted RHS constant value, newRCV.
		newRCV := big.NewInt(0).Sub(rcv, advance)
		o, err := makeConstValueExpr(q.tm, newRCV)
		if err != nil {
			return nil, err
		}

		return a.NewExpr(x.AsNode().AsRaw().Flags(),
			t.IDXBinaryGreaterEq, 0, x.LHS(), nil, o.AsNode(), nil), nil
	})
	return retOK, retErr
}

func (q *checker) optimizeIOMethodAdvanceExpr(receiver *a.Expr, advanceExpr *a.Expr, update bool) (retOK bool, retErr error) {
	retErr = q.facts.update(func(x *a.Expr) (*a.Expr, error) {
		// TODO: update (discard?) any facts that merely mention
		// receiver.length(), even if they aren't an exact match.

		op := x.Operator()
		if op != t.IDXBinaryGreaterEq && op != t.IDXBinaryGreaterThan {
			return x, nil
		}

		// Check that lhs is "receiver.length()".
		lhs := x.LHS().AsExpr()
		if (lhs.Operator() != a.ExprOperatorCall) || (len(lhs.Args()) != 0) {
			return x, nil
		}
		lhs = lhs.LHS().AsExpr()
		if (lhs.Operator() != a.ExprOperatorSelector) || (lhs.Ident() != t.IDLength) {
			return x, nil
		}
		lhs = lhs.LHS().AsExpr()
		if !lhs.Eq(receiver) {
			return x, nil
		}

		// Check that rhs is "advanceExpr as base.u64".
		rhs := x.RHS().AsExpr()
		if rhs.Operator() != t.IDXBinaryAs ||
			!rhs.LHS().AsExpr().Eq(advanceExpr) ||
			!rhs.RHS().AsTypeExpr().Eq(typeExprU64) {
			return x, nil
		}

		retOK = true

		if !update {
			return x, nil
		}
		return nil, nil
	})
	return retOK, retErr
}
