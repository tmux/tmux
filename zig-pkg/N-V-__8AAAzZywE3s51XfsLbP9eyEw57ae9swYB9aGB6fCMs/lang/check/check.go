// Copyright 2017 The Wuffs Authors.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

//go:generate go run gen.go

package check

import (
	"errors"
	"fmt"
	"math/big"
	"path"
	"sort"

	"github.com/google/wuffs/lang/builtin"
	"github.com/google/wuffs/lang/parse"

	a "github.com/google/wuffs/lang/ast"
	t "github.com/google/wuffs/lang/token"
)

type Error struct {
	Err      error
	Filename string
	Line     uint32

	TMap  *t.Map
	Facts []*a.Expr
}

func (e *Error) Error() string {
	s := fmt.Sprintf("%s at %s:%d", e.Err, e.Filename, e.Line)
	if e.TMap == nil {
		return s
	}
	b := append([]byte(s), ". Facts:\n"...)
	for _, f := range e.Facts {
		b = append(b, '\t')
		b = append(b, f.Str(e.TMap)...)
		b = append(b, '\n')
	}
	return string(b)
}

func Check(tm *t.Map, files []*a.File, resolveUse func(usePath string) ([]byte, error)) (*Checker, error) {
	for _, f := range files {
		if f == nil {
			return nil, errors.New("check: Check given a nil *ast.File")
		}
	}

	if len(files) > 1 {
		m := map[string]bool{}
		for _, f := range files {
			if m[f.Filename()] {
				return nil, fmt.Errorf("check: Check given duplicate filename %q", f.Filename())
			}
			m[f.Filename()] = true
		}
	}

	rMap := reasonMap{}
	for _, r := range reasons {
		if id := tm.ByName(r.s); id != 0 {
			rMap[id] = r.r
		}
	}
	c := &Checker{
		tm:         tm,
		resolveUse: resolveUse,
		reasonMap:  rMap,

		topLevelNames: map[t.ID]a.Kind{
			t.IDBase: a.KUse,
		},

		consts:   map[t.QID]*a.Const{},
		statuses: map[t.QID]*a.Status{},
		structs:  map[t.QID]*a.Struct{},

		funcs:     map[t.QQID]*a.Func{},
		localVars: map[t.QQID]typeMap{},

		builtInRosliceFuncs:   map[t.QQID]*a.Func{},
		builtInRosliceU8Funcs: map[t.QQID]*a.Func{},
		builtInRotableFuncs:   map[t.QQID]*a.Func{},
		builtInSliceFuncs:     map[t.QQID]*a.Func{},
		builtInSliceU8Funcs:   map[t.QQID]*a.Func{},
		builtInTableFuncs:     map[t.QQID]*a.Func{},

		builtInInterfaces:     map[t.QID][]t.QQID{},
		builtInInterfaceFuncs: map[t.QQID]*a.Func{},
		unseenInterfaceImpls:  map[t.QQID]*a.Func{},

		chooseAlternatives: map[t.QID][]t.ID{},
		noRecursiveMarks:   map[t.QID]uint8{},
	}

	for _, funcs := range builtin.Funcs {
		if err := c.parseBuiltInFuncs(nil, nil, funcs); err != nil {
			return nil, err
		}
	}
	if err := c.parseBuiltInFuncs(c.builtInSliceFuncs, c.builtInRosliceFuncs, builtin.SliceFuncs); err != nil {
		return nil, err
	}
	if err := c.parseBuiltInFuncs(c.builtInSliceU8Funcs, c.builtInRosliceU8Funcs, builtin.SliceU8Funcs); err != nil {
		return nil, err
	}
	if err := c.parseBuiltInFuncs(c.builtInTableFuncs, c.builtInRotableFuncs, builtin.TableFuncs); err != nil {
		return nil, err
	}
	if err := c.parseBuiltInFuncs(c.builtInInterfaceFuncs, nil, builtin.InterfaceFuncs); err != nil {
		return nil, err
	}

	for qqid := range c.builtInInterfaceFuncs {
		qid := t.QID{qqid[0], qqid[1]}
		c.builtInInterfaces[qid] = append(c.builtInInterfaces[qid], qqid)
	}
	for _, qqids := range c.builtInInterfaces {
		sort.Slice(qqids, func(i int, j int) bool {
			return qqids[i].LessThan(qqids[j])
		})
	}

	for _, z := range builtin.Consts {
		name, err := tm.Insert(z.Name)
		if err != nil {
			return nil, err
		}
		xType := (*a.TypeExpr)(nil)
		switch z.Type {
		case t.IDU8:
			xType = typeExprU8
		case t.IDU16:
			xType = typeExprU16
		case t.IDU32:
			xType = typeExprU32
		case t.IDU64:
			xType = typeExprU64
		default:
			return nil, fmt.Errorf("check: unsupported built-in const type %q", z.Type.Str(tm))
		}
		value, err := tm.Insert(z.Value)
		if err != nil {
			return nil, err
		}
		cNode := a.NewConst(0, "", 0, name, xType, a.NewExpr(0, 0, value, nil, nil, nil, nil))
		if err := c.checkConst(cNode.AsNode()); err != nil {
			return nil, err
		}
		c.consts[t.QID{t.IDBase, name}] = cNode
	}

	for _, z := range builtin.Statuses {
		id, err := tm.Insert(z)
		if err != nil {
			return nil, err
		}
		c.statuses[t.QID{t.IDBase, id}] = nil
	}

	for _, phase := range phases {
		if phase.kind == a.KInvalid {
			if err := phase.check(c, nil); err != nil {
				return nil, err
			}
			continue
		}

		for _, f := range files {
			for _, n := range f.TopLevelDecls() {
				if n.Kind() != phase.kind {
					continue
				}
				if err := phase.check(c, n); err != nil {
					return nil, err
				}
			}
			setPlaceholderMBoundsMType(f.AsNode())
		}
	}

	return c, nil
}

