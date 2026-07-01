// Copyright 2018 The Wuffs Authors.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

package cgen

// This file deals with liveness analysis, determining whether local variables
// need to be kept: saved / loaded when suspending / resuming a coroutine. For
// simplicity, this is a boolean property of each local variable. The analysis
// does not determine that a local variable needs saving at some CSPs
// (coroutine suspension points) but not others.
//
// For example, in this code:
//
//   i = etc
//   yield? foo  // The first CSP.
//   j = 0
//   while j < i {
//     a[j] = 42
//     j += 1
//   }
//   c = args.src.read_u8?()  // The second CSP.
//   k = a[c]
//
// There are two CSPs. The i local variable will need to be kept, as it is
// written to before the first CSP and read from after the first CSP.
// Similarly, the a local variable (an array) will need to be kept, as its use
// crosses the second CSP. The j local variable need not be kept, as all of its
// uses happens between two consecutive CSPs. To be precise, for every possible
// code path (including if branches and while loops), each read from j is
// preceded by a write to j without an intervening CSP. The c and k local
// variables also need not be kept, as their uses are all after the last CSP.
//
// Algorithmically, we walk through a function's statements in two passes. The
// first pass finds all of the local variables, and assigns an integer index to
// each one. The second tracks (in a slice indexed by that integer index) each
// local variable's liveness, one of three possible states: none, weak and
// strong. Strong means that the local variable definitely needs to be kept.
// Weak means that we have (in the worst case, for branches' and loops'
// multiple paths) seen a CSP without a write afterwards, so that seeing a read
// would change the liveness to strong. None means that we have not seen a CSP
// since the last write.
//
// Essentially, the state transitions are: strong is sticky, weak to strong
// happens on a read and weak to none happens on a write. On a CSP, none/weak
// to strong happens for variables explicitly mentioned in the CSP expression
// and none to weak happens for other variables.
//
// When reconciling multiple code paths, the strongest wins, where strong >
// weak and weak > none. Loops are repeated until the reconciliations between
// the N'th and N+1'th iteration are all no-ops: a steady state has been
// reached. There can be multiple reconciliations per iteration, due to break
// and continue statements.

import (
	"fmt"

	a "github.com/google/wuffs/lang/ast"
	t "github.com/google/wuffs/lang/token"
)

// liveness tracks whether a local variable's value will need to be kept across
// CSPs (coroutine suspension points).
type liveness uint32

const (
	livenessNone   = liveness(0)
	livenessWeak   = liveness(1)
	livenessStrong = liveness(2)
)

// livenesses is a slice of liveness values, one per local variable.
type livenesses []liveness

func (r livenesses) clear() {
	for i := range r {
		r[i] = livenessNone
	}
}

func (r livenesses) clone() livenesses {
	return append(livenesses(nil), r...)
}

func (r livenesses) reconcile(s livenesses) (changed bool) {
	if len(r) != len(s) {
		panic("livenesses have different length")
	}
	for i := range r {
		if r[i] < s[i] {
			r[i] = s[i]
			changed = true
		}
	}
	return changed
}

func (r livenesses) lowerWeakToNone(i int) {
	if r[i] == livenessWeak {
		r[i] = livenessNone
	}
}

func (r livenesses) raiseToStrong(i int) {
	r[i] = livenessStrong
}

func (r livenesses) raiseWeakToStrong(i int) {
	if r[i] == livenessWeak {
		r[i] = livenessStrong
	}
}

func (r livenesses) raiseNoneToWeak() {
	for i, x := range r {
		if x == livenessNone {
			r[i] = livenessWeak
		}
	}
}

type loopLivenesses struct {
	before  livenesses
	after   livenesses
	changed bool
}

type livenessHelper struct {
	tm    *t.Map
	vars  map[t.ID]int // Maps from local variable name to livenesses index.
	loops map[a.Loop]*loopLivenesses
	final livenesses
}

