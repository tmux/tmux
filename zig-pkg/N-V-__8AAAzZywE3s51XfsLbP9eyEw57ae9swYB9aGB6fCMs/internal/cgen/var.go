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
	"errors"
	"fmt"

	a "github.com/google/wuffs/lang/ast"
	t "github.com/google/wuffs/lang/token"
)

var errNeedDerivedVar = errors.New("cgen: internal error: need derived var")

// argsContainsArgsDotFoo returns whether the argNodes, a slice of *ast.Node
// where each ast.Node is an ast.Arg, contains a "name: value" element whose
// value is "args.foo".
func argsContainsArgsDotFoo(argNodes []*a.Node, foo t.ID) bool {
	for _, o := range argNodes {
		if foo == o.AsArg().Value().IsArgsDotFoo() {
			return true
		}
	}
	return false
}

func (g *gen) needDerivedVar(name t.ID) bool {
	if name == 0 {
		return false
	}
	for _, o := range g.currFunk.astFunc.Body() {
		err := o.Walk(func(p *a.Node) error {
			switch p.Kind() {
			case a.KExpr:
				// Look for p matching "args.name.etc(etc)".
				recv, meth, args, ok := p.AsExpr().IsMethodCall()
				if !ok {
					return nil
				}
				if recv.IsArgsDotFoo() == name {
					if meth == t.IDIsClosed {
						return nil
					}
					return errNeedDerivedVar
				}
				// Some built-in methods will also need a derived var for their
				// arguments.
				//
				// TODO: use a comprehensive list of such methods.
				switch meth {
				case t.IDLimitedSwizzleU32InterleavedFromReader,
					t.IDSwizzleInterleavedFromReader:
					if recv.MType().Eq(typeExprPixelSwizzler) && argsContainsArgsDotFoo(args, name) {
						return errNeedDerivedVar
					}
				}

			case a.KIOManip:
				if p.AsIOManip().IO().IsArgsDotFoo() == name {
					return errNeedDerivedVar
				}
			}
			return nil
		})
		if err == errNeedDerivedVar {
			return true
		}
	}
	return false
}

func (g *gen) findDerivedVars() {
	for _, o := range g.currFunk.astFunc.In().Fields() {
		o := o.AsField()
		if !o.XType().IsIOTokenType() || !g.needDerivedVar(o.Name()) {
			continue
		}
		if g.currFunk.derivedVars == nil {
			g.currFunk.derivedVars = map[t.ID]struct{}{}
		}
		g.currFunk.derivedVars[o.Name()] = struct{}{}
	}
}

func (g *gen) derivedVarCNames(typ *a.TypeExpr) (elem string, i1 string, i2 string, isWriter bool, retErr error) {
	if typ.Decorator() == 0 {
		if qid := typ.QID(); qid[0] == t.IDBase {
			switch qid[1] {
			case t.IDIOReader:
				return "uint8_t", "meta.ri", "meta.wi", false, nil
			case t.IDIOWriter:
				return "uint8_t", "meta.wi", "data.len", true, nil
			case t.IDTokenReader:
				return "wuffs_base__token", "meta.ri", "meta.wi", false, nil
			case t.IDTokenWriter:
				return "wuffs_base__token", "meta.wi", "data.len", true, nil
			}
		}
	}
	return "", "", "", false, fmt.Errorf("unsupported derivedVarCNames type %q", typ.Str(g.tm))
}

func (g *gen) writeInitialLoadDerivedVar(b *buffer, n *a.Field) error {
	elem, i1, i2, isWriter, err := g.derivedVarCNames(n.XType())
	if err != nil {
		return err
	}
	preName := aPrefix + n.Name().Str(g.tm)
	c := ""
	if !isWriter {
		c = "const "
	}

	b.printf("%s%s* %s%s = NULL;\n", c, elem, iopPrefix, preName)
	b.printf("%s%s* %s%s WUFFS_BASE__POTENTIALLY_UNUSED = NULL;\n", c, elem, io0Prefix, preName)
	b.printf("%s%s* %s%s WUFFS_BASE__POTENTIALLY_UNUSED = NULL;\n", c, elem, io1Prefix, preName)
	b.printf("%s%s* %s%s WUFFS_BASE__POTENTIALLY_UNUSED = NULL;\n", c, elem, io2Prefix, preName)

	b.printf("if (%s && %s->data.ptr) {\n", preName, preName)

	b.printf("%s%s = %s->data.ptr;\n", io0Prefix, preName, preName)
	b.printf("%s%s = %s%s + %s->%s;\n", io1Prefix, preName, io0Prefix, preName, preName, i1)
	b.printf("%s%s = %s%s;\n", iopPrefix, preName, io1Prefix, preName)
	b.printf("%s%s = %s%s + %s->%s;\n", io2Prefix, preName, io0Prefix, preName, preName, i2)

	if isWriter {
		b.printf("if (%s->meta.closed) {\n", preName)
		b.printf("%s%s = %s%s;\n", io2Prefix, preName, iopPrefix, preName)
		b.printf("}\n")
	}

	b.printf("}\n")
	return nil
}