var phases = [...]struct {
	kind  a.Kind
	check func(*Checker, *a.Node) error
}{
	{a.KUse, (*Checker).checkUse},
	{a.KStatus, (*Checker).checkStatus},
	{a.KConst, (*Checker).checkConst},
	{a.KStruct, (*Checker).checkStructDecl},
	{a.KInvalid, (*Checker).checkStructCycles},
	{a.KStruct, (*Checker).checkStructFields},
	{a.KFunc, (*Checker).gatherChooseAlternatives},
	{a.KFunc, (*Checker).checkFuncSignature},
	{a.KFunc, (*Checker).checkFuncContract},
	{a.KFunc, (*Checker).checkFuncImplements},
	{a.KFunc, (*Checker).checkFuncBody},
	{a.KFunc, (*Checker).checkNoRecursiveFuncs},
	{a.KInvalid, (*Checker).checkInterfacesSatisfied},
	{a.KStruct, (*Checker).checkFieldMethodCollisions},
	{a.KInvalid, (*Checker).checkAllTypeChecked},
}

type reason func(q *checker, n *a.Assert) error

type reasonMap map[t.ID]reason

type Checker struct {
	tm         *t.Map
	resolveUse func(usePath string) ([]byte, error)
	reasonMap  reasonMap

	// The topLevelNames map is keyed by the const/status/struct/use
	// unqualified name (ID, not QID).
	//
	// For `use "foo/bar"`, the name is the base name: "bar".
	topLevelNames map[t.ID]a.Kind

	// These maps are keyed by the const/status/struct name (QID).
	consts   map[t.QID]*a.Const
	statuses map[t.QID]*a.Status
	structs  map[t.QID]*a.Struct

	// These maps are keyed by the func name (QQID).
	funcs     map[t.QQID]*a.Func
	localVars map[t.QQID]typeMap

	builtInRosliceFuncs   map[t.QQID]*a.Func
	builtInRosliceU8Funcs map[t.QQID]*a.Func
	builtInRotableFuncs   map[t.QQID]*a.Func
	builtInSliceFuncs     map[t.QQID]*a.Func
	builtInSliceU8Funcs   map[t.QQID]*a.Func
	builtInTableFuncs     map[t.QQID]*a.Func

	builtInInterfaces     map[t.QID][]t.QQID
	builtInInterfaceFuncs map[t.QQID]*a.Func
	unseenInterfaceImpls  map[t.QQID]*a.Func

	chooseAlternatives map[t.QID][]t.ID
	noRecursiveMarks   map[t.QID]uint8

	unsortedStructs []*a.Struct
}