func (g *gen) findVars() error {
	h := livenessHelper{
		tm:    g.tm,
		vars:  map[t.ID]int{},
		loops: map[a.Loop]*loopLivenesses{},
	}

	f := g.currFunk.astFunc
	for _, n := range f.Body() {
		if n.Kind() != a.KVar {
			break
		}
		g.currFunk.varList = append(g.currFunk.varList, n.AsVar())
		h.vars[n.AsVar().Name()] = len(h.vars)
	}

	if f.Effect().Coroutine() {
		h.final = make(livenesses, len(h.vars))
		r := make(livenesses, len(h.vars))
		if err := h.doBlock(r, f.Body(), 0); err != nil {
			return err
		}
		h.final.reconcile(r)

		g.currFunk.varResumables = map[t.ID]bool{}
		for i, v := range g.currFunk.varList {
			g.currFunk.varResumables[v.Name()] = h.final[i] == livenessStrong
		}
	}

	return nil
}

func (h *livenessHelper) doBlock(r livenesses, block []*a.Node, depth uint32) error {
	if depth > a.MaxBodyDepth {
		return fmt.Errorf("body recursion depth too large")
	}
	depth++

loop:
	for _, o := range block {
		switch o.Kind() {
		case a.KAssign:
			if err := h.doAssign(r, o.AsAssign(), depth); err != nil {
				return err
			}

		case a.KExpr:
			if err := h.doExpr(r, o.AsExpr()); err != nil {
				return err
			}

		case a.KIOManip:
			if err := h.doIOManip(r, o.AsIOManip(), depth); err != nil {
				return err
			}

		case a.KIf:
			if err := h.doIf(r, o.AsIf(), depth); err != nil {
				return err
			}

		case a.KIterate:
			return fmt.Errorf("iterate loop is inside a coroutine")

		case a.KJump:
			if err := h.doJump(r, o.AsJump(), depth); err != nil {
				return err
			}
			break loop

		case a.KRet:
			if err := h.doRet(r, o.AsRet(), depth); err != nil {
				return err
			}
			if o.AsRet().Keyword() == t.IDReturn {
				break loop
			}

		case a.KVar:
			if err := h.doVar(r, o.AsVar(), depth); err != nil {
				return err
			}

		case a.KWhile:
			if err := h.doWhile(r, o.AsWhile(), depth); err != nil {
				return err
			}
		}
	}
	return nil
}

func (h *livenessHelper) doAssign(r livenesses, n *a.Assign, depth uint32) error {
	if n.Operator() == t.IDEqQuestion {
		if err := h.doExpr1(r, n.RHS(), false, 0); err != nil {
			return err
		}
	} else {
		if err := h.doExpr(r, n.RHS()); err != nil {
			return err
		}
	}

	if n.LHS() == nil {
		return nil
	}

	// If the LHS is not a local variable (e.g. "this.foo[bar] = etc", or if
	// the LHS is implicitly also on the RHS (e.g. for a += or *= operator),
	// walk the LHS Expr.
	if n.LHS().Operator() != 0 || (n.Operator() != t.IDEq && n.Operator() != t.IDEqQuestion) {
		if err := h.doExpr(r, n.LHS()); err != nil {
			return err
		}
	}

	// If the LHS is just a local variable, then call lowerWeakToNone.
	if lhs := n.LHS(); lhs.Operator() == 0 {
		if i, ok := h.vars[lhs.Ident()]; !ok {
			return fmt.Errorf("unrecognized variable %q", lhs.Ident().Str(h.tm))
		} else {
			r.lowerWeakToNone(i)
		}
	}
	return nil
}

func (h *livenessHelper) doExpr(r livenesses, n *a.Expr) error {
	allToStrong := false
	if n.Effect().Coroutine() {
		recv := n.LHS().AsExpr().LHS().AsExpr()
		if recv.MType().IsIOTokenType() {
			// No-op. These methods already save their args across suspensions.
		} else {
			allToStrong = true
		}
	}

	if err := h.doExpr1(r, n, allToStrong, 0); err != nil {
		return err
	}
	if n.Effect().Coroutine() {
		r.raiseNoneToWeak()
	}
	return nil
}