func (g *gen) writeFinalSaveDerivedVar(b *buffer, n *a.Field) error {
	_, i1, _, _, err := g.derivedVarCNames(n.XType())
	if err != nil {
		return err
	}
	preName := aPrefix + n.Name().Str(g.tm)

	b.printf("if (%s && %s->data.ptr) {\n%s->%s = ((size_t)(%s%s - %s->data.ptr));\n}\n",
		preName, preName, preName, i1, iopPrefix, preName, preName)
	return nil
}

func (g *gen) writeLoadDerivedVar(b *buffer, n *a.Expr) error {
	_, i1, _, _, err := g.derivedVarCNames(n.MType())
	if err != nil {
		return err
	}

	switch n.Operator() {
	case 0:
		name := n.Ident().Str(g.tm)
		b.printf("%s%s%s = %s%s.data.ptr + %s%s.%s;\n",
			iopPrefix, vPrefix, name, uPrefix, name, uPrefix, name, i1)
		return nil

	case t.IDDot:
		if lhs := n.LHS().AsExpr(); (lhs.Operator() == 0) && (lhs.Ident() == t.IDArgs) {
			name := n.Ident().Str(g.tm)
			b.printf("if (%s%s) {\n", aPrefix, name)
			b.printf("%s%s%s = %s%s->data.ptr + %s%s->%s;\n",
				iopPrefix, aPrefix, name, aPrefix, name, aPrefix, name, i1)
			b.printf("}\n")
			return nil
		}
	}

	return fmt.Errorf("could not determine derived variables for %q", n.Str(g.tm))
}

func (g *gen) writeSaveDerivedVar(b *buffer, n *a.Expr) error {
	_, i1, _, _, err := g.derivedVarCNames(n.MType())
	if err != nil {
		return err
	}

	switch n.Operator() {
	case 0:
		name := n.Ident().Str(g.tm)
		b.printf("%s%s.%s = ((size_t)(%s%s%s - %s%s.data.ptr));\n",
			uPrefix, name, i1, iopPrefix, vPrefix, name, uPrefix, name)
		return nil

	case t.IDDot:
		if lhs := n.LHS().AsExpr(); (lhs.Operator() == 0) && (lhs.Ident() == t.IDArgs) {
			name := n.Ident().Str(g.tm)
			b.printf("if (%s%s) {\n", aPrefix, name)
			b.printf("%s%s->%s = ((size_t)(%s%s%s - %s%s->data.ptr));\n",
				aPrefix, name, i1, iopPrefix, aPrefix, name, aPrefix, name)
			b.printf("}\n")
			return nil
		}
	}

	return fmt.Errorf("could not determine derived variables for %q", n.Str(g.tm))
}

func (g *gen) couldHaveDerivedVar(n *a.Expr) bool {
	typ := n.MType()
	if typ.Decorator() != 0 {
		return false
	}

	qid := typ.QID()
	if qid[0] != t.IDBase {
		return false
	}
	switch qid[1] {
	case t.IDIOReader, t.IDIOWriter, t.IDTokenReader, t.IDTokenWriter:
		// No-op.
	default:
		return false
	}

	switch n.Operator() {
	case t.IDDot:
		if lhs := n.LHS().AsExpr(); (lhs.Operator() == 0) && (lhs.Ident() == t.IDArgs) {
			if g.currFunk.derivedVars == nil {
				return false
			} else if _, ok := g.currFunk.derivedVars[n.Ident()]; !ok {
				return false
			}
		}

	case t.IDOpenParen:
		switch n.LHS().AsExpr().Ident() {
		case t.IDEmptyIOReader, t.IDEmptyIOWriter:
			if n.LHS().AsExpr().LHS().AsExpr().MType().IsEtcUtilityType() {
				return false
			}
		}
	}

	return true
}

func (g *gen) writeLoadExprDerivedVars(b *buffer, n *a.Expr) error {
	if (g.currFunk.derivedVars != nil) && (n.Operator() == a.ExprOperatorCall) {
		for _, o := range n.Args() {
			if v := o.AsArg().Value(); g.couldHaveDerivedVar(v) {
				if err := g.writeLoadDerivedVar(b, v); err != nil {
					return err
				}
			}
		}
	}
	return nil
}