func (c *Checker) checkUse(node *a.Node) error {
	usePath := node.AsUse().Path()
	filename, ok := t.Unescape(usePath.Str(c.tm))
	if !ok {
		return fmt.Errorf("check: cannot resolve `use %s`", usePath.Str(c.tm))
	}
	baseName, err := c.tm.Insert(path.Base(filename))
	if err != nil {
		return fmt.Errorf("check: cannot resolve `use %s`: %v", usePath.Str(c.tm), err)
	} else if c.topLevelNames[baseName] != 0 {
		return &Error{
			Err:      fmt.Errorf("check: duplicate top level name %q", baseName.Str(c.tm)),
			Filename: node.AsUse().Filename(),
			Line:     node.AsUse().Line(),
		}
	}
	filename += ".wuffs"

	if c.resolveUse == nil {
		return fmt.Errorf("check: cannot resolve a use declaration")
	}
	src, err := c.resolveUse(filename)
	if err != nil {
		return err
	}
	tokens, _, err := t.Tokenize(c.tm, filename, src)
	if err != nil {
		return err
	}
	f, err := parse.Parse(c.tm, filename, tokens, &parse.Options{
		AllowDoubleUnderscoreNames: true,
	})
	if err != nil {
		return err
	}

	for _, n := range f.TopLevelDecls() {
		if err := n.AsRaw().SetPackage(c.tm, baseName); err != nil {
			return err
		}

		switch n.Kind() {
		case a.KConst:
			if err := c.checkConst(n); err != nil {
				return err
			}
		case a.KFunc:
			if err := c.checkFuncSignature(n); err != nil {
				return err
			}
		case a.KStatus:
			if err := c.checkStatus(n); err != nil {
				return err
			}
		case a.KStruct:
			if err := c.checkStructDecl(n); err != nil {
				return err
			}
		}
	}
	c.topLevelNames[baseName] = a.KUse
	setPlaceholderMBoundsMType(node)
	return nil
}

func (c *Checker) checkStatus(node *a.Node) error {
	n := node.AsStatus()
	qid := n.QID()
	if qid[0] == 0 {
		if c.topLevelNames[qid[1]] != 0 {
			return &Error{
				Err:      fmt.Errorf("check: duplicate top level name %q", qid[1].Str(c.tm)),
				Filename: n.Filename(),
				Line:     n.Line(),
			}
		}
		c.topLevelNames[qid[1]] = a.KStatus
	} else if c.statuses[qid] != nil {
		return &Error{
			Err:      fmt.Errorf("check: duplicate top level name %q", qid.Str(c.tm)),
			Filename: n.Filename(),
			Line:     n.Line(),
		}
	}
	c.statuses[qid] = n

	setPlaceholderMBoundsMType(n.AsNode())
	return nil
}

func (c *Checker) checkConst(node *a.Node) error {
	n := node.AsConst()
	qid := n.QID()
	if qid[0] == 0 {
		if c.topLevelNames[qid[1]] != 0 {
			return &Error{
				Err:      fmt.Errorf("check: duplicate top level name %q", qid[1].Str(c.tm)),
				Filename: n.Filename(),
				Line:     n.Line(),
			}
		}
		c.topLevelNames[qid[1]] = a.KConst
	} else if c.consts[qid] != nil {
		return &Error{
			Err:      fmt.Errorf("check: duplicate top level %q", qid.Str(c.tm)),
			Filename: n.Filename(),
			Line:     n.Line(),
		}
	}
	c.consts[qid] = n

	q := &checker{
		c:  c,
		tm: c.tm,
	}
	typ := n.XType()
	if err := q.tcheckTypeExpr(typ, 0); err != nil {
		return fmt.Errorf("%v in const %s", err, qid.Str(c.tm))
	}
	if _, err := q.bcheckTypeExpr(typ); err != nil {
		return fmt.Errorf("%v in const %s", err, qid.Str(c.tm))
	}

	if err := q.tcheckExpr(n.Value(), 0); err != nil {
		return fmt.Errorf("%v in const %s", err, qid.Str(c.tm))
	}
	if _, err := q.bcheckExpr(n.Value(), 0); err != nil {
		return fmt.Errorf("%v in const %s", err, qid.Str(c.tm))
	}

	nLists := 0
	for elemTyp := typ; ; {
		if dec := elemTyp.Decorator(); dec == t.IDRoarray {
			if nLists == a.MaxTypeExprDepth {
				return fmt.Errorf("check: type expression recursion depth too large")
			}
			nLists++
			elemTyp = elemTyp.Inner()
			continue
		} else if dec != 0 {
			return fmt.Errorf("check: invalid const type %q for %s", n.XType().Str(c.tm), qid.Str(c.tm))
		}
		break
	}

	nb := typ.Innermost().AsNode().MBounds()
	if err := c.checkConstElement(typ, n.Value(), nb, nLists); err != nil {
		return fmt.Errorf("check: %v for %s", err, qid.Str(c.tm))
	}
	setPlaceholderMBoundsMType(n.AsNode())
	return nil
}