func (h *livenessHelper) doExpr1(r livenesses, n *a.Expr, allToStrong bool, depth uint32) error {
	if depth > a.MaxBodyDepth {
		return fmt.Errorf("body recursion depth too large")
	}
	depth++

	for _, o := range n.AsNode().AsRaw().SubNodes() {
		if o != nil && o.Kind() == a.KExpr {
			if err := h.doExpr1(r, o.AsExpr(), allToStrong, depth); err != nil {
				return err
			}
		}
	}
	for _, o := range n.Args() {
		e := (*a.Expr)(nil)
		switch o.Kind() {
		case a.KArg:
			e = o.AsArg().Value()
		case a.KExpr:
			e = o.AsExpr()
		default:
			return fmt.Errorf("unrecognized arg kind")
		}
		if err := h.doExpr1(r, e, allToStrong, depth); err != nil {
			return err
		}
	}

	if n.Operator() == 0 {
		if i, ok := h.vars[n.Ident()]; ok {
			if allToStrong {
				r.raiseToStrong(i)
			} else {
				r.raiseWeakToStrong(i)
			}
		}
	}

	return nil
}

func (h *livenessHelper) doIOManip(r livenesses, n *a.IOManip, depth uint32) error {
	if err := h.doExpr(r, n.IO()); err != nil {
		return err
	}
	if arg1 := n.Arg1(); arg1 != nil {
		if err := h.doExpr(r, arg1); err != nil {
			return err
		}
	}
	if histPos := n.HistoryPosition(); histPos != nil {
		if err := h.doExpr(r, histPos); err != nil {
			return err
		}
	}
	return h.doBlock(r, n.Body(), depth)
}

func (h *livenessHelper) doIf(r livenesses, n *a.If, depth uint32) error {
	scratch := make(livenesses, len(r))
	result := make(livenesses, len(r))
	for {
		if err := h.doExpr(r, n.Condition()); err != nil {
			return err
		}

		copy(scratch, r)
		if err := h.doBlock(scratch, n.BodyIfTrue(), depth); err != nil {
			return err
		}
		result.reconcile(scratch)

		if ei := n.ElseIf(); ei != nil {
			n = ei
			continue
		}

		copy(scratch, r)
		if err := h.doBlock(scratch, n.BodyIfFalse(), depth); err != nil {
			return err
		}
		result.reconcile(scratch)
		break
	}
	copy(r, result)
	return nil
}

func (h *livenessHelper) doJump(r livenesses, n *a.Jump, depth uint32) error {
	l := h.loops[n.JumpTarget()]
	switch n.Keyword() {
	case t.IDBreak:
		l.changed = l.after.reconcile(r) || l.changed
	case t.IDContinue:
		l.changed = l.before.reconcile(r) || l.changed
	default:
		return fmt.Errorf("unrecognized ast.Jump keyword")
	}
	r.clear()
	return nil
}

func (h *livenessHelper) doRet(r livenesses, n *a.Ret, depth uint32) error {
	if err := h.doExpr(r, n.Value()); err != nil {
		return err
	}

	switch n.Keyword() {
	case t.IDReturn:
		h.final.reconcile(r)
		r.clear()
	case t.IDYield:
		r.raiseNoneToWeak()
	default:
		return fmt.Errorf("unrecognized ast.Ret keyword")
	}
	return nil
}

func (h *livenessHelper) doVar(r livenesses, n *a.Var, depth uint32) error {
	name := n.Name()
	if i, ok := h.vars[name]; !ok {
		return fmt.Errorf("unrecognized variable %q", name.Str(h.tm))
	} else {
		r.lowerWeakToNone(i)
	}
	return nil
}

func (h *livenessHelper) doWhile(r livenesses, n *a.While, depth uint32) error {
	l := &loopLivenesses{
		before:  make(livenesses, len(r)),
		after:   make(livenesses, len(r)),
		changed: false,
	}
	h.loops[n] = l

	copy(l.before, r)

	for l.changed = true; l.changed; {
		l.changed = false
		copy(r, l.before)

		if err := h.doExpr(r, n.Condition()); err != nil {
			return err
		} else if !n.IsWhileTrue() {
			l.changed = l.after.reconcile(r) || l.changed
		}

		if err := h.doBlock(r, n.Body(), depth); err != nil {
			return err
		} else {
			l.changed = l.before.reconcile(r) || l.changed
		}
	}

	copy(r, l.after)
	return nil
}