func (g *gen) writeSaveExprDerivedVars(b *buffer, n *a.Expr) error {
	if (g.currFunk.derivedVars != nil) && (n.Operator() == a.ExprOperatorCall) {
		for _, o := range n.Args() {
			if v := o.AsArg().Value(); g.couldHaveDerivedVar(v) {
				if err := g.writeSaveDerivedVar(b, v); err != nil {
					return err
				}
			}
		}
	}
	return nil
}

func (g *gen) writeResumeSuspend1(b *buffer, f *funk, n *a.Var, suspend bool) error {
	if typ := n.XType(); typ.HasPointers() {
		return nil
	} else if f.varResumables == nil || !f.varResumables[n.Name()] {
		return nil
	} else {
		local := fmt.Sprintf("%s%s", vPrefix, n.Name().Str(g.tm))
		lhs := local
		rhs := fmt.Sprintf("self->private_data.%s%s.%s", sPrefix, g.currFunk.astFunc.FuncName().Str(g.tm), lhs)
		if suspend {
			lhs, rhs = rhs, lhs
		}

		switch typ.Decorator() {
		case 0:
			b.printf("%s = %s;\n", lhs, rhs)
			return nil
		case t.IDArray:
			inner := typ.Inner()
			if inner.Decorator() != 0 {
				break
			}
			qid := inner.QID()
			if qid[0] != t.IDBase {
				break
			}
			switch qid[1] {
			case t.IDU8, t.IDU16, t.IDU32, t.IDU64:
				b.printf("memcpy(%s, %s, sizeof(%s));\n", lhs, rhs, local)
				return nil
			}
		}
	}

	return fmt.Errorf("cannot resume or suspend a local variable %q of type %q",
		n.Name().Str(g.tm), n.XType().Str(g.tm))
}

func (g *gen) writeResumeSuspend(b *buffer, f *funk, suspend bool) error {
	for _, n := range f.varList {
		if err := g.writeResumeSuspend1(b, f, n, suspend); err != nil {
			return err
		}
	}
	return nil
}

func (g *gen) writeVars(b *buffer, f *funk, inStructDecl bool) error {
	for _, n := range f.varList {
		typ := n.XType()
		if typ.Innermost().IsEtcUtilityType() {
			continue
		} else if inStructDecl &&
			(typ.HasPointers() || f.varResumables == nil || !f.varResumables[n.Name()]) {
			continue
		}

		name := n.Name().Str(g.tm)

		if typ.IsIOType() {
			b.printf("wuffs_base__io_buffer %s%s = wuffs_base__empty_io_buffer();\n", uPrefix, name)
		} else if typ.IsTokenType() {
			return fmt.Errorf("TODO: support token_{reader,writer} typed variables")
		}

		if typ.IsPointerType() && typ.IsRecursivelyReadOnly() {
			b.writes("const ")
		}

		if err := g.writeCTypeName(b, typ, vPrefix, name); err != nil {
			return err
		}
		if inStructDecl {
			b.writes(";\n")
		} else if typ.IsNumType() {
			b.writes(" = 0;\n")
		} else if typ.IsBool() {
			b.writes(" = false;\n")
		} else if typ.IsStatus() {
			b.writes(" = wuffs_base__make_status(NULL);\n")
		} else if typ.IsIOType() {
			b.printf(" = &%s%s;\n", uPrefix, name)
		} else if typ.Eq(typeExprARMCRC32U32) {
			b.writes(" = 0;\n")
		} else {
			switch typ.Decorator() {
			case t.IDNptr:
				b.writes(" = NULL;\n")
			case t.IDPtr:
				return fmt.Errorf("cannot zero-initialize a non-nullable ptr")
			default:
				b.writes(" = {0};\n")
			}
		}

		if typ.IsIOType() {
			qualifier := ""
			if typ.QID()[1] == t.IDIOReader {
				qualifier = "const "
			}
			b.printf("%suint8_t* %s%s%s WUFFS_BASE__POTENTIALLY_UNUSED = NULL;\n",
				qualifier, iopPrefix, vPrefix, name)
			b.printf("%suint8_t* %s%s%s WUFFS_BASE__POTENTIALLY_UNUSED = NULL;\n",
				qualifier, io0Prefix, vPrefix, name)
			b.printf("%suint8_t* %s%s%s WUFFS_BASE__POTENTIALLY_UNUSED = NULL;\n",
				qualifier, io1Prefix, vPrefix, name)
			b.printf("%suint8_t* %s%s%s WUFFS_BASE__POTENTIALLY_UNUSED = NULL;\n",
				qualifier, io2Prefix, vPrefix, name)
		}
	}
	return nil
}