func (c *Checker) checkConstElement(typ *a.TypeExpr, n *a.Expr, nb bounds, nLists int) error {
	if nLists > 0 {
		nLists--
		if typ.Decorator() != t.IDRoarray {
			return fmt.Errorf("internal error: inconsistent element type %q", typ.Str(c.tm))
		}
		cv := typ.ArrayLength().ConstValue()
		if args, ok := n.IsList(); !ok {
			return fmt.Errorf("invalid const value %q", n.Str(c.tm))
		} else if cv.Cmp(big.NewInt(int64(len(args)))) != 0 {
			return fmt.Errorf("inconsistent array length (%d) and element count (%d)", cv, len(args))
		} else {
			for _, o := range args {
				if err := c.checkConstElement(typ.Inner(), o.AsExpr(), nb, nLists); err != nil {
					return err
				}
			}
		}
		return nil
	}
	if cv := n.ConstValue(); cv == nil || cv.Cmp(nb[0]) < 0 || cv.Cmp(nb[1]) > 0 {
		return fmt.Errorf("check: invalid const value %q not within %v", n.Str(c.tm), nb)
	}
	return nil
}

func (c *Checker) checkStructDecl(node *a.Node) error {
	n := node.AsStruct()
	qid := n.QID()
	if qid[0] == 0 {
		if c.topLevelNames[qid[1]] != 0 {
			return &Error{
				Err:      fmt.Errorf("check: duplicate top level name %q", qid[1].Str(c.tm)),
				Filename: n.Filename(),
				Line:     n.Line(),
			}
		}
		c.topLevelNames[qid[1]] = a.KStruct
	} else if c.structs[qid] != nil {
		return &Error{
			Err:      fmt.Errorf("check: duplicate top level name %q", qid.Str(c.tm)),
			Filename: n.Filename(),
			Line:     n.Line(),
		}
	}
	c.structs[qid] = n
	c.unsortedStructs = append(c.unsortedStructs, n)
	setPlaceholderMBoundsMType(n.AsNode())

	// Add entries to c.unseenInterfaceImpls that later stages remove, checking
	// that the concrete type (in this package) actually implements the
	// interfaces that it claims to.
	for _, o := range n.Implements() {
		// For example, qid and ifaceType could be "<>.hasher" (i.e. defined in
		// this package, not the base package) and "base.hasher_u32".
		//
		// The "<>" denotes an empty element of a t.QID or t.QQID.
		o := o.AsTypeExpr()
		ifaceType := o.QID()

		if (o.Decorator() != 0) || (ifaceType[0] != t.IDBase) ||
			!builtin.InterfacesMap[ifaceType[1].Str(c.tm)] {
			return fmt.Errorf("check: invalid interface type %q", o.Str(c.tm))
		}
		o.AsNode().SetMBounds(bounds{zero, zero})
		o.AsNode().SetMType(typeExprTypeExpr)

		if qid[0] != 0 {
			continue
		}
		for _, ifaceFunc := range c.builtInInterfaces[ifaceType] {
			// Continuing the example, ifaceFunc could be
			// "base.hasher_u32.update_u32".
			c.unseenInterfaceImpls[t.QQID{qid[0], qid[1], ifaceFunc[2]}] =
				c.builtInInterfaceFuncs[ifaceFunc]
		}
	}

	// A struct declaration implies a reset method.
	in := a.NewStruct(0, n.Filename(), n.Line(), t.IDArgs, nil, nil)
	f := a.NewFunc(a.EffectImpure.AsFlags(), n.Filename(), n.Line(), qid[1], t.IDReset, in, nil, nil, nil)
	if qid[0] != 0 {
		f.AsNode().AsRaw().SetPackage(c.tm, qid[0])
	}
	return c.checkFuncSignature(f.AsNode())
}

func (c *Checker) checkStructCycles(_ *a.Node) error {
	if _, ok := a.TopologicalSortStructs(c.unsortedStructs); !ok {
		return fmt.Errorf("check: cyclical struct definitions")
	}
	return nil
}

func (c *Checker) checkStructFields(node *a.Node) error {
	n := node.AsStruct()
	if err := c.checkFields(n.Fields(), true, false, true, true); err != nil {
		return &Error{
			Err:      fmt.Errorf("%v in struct %s", err, n.QID().Str(c.tm)),
			Filename: n.Filename(),
			Line:     n.Line(),
		}
	}
	return nil
}

func (c *Checker) checkFields(fields []*a.Node, banCPUArchTypes bool, banNonBaseTypes bool, banPtrTypes bool, checkDefaultZeroValue bool) error {
	if len(fields) == 0 {
		return nil
	}

	q := &checker{
		c:  c,
		tm: c.tm,
	}
	fieldNames := map[t.ID]bool{}
	for _, n := range fields {
		f := n.AsField()
		if _, ok := fieldNames[f.Name()]; ok {
			return fmt.Errorf("check: duplicate field %q", f.Name().Str(c.tm))
		}
		if err := checkTypeExpr(q, f.XType()); err != nil {
			return fmt.Errorf("%v for field %q", err, f.Name().Str(c.tm))
		}

		innermost := f.XType().Innermost()
		if banCPUArchTypes && innermost.IsCPUArchType() {
			return fmt.Errorf("check: cpu_arch type %q not allowed for field %q",
				f.XType().Str(c.tm), f.Name().Str(c.tm))
		}
		if banNonBaseTypes && (innermost.QID()[0] != t.IDBase) {
			return fmt.Errorf("check: non-base type %q not allowed for field %q",
				f.XType().Str(c.tm), f.Name().Str(c.tm))
		}
		if banPtrTypes && f.XType().HasPointers() {
			return fmt.Errorf("check: pointer-containing type %q not allowed for field %q",
				f.XType().Str(c.tm), f.Name().Str(c.tm))
		}

		if checkDefaultZeroValue {
			fb := f.XType().Innermost().AsNode().MBounds()
			if (zero.Cmp(fb[0]) < 0) || (zero.Cmp(fb[1]) > 0) {
				return fmt.Errorf("check: default zero value is not within bounds %v for field %q",
					fb, f.Name().Str(c.tm))
			}
		}

		fieldNames[f.Name()] = true
		setPlaceholderMBoundsMType(f.AsNode())
	}

	return nil
}

func checkTypeExpr(q *checker, n *a.TypeExpr) error {
	if err := q.tcheckTypeExpr(n, 0); err != nil {
		return err
	}
	if _, err := q.bcheckTypeExpr(n); err != nil {
		return err
	}
	return nil
}

func (c *Checker) gatherChooseAlternatives(node *a.Node) error {
	n := node.AsFunc()
	return node.Walk(func(innerNode *a.Node) error {
		if innerNode.Kind() == a.KChoose {
			o := innerNode.AsChoose()
			receiverType := n.Receiver()[1]
			oQID := t.QID{receiverType, o.Name()}
			chooseAlternatives := c.chooseAlternatives[oQID]
			for _, p := range o.Args() {
				p := p.AsExpr()
				pQID := t.QID{receiverType, p.Ident()}
				if oQID[1] != pQID[1] {
					chooseAlternatives = append(chooseAlternatives, pQID[1])
				}
			}
			c.chooseAlternatives[oQID] = chooseAlternatives
		}
		return nil
	})
}

func (c *Checker) checkFuncSignature(node *a.Node) error {
	return c.checkFuncSignature1(node, true)
}

func (c *Checker) checkFuncSignature1(node *a.Node, banCPUArchTypes bool) error {
	n := node.AsFunc()
	if err := c.checkFields(n.In().Fields(), banCPUArchTypes, true, false, false); err != nil {
		return &Error{
			Err:      fmt.Errorf("%v in in-params for func %s", err, n.QQID().Str(c.tm)),
			Filename: n.Filename(),
			Line:     n.Line(),
		}
	}
	if banCPUArchTypes && (n.Out() != nil) && n.Out().Innermost().IsCPUArchType() {
		return &Error{
			Err:      fmt.Errorf("check: cpu_arch type %q not allowed as return type", n.Out().Str(c.tm)),
			Filename: n.Filename(),
			Line:     n.Line(),
		}
	}
	setPlaceholderMBoundsMType(n.In().AsNode())
	if out := n.Out(); out != nil {
		if n.Effect().Coroutine() && n.Receiver()[0] != t.IDBase {
			return &Error{
				Err:      fmt.Errorf("func %s has ? effect but non-empty return type", n.QQID().Str(c.tm)),
				Filename: n.Filename(),
				Line:     n.Line(),
			}
		}
		// TODO: does checking a TypeExpr need a q?
		q := &checker{
			c:  c,
			tm: c.tm,
		}
		if err := checkTypeExpr(q, out); err != nil {
			return &Error{
				Err:      fmt.Errorf("%v in out-param for func %s", err, n.QQID().Str(c.tm)),
				Filename: n.Filename(),
				Line:     n.Line(),
			}
		}
	}
	setPlaceholderMBoundsMType(n.AsNode())

	// TODO: check somewhere that, if n.Out() is non-nil (or we are
	// suspendible), that we end with a return statement? Or is that an
	// implicit "return out"?

	qqid := n.QQID()
	if c.funcs[qqid] != nil {
		return &Error{
			Err:      fmt.Errorf("check: duplicate top level name %q", qqid.Str(c.tm)),
			Filename: n.Filename(),
			Line:     n.Line(),
		}
	}
	c.funcs[qqid] = n

	if qqid[0] != 0 {
		// No need to populate c.localVars for built-in or used-package funcs.
		// In any case, the remaining type checking code in this function
		// doesn't handle the base.† dagger type.
		return nil
	}

	iQID := n.In().QID()
	inTyp := a.NewTypeExpr(0, iQID[0], iQID[1], nil, nil, nil)
	inTyp.AsNode().SetMBounds(bounds{zero, zero})
	inTyp.AsNode().SetMType(typeExprTypeExpr)

	localVars := typeMap{
		t.IDArgs:             inTyp,
		t.IDCoroutineResumed: typeExprBool,
	}
	if qqid[1] != 0 {
		if _, ok := c.structs[t.QID{qqid[0], qqid[1]}]; !ok {
			return &Error{
				Err:      fmt.Errorf("check: no receiver struct defined for function %s", qqid.Str(c.tm)),
				Filename: n.Filename(),
				Line:     n.Line(),
			}
		}

		sTyp := a.NewTypeExpr(0, qqid[0], qqid[1], nil, nil, nil)
		sTyp.AsNode().SetMBounds(bounds{zero, zero})
		sTyp.AsNode().SetMType(typeExprTypeExpr)

		pTyp := a.NewTypeExpr(t.IDPtr, 0, 0, nil, nil, sTyp)
		pTyp.AsNode().SetMBounds(bounds{one, maxPointerBounds})
		pTyp.AsNode().SetMType(typeExprTypeExpr)

		localVars[t.IDThis] = pTyp
	}
	c.localVars[qqid] = localVars
	return nil
}

func (c *Checker) checkFuncContract(node *a.Node) error {
	n := node.AsFunc()
	if len(n.Asserts()) == 0 {
		return nil
	}
	q := &checker{
		c:  c,
		tm: c.tm,
	}
	for _, o := range n.Asserts() {
		setPlaceholderMBoundsMType(o)
		if err := q.tcheckFuncAssert(o.AsAssert()); err != nil {
			return err
		}
		if err := q.bcheckFuncAssert(o.AsAssert()); err != nil {
			return err
		}
	}
	return nil
}

func (c *Checker) checkFuncImplements(node *a.Node) error {
	n := node.AsFunc()
	o := c.unseenInterfaceImpls[n.QQID()]
	if o == nil {
		return nil
	}

	if !n.Public() || (n.Effect() != o.Effect()) || !n.Out().Eq(o.Out()) {
		return nil
	}

	// Check that the args (the implicit In types) match.
	nArgs := n.In().Fields()
	oArgs := o.In().Fields()
	if len(nArgs) != len(oArgs) {
		return nil
	}
	for i := range nArgs {
		na := nArgs[i].AsField()
		oa := oArgs[i].AsField()
		if na.Name() != oa.Name() || !na.XType().Eq(oa.XType()) {
			return nil
		}
	}

	// TODO: check that n.Asserts() matches o.Asserts().

	delete(c.unseenInterfaceImpls, n.QQID())
	return nil
}

func (c *Checker) checkFuncBody(node *a.Node) error {
	n := node.AsFunc()
	if len(n.Body()) == 0 {
		return nil
	}

	q := &checker{
		c:         c,
		tm:        c.tm,
		reasonMap: c.reasonMap,
		astFunc:   c.funcs[n.QQID()],
		localVars: c.localVars[n.QQID()],
	}

	// Fill in the TypeMap with all local variables.
	if err := q.tcheckVars(calcCPUArchBits(q.astFunc), n.Body()); err != nil {
		return &Error{
			Err:      err,
			Filename: q.errFilename,
			Line:     q.errLine,
		}
	}

	// TODO: check that variables are never used before they're initialized.

	for _, o := range n.Body() {
		if err := q.tcheckStatement(o); err != nil {
			return &Error{
				Err:      err,
				Filename: q.errFilename,
				Line:     q.errLine,
			}
		}
	}

	if err := q.bcheckBlock(n.Body()); err != nil {
		return &Error{
			Err:      err,
			Filename: q.errFilename,
			Line:     q.errLine,
			TMap:     c.tm,
			Facts:    q.facts,
		}
	}

	return nil
}

// checkNoRecursiveFuncs essentially performs a topological sort (via a
// depth-first search), ordering bar and baz functions on whether bar's body
// (or asserts) calls the this.baz method.
//
// https://en.wikipedia.org/wiki/Topological_sorting#Depth-first_search
func (c *Checker) checkNoRecursiveFuncs(node *a.Node) error {
	_, err := c.checkNoRecursiveFuncs1(nil, node.AsFunc())
	return err
}

func (c *Checker) checkNoRecursiveFuncs1(dst []t.QID, n *a.Func) ([]t.QID, error) {
	nQID := n.QQID().QIDSuffix()
	dst = append(dst, nQID)

	const (
		unmarked  = 0
		temporary = 1
		permanent = 2
	)
	switch c.noRecursiveMarks[nQID] {
	case temporary:
		hidden, names := true, make([]string, 0, len(dst))
		for _, qid := range dst {
			if hidden {
				if qid != nQID {
					continue
				}
				hidden = false
			}
			names = append(names, qid.Str(c.tm))
		}
		return nil, fmt.Errorf("check: recursive call chain: %s", names)
	case permanent:
		return dst, nil
	}
	c.noRecursiveMarks[nQID] = temporary

	if err := n.AsNode().Walk(func(innerNode *a.Node) error {
		if innerNode.Kind() != a.KExpr {
			return nil
		}
		o := innerNode.AsExpr()
		if o.Operator() != t.IDOpenParen {
			return nil
		}
		foo := o.LHS().AsExpr().IsThisDotFoo()
		if foo == 0 {
			return nil
		}
		qqid := n.QQID()
		qqid[2] = foo

		for _, altID := range c.chooseAlternatives[qqid.QIDSuffix()] {
			altQQID := t.QQID{qqid[0], qqid[1], altID}
			dst1, err1 := c.checkNoRecursiveFuncs1(dst, c.funcs[altQQID])
			if err1 != nil {
				return err1
			}
			dst = dst1
		}

		dst1, err1 := c.checkNoRecursiveFuncs1(dst, c.funcs[qqid])
		if err1 != nil {
			return err1
		}
		dst = dst1
		return nil
	}); err != nil {
		return nil, err
	}

	c.noRecursiveMarks[nQID] = permanent
	return dst[:len(dst)-1], nil
}

func (c *Checker) checkInterfacesSatisfied(node *a.Node) error {
	if len(c.unseenInterfaceImpls) == 0 {
		return nil
	}

	// Pick the largest t.QQID key, despite randomized map iteration order, for
	// deterministic error messages. The zero-valued t.QQID is LessThan any
	// non-zero value.
	method, iface := t.QQID{}, t.QID{}
	for qqid, f := range c.unseenInterfaceImpls {
		if method.LessThan(qqid) {
			fqqid := f.QQID()
			method, iface = qqid, t.QID{fqqid[0], fqqid[1]}
		}
	}
	// For example, at the end of the loop above, method and iface could be
	// "<>.hasher.update_u32" and "base.hasher_u32".
	//
	// The "<>" denotes an empty element of a t.QID or t.QQID.

	return fmt.Errorf("check: %q does not implement %q: no matching %q method",
		method[1].Str(c.tm), iface.Str(c.tm), method[2].Str(c.tm))
}

func (c *Checker) checkFieldMethodCollisions(node *a.Node) error {
	n := node.AsStruct()
	for _, o := range n.Fields() {
		nQID := n.QID()
		qqid := t.QQID{nQID[0], nQID[1], o.AsField().Name()}
		if _, ok := c.funcs[qqid]; ok {
			return fmt.Errorf("check: struct %q has both a field and method named %q",
				nQID.Str(c.tm), qqid[2].Str(c.tm))
		}
	}
	return nil
}

func (c *Checker) checkAllTypeChecked(node *a.Node) error {
	for _, v := range c.consts {
		if err := allTypeChecked(c.tm, v.AsNode()); err != nil {
			return err
		}
	}
	for _, v := range c.funcs {
		if err := allTypeChecked(c.tm, v.AsNode()); err != nil {
			return err
		}
	}
	for _, v := range c.statuses {
		if v == nil {
			// Built-in statuses have a nil v node.
			continue
		}
		if err := allTypeChecked(c.tm, v.AsNode()); err != nil {
			return err
		}
	}
	for _, v := range c.structs {
		if err := allTypeChecked(c.tm, v.AsNode()); err != nil {
			return err
		}
	}
	return nil
}

func nodeDebugString(tm *t.Map, n *a.Node) string {
	switch n.Kind() {
	case a.KConst:
		return fmt.Sprintf("%s node %q", n.Kind(), n.AsConst().QID().Str(tm))
	case a.KExpr:
		return fmt.Sprintf("%s node %q", n.Kind(), n.AsExpr().Str(tm))
	case a.KFunc:
		return fmt.Sprintf("%s node %q", n.Kind(), n.AsFunc().QQID().Str(tm))
	case a.KTypeExpr:
		return fmt.Sprintf("%s node %q", n.Kind(), n.AsTypeExpr().Str(tm))
	case a.KStatus:
		return fmt.Sprintf("%s node %q", n.Kind(), n.AsStatus().QID().Str(tm))
	case a.KStruct:
		return fmt.Sprintf("%s node %q", n.Kind(), n.AsStruct().QID().Str(tm))
	}
	return fmt.Sprintf("%s node", n.Kind())
}

func allTypeChecked(tm *t.Map, n *a.Node) error {
	return n.Walk(func(o *a.Node) error {
		if b := o.MBounds(); (b[0] == nil) || (b[1] == nil) {
			return fmt.Errorf("check: internal error: unchecked %s (missing bounds)",
				nodeDebugString(tm, o))
		}
		typ := o.MType()
		if typ == nil {
			return fmt.Errorf("check: internal error: unchecked %s (missing type)",
				nodeDebugString(tm, o))
		}

		typOK := false
		switch o.Kind() {
		case a.KExpr:
			typOK = typ != typeExprPlaceholder && typ != typeExprTypeExpr
		case a.KTypeExpr:
			typOK = typ == typeExprTypeExpr
		default:
			typOK = typ == typeExprPlaceholder
		}
		if !typOK {
			return fmt.Errorf("check: internal error: %s has incorrect type, %s",
				nodeDebugString(tm, o), typ.Str(tm))
		}

		if o.Kind() == a.KExpr {
			o := o.AsExpr()
			if typ.IsIdeal() && o.ConstValue() == nil {
				return fmt.Errorf("check: internal error: expression %q has ideal number type "+
					"but no const value", o.Str(tm))
			}
		}
		return nil
	})
}

type checker struct {
	c         *Checker
	tm        *t.Map
	reasonMap reasonMap
	astFunc   *a.Func
	localVars typeMap

	errFilename string
	errLine     uint32

	facts facts
}
