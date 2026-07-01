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
	"strconv"

	a "github.com/google/wuffs/lang/ast"
	t "github.com/google/wuffs/lang/token"
)

type funk struct {
	bPrologue    buffer
	bBodyResume  buffer
	bBody        buffer
	bBodySuspend buffer
	bEpilogue    buffer

	astFunc       *a.Func
	cName         string
	coroID        uint32
	returnsStatus bool

	varList           []*a.Var
	varResumables     map[t.ID]bool
	derivedVars       map[t.ID]struct{}
	jumpTargets       map[a.Loop]string
	activeLoops       a.LoopStack
	coroSuspPoint     uint32
	ioManips          uint32
	tempW             uint32
	tempR             uint32
	usesEmptyIOBuffer bool
	usesScratch       bool
	hasGotoOK         bool
}

func (k *funk) jumpTarget(tm *t.Map, n a.Loop) (string, error) {
	if label := n.Label(); label != 0 {
		return label.Str(tm), nil
	}
	if k.jumpTargets == nil {
		k.jumpTargets = map[a.Loop]string{}
	}
	if jt, ok := k.jumpTargets[n]; ok {
		return jt, nil
	}
	jtInt := len(k.jumpTargets)
	if jtInt == 1000000 {
		return "", fmt.Errorf("too many jump targets")
	}
	jt := strconv.Itoa(jtInt)
	k.jumpTargets[n] = jt
	return jt, nil
}

func (g *gen) funcCName(n *a.Func) string {
	if r := n.Receiver(); !r.IsZero() {
		// TODO: this isn't right if r[0] != 0, i.e. the receiver is from a
		// used package. There might be similar cases elsewhere in this
		// package.
		return g.pkgPrefix + r[1].Str(g.tm) + "__" + n.FuncName().Str(g.tm)
	}
	return g.pkgPrefix + n.FuncName().Str(g.tm)
}

// writeFunctionSignature modes.
const (
	wfsCDecl               = 0
	wfsCDeclChoosy         = 1
	wfsCppDecl             = 2
	wfsCFuncPtrField       = 3
	wfsCFuncPtrFieldChoosy = 4
	wfsCFuncPtrType        = 5
)

func (g *gen) writeFuncSignature(b *buffer, n *a.Func, wfs uint32) error {
	switch wfs {
	case wfsCDecl:
		b.writes("WUFFS_BASE__GENERATED_C_CODE\n")
		if n.Public() {
			b.writes("WUFFS_BASE__MAYBE_STATIC ")
		} else {
			b.writes("static ")
		}

	case wfsCDeclChoosy:
		b.writes("WUFFS_BASE__GENERATED_C_CODE\n")
		b.writes("static ")

	case wfsCppDecl:
		b.writes("  inline ")

	case wfsCFuncPtrField, wfsCFuncPtrFieldChoosy, wfsCFuncPtrType:
		// No-op.
	}

	// TODO: write n's return values.
	if n.Effect().Coroutine() {
		b.writes("wuffs_base__status")
	} else if out := n.Out(); out == nil {
		b.writes("wuffs_base__empty_struct")
		// TODO: does writeCTypeName generate the right C if out is an array?
	} else if err := g.writeCTypeName(b, out, "", ""); err != nil {
		return err
	}

	switch wfs {
	case wfsCDecl, wfsCDeclChoosy:
		b.writes("\n")
	case wfsCppDecl:
		b.writes("\n  ")
	case wfsCFuncPtrField, wfsCFuncPtrFieldChoosy:
		b.writes(" ")
	}

	comma := false
	switch wfs {
	case wfsCDecl, wfsCDeclChoosy:
		b.writes(g.funcCName(n))
		if wfs == wfsCDeclChoosy {
			b.writes("__choosy_default")
		}
		b.writeb('(')
		if r := n.Receiver(); !r.IsZero() {
			b.writes("\n    ")
			if n.Effect().Pure() {
				b.writes("const ")
			}
			b.printf("%s%s* self", g.pkgPrefix, r[1].Str(g.tm))
			comma = true
		}

	case wfsCppDecl:
		b.writes(n.FuncName().Str(g.tm))
		b.writeb('(')
		if len(n.In().Fields()) > 0 {
			b.writes("\n      ")
		}

	case wfsCFuncPtrField, wfsCFuncPtrFieldChoosy, wfsCFuncPtrType:
		b.writes("(*")
		if wfs == wfsCFuncPtrField {
			b.writes(n.FuncName().Str(g.tm))
			b.writes(")(\n    ")
		} else if wfs == wfsCFuncPtrFieldChoosy {
			b.writes("choosy_")
			b.writes(n.FuncName().Str(g.tm))
			b.writes(")(\n    ")
		} else {
			b.writes(")(")
		}

		if n.Effect().Pure() {
			b.writes("const ")
		}

		if wfs == wfsCFuncPtrField {
			b.writes("void* self")
		} else if wfs == wfsCFuncPtrFieldChoosy {
			b.printf("%s%s* self", g.pkgPrefix, n.Receiver()[1].Str(g.tm))
		} else {
			b.writes("void*")
		}
		comma = true
	}

	for _, o := range n.In().Fields() {
		if comma {
			b.writes(",\n    ")
			if wfs == wfsCppDecl {
				b.writes("  ")
			}
		}
		comma = true
		o := o.AsField()
		varNamePrefix, varName := "", ""
		if wfs != wfsCFuncPtrType {
			varNamePrefix, varName = aPrefix, o.Name().Str(g.tm)
		}
		if err := g.writeCTypeName(b, o.XType(), varNamePrefix, varName); err != nil {
			return err
		}
	}

	b.printf(")")
	if (wfs == wfsCppDecl) && !n.Receiver().IsZero() && n.Effect().Pure() {
		b.writes(" const")
	}
	return nil
}

func (g *gen) writeFuncPrototype(b *buffer, n *a.Func) error {
	caMacro, _, _, err := cpuArchCNames(n.Asserts())
	if err != nil {
		return err
	}
	if caMacro != "" {
		b.printf("#if defined(WUFFS_PRIVATE_IMPL__CPU_ARCH__%s)\n", caMacro)
	}
	if err := g.writeFuncSignature(b, n, wfsCDecl); err != nil {
		return err
	}
	b.writes(";\n")
	if caMacro != "" {
		b.printf("#endif  // defined(WUFFS_PRIVATE_IMPL__CPU_ARCH__%s)\n", caMacro)
	}
	b.writes("\n")
	if n.Choosy() {
		if err := g.writeFuncSignature(b, n, wfsCDeclChoosy); err != nil {
			return err
		}
		b.writes(";\n\n")
	}
	return nil
}

func (g *gen) writeFuncImpl(b *buffer, n *a.Func) error {
	k := g.funks[n.QQID()]

	caMacro, caName, caAttribute, err := cpuArchCNames(n.Asserts())
	if err != nil {
		return err
	}
	if caName != "" {
		b.printf("// ‼ WUFFS MULTI-FILE SECTION +%s\n", caName)
	}
	b.printf("// -------- func %s.%s\n\n", g.pkgName, n.QQID().Str(g.tm))
	if caMacro != "" {
		b.printf("#if defined(WUFFS_PRIVATE_IMPL__CPU_ARCH__%s)\n", caMacro)
	}
	if caAttribute != "" {
		b.printf("%s\n", caAttribute)
	}

	if err := g.writeFuncSignature(b, n, wfsCDecl); err != nil {
		return err
	}
	b.writes(" {\n")

	if n.Choosy() {
		b.printf("return (*self->private_impl.choosy_%s)(self", n.FuncName().Str(g.tm))
		for _, o := range n.In().Fields() {
			b.printf(", %s%s", aPrefix, o.AsField().Name().Str(g.tm))
		}
		b.writes(");\n}\n\n")

		if err := g.writeFuncSignature(b, n, wfsCDeclChoosy); err != nil {
			return err
		}
		b.writes(" {\n")
	}

	if (len(n.Body()) != 0) || n.Effect().Coroutine() || (n.Out() != nil) {
		b.writex(k.bPrologue)
		if n.Effect().Coroutine() {
			b.writex(k.bBodyResume)
		}
		b.writex(k.bBody)
		if n.Effect().Coroutine() {
			b.writex(k.bBodySuspend)
		} else if k.hasGotoOK {
			b.writes("\nok:\n")
		}
	}

	b.writex(k.bEpilogue)
	b.writes("}\n")
	if caMacro != "" {
		b.printf("#endif  // defined(WUFFS_PRIVATE_IMPL__CPU_ARCH__%s)\n", caMacro)
	}
	if caName != "" {
		b.printf("// ‼ WUFFS MULTI-FILE SECTION -%s\n", caName)
	}
	b.writes("\n")
	return nil
}

func (g *gen) gatherFuncImpl(_ *buffer, n *a.Func) error {
	coroID := uint32(0)
	if n.Public() && n.Effect().Coroutine() {
		g.numPublicCoroutines[n.Receiver()]++
		coroID = g.numPublicCoroutines[n.Receiver()]
		if coroID >= 0x8000 {
			return fmt.Errorf("too many coroutines for %q", n.Receiver().Str(g.tm))
		}
	}

	g.currFunk = funk{
		astFunc: n,
		cName:   g.funcCName(n),
		coroID:  coroID,

		returnsStatus: n.Effect().Coroutine() ||
			((n.Out() != nil) && n.Out().IsStatus()),
	}

	if err := g.findVars(); err != nil {
		return err
	}
	g.findDerivedVars()

	if err := g.writeFuncImplBody(&g.currFunk.bBody); err != nil {
		return err
	}

	if err := g.writeFuncImplPrologue(&g.currFunk.bPrologue); err != nil {
		return err
	}
	if err := g.writeFuncImplBodyResume(&g.currFunk.bBodyResume); err != nil {
		return err
	}
	if err := g.writeFuncImplBodySuspend(&g.currFunk.bBodySuspend); err != nil {
		return err
	}
	if err := g.writeFuncImplEpilogue(&g.currFunk.bEpilogue); err != nil {
		return err
	}

	if g.currFunk.tempW != g.currFunk.tempR {
		return fmt.Errorf("internal error: temporary variable count out of sync")
	}
	g.funks[n.QQID()] = g.currFunk
	return nil
}

func writeOutParamZeroValue(b *buffer, tm *t.Map, typ *a.TypeExpr) error {
	if typ == nil {
		b.writes("wuffs_base__make_empty_struct()")
		return nil
	} else if typ.IsNumType() {
		b.writes("0")
		return nil
	} else if typ.IsEitherSliceType() {
		if inner := typ.Inner(); (inner.Decorator() == 0) && (inner.QID() == t.QID{t.IDBase, t.IDU8}) {
			b.writes("wuffs_base__empty_slice_u8()")
			return nil
		}
	} else if (typ.Decorator() == 0) && (typ.QID()[0] == t.IDBase) {
		switch typ.QID()[1] {
		case t.IDBitvec256:
			b.writes("wuffs_base__utility__make_bitvec256(0u, 0u, 0u, 0u)")
			return nil
		case t.IDOptionalU63:
			b.writes("wuffs_base__utility__make_optional_u63(false, 0u)")
			return nil
		case t.IDRangeIEU32:
			b.writes("wuffs_base__utility__empty_range_ie_u32()")
			return nil
		case t.IDRangeIIU32:
			b.writes("wuffs_base__utility__empty_range_ii_u32()")
			return nil
		case t.IDRangeIEU64:
			b.writes("wuffs_base__utility__empty_range_ie_u64()")
			return nil
		case t.IDRangeIIU64:
			b.writes("wuffs_base__utility__empty_range_ii_u64()")
			return nil
		case t.IDRectIEU32:
			b.writes("wuffs_base__utility__empty_rect_ie_u32()")
			return nil
		case t.IDRectIIU32:
			b.writes("wuffs_base__utility__empty_rect_ii_u32()")
			return nil
		}
	}
	return fmt.Errorf("internal error: cannot write the zero value of type %q", typ.Str(tm))
}

func writeFuncImplSelfMagicCheck(b *buffer, tm *t.Map, f *a.Func) error {
	returnsStatus := f.Effect().Coroutine() ||
		((f.Out() != nil) && f.Out().IsStatus())

	b.writes("  if (!self) {\n    return ")
	if returnsStatus {
		b.writes("wuffs_base__make_status(wuffs_base__error__bad_receiver)")
	} else if err := writeOutParamZeroValue(b, tm, f.Out()); err != nil {
		return err
	}
	b.writes(";\n  }\n")

	if f.Effect().Pure() {
		b.writes("  if ((self->private_impl.magic != WUFFS_BASE__MAGIC) &&\n")
		b.writes("      (self->private_impl.magic != WUFFS_BASE__DISABLED)) {\n")
	} else {
		b.writes("  if (self->private_impl.magic != WUFFS_BASE__MAGIC) {\n")
	}
	b.writes("    return ")
	if returnsStatus {
		b.writes("wuffs_base__make_status(\n" +
			"        (self->private_impl.magic == WUFFS_BASE__DISABLED)\n" +
			"            ? wuffs_base__error__disabled_by_previous_error\n" +
			"            : wuffs_base__error__initialize_not_called)")
	} else if err := writeOutParamZeroValue(b, tm, f.Out()); err != nil {
		return err
	}

	b.writes(";\n  }\n")
	return nil
}

func (g *gen) writeFuncImplPrologue(b *buffer) error {
	oldLenB := len(*b)

	// Check the initialized/disabled state and the "self" arg.
	if g.currFunk.astFunc.Public() && !g.currFunk.astFunc.Receiver().IsZero() {
		if err := writeFuncImplSelfMagicCheck(b, g.tm, g.currFunk.astFunc); err != nil {
			return err
		}
	}

	// For public functions, check (at runtime) the other args for bounds and
	// null-ness. For private functions, those checks are done at compile time.
	if g.currFunk.astFunc.Public() {
		if err := g.writeFuncImplArgChecks(b, g.currFunk.astFunc); err != nil {
			return err
		}

		// For public coroutines, check that we are not suspended in an active
		// coroutine.
		if g.currFunk.astFunc.Effect().Coroutine() {
			b.printf("if ((self->private_impl.active_coroutine != 0) &&\n"+
				"(self->private_impl.active_coroutine != %d)) {\n", g.currFunk.coroID)
			b.writes("self->private_impl.magic = WUFFS_BASE__DISABLED;\n")
			b.writes("return wuffs_base__make_status(wuffs_base__error__interleaved_coroutine_calls);\n")
			b.writes("}\n")
			b.writes("self->private_impl.active_coroutine = 0;\n")
		}
	}

	if g.currFunk.astFunc.Effect().Coroutine() ||
		(g.currFunk.returnsStatus && (len(g.currFunk.derivedVars) > 0)) {
		// TODO: rename the "status" variable to "ret"?
		b.printf("wuffs_base__status status = wuffs_base__make_status(NULL);\n")
	}

	if oldLenB != len(*b) {
		b.writes("\n")
	}

	// Generate the local variables.
	oldLenB = len(*b)
	if err := g.writeVars(b, &g.currFunk, false); err != nil {
		return err
	}
	if oldLenB != len(*b) {
		b.writes("\n")
	}

	if g.currFunk.derivedVars != nil {
		for _, o := range g.currFunk.astFunc.In().Fields() {
			o := o.AsField()
			if _, ok := g.currFunk.derivedVars[o.Name()]; ok {
				if err := g.writeInitialLoadDerivedVar(b, o); err != nil {
					return err
				}
			}
		}
		b.writes("\n")
	}
	return nil
}

func (g *gen) writeFuncImplBodyResume(b *buffer) error {
	if g.currFunk.coroSuspPoint > 0 {
		b.printf("uint32_t coro_susp_point = self->private_impl.%s%s;\n",
			pPrefix, g.currFunk.astFunc.FuncName().Str(g.tm))

		resumeBuffer := buffer{}
		if err := g.writeResumeSuspend(&resumeBuffer, &g.currFunk, false); err != nil {
			return err
		}
		if len(resumeBuffer) > 0 {
			b.writes("if (coro_susp_point) {\n")
			b.writex(resumeBuffer)
			b.writes("}\n")
		}

		// Generate a coroutine switch similiar to the technique in
		// https://www.chiark.greenend.org.uk/~sgtatham/coroutines.html
		//
		// The matching } is written below. See "Close the coroutine switch".
		b.writes("switch (coro_susp_point) {\nWUFFS_BASE__COROUTINE_SUSPENSION_POINT_0;\n\n")
	}
	return nil
}

func (g *gen) writeFuncImplBody(b *buffer) error {
	for _, o := range g.currFunk.astFunc.Body() {
		if err := g.writeStatement(b, o, 0); err != nil {
			return err
		}
	}
	return nil
}

func (g *gen) writeFuncImplBodySuspend(b *buffer) error {
	if (g.currFunk.coroSuspPoint > 0) || g.currFunk.astFunc.Effect().Coroutine() {
		if !g.currFunk.hasGotoOK {
			b.writes("\ngoto ok;") // Avoid the "unused label" warning.
		}
		b.writes("\nok:\n")
	}

	if g.currFunk.coroSuspPoint > 0 {
		// We've reached the end of the function body. Reset the coroutine
		// suspension point so that the next call to this function starts at
		// the top.
		b.printf("self->private_impl.%s%s = 0;\n",
			pPrefix, g.currFunk.astFunc.FuncName().Str(g.tm))
		b.writes("goto exit;\n}\n\n") // Close the coroutine switch.

		b.writes("goto suspend;\nsuspend:\n") // The goto avoids the "unused label" warning.

		b.printf("self->private_impl.%s%s = "+
			"wuffs_base__status__is_suspension(&status) ? coro_susp_point : 0;\n",
			pPrefix, g.currFunk.astFunc.FuncName().Str(g.tm))
		if g.currFunk.astFunc.Public() {
			b.printf("self->private_impl.active_coroutine = "+
				"wuffs_base__status__is_suspension(&status) ? %d : 0;\n", g.currFunk.coroID)
		}
		if err := g.writeResumeSuspend(b, &g.currFunk, true); err != nil {
			return err
		}
		b.writes("\n")
	}
	return nil
}

func (g *gen) writeFuncImplEpilogue(b *buffer) error {
	epilogue := ""
	if g.currFunk.astFunc.Effect().Coroutine() ||
		(g.currFunk.returnsStatus && (len(g.currFunk.derivedVars) > 0)) {

		b.writes("goto exit;\nexit:\n") // The goto avoids the "unused label" warning.

		if g.currFunk.astFunc.Public() {
			epilogue = "if (wuffs_base__status__is_error(&status)) {\n" +
				"self->private_impl.magic = WUFFS_BASE__DISABLED;\n}\n" +
				"return status;\n"
		} else {
			epilogue = "return status;\n"
		}
	} else if g.currFunk.astFunc.Out() == nil {
		epilogue = "return wuffs_base__make_empty_struct();\n"
	}

	if (epilogue == "") && g.currFunk.astFunc.BodyEndsWithReturn() {
		// No-op.
	} else if g.currFunk.derivedVars != nil {
		for _, o := range g.currFunk.astFunc.In().Fields() {
			o := o.AsField()
			if _, ok := g.currFunk.derivedVars[o.Name()]; ok {
				if err := g.writeFinalSaveDerivedVar(b, o); err != nil {
					return err
				}
			}
		}
		b.writes("\n")
	}

	b.writes(epilogue)
	return nil
}

func (g *gen) writeFuncImplArgChecks(b *buffer, n *a.Func) error {
	checks := []string(nil)

	for _, o := range n.In().Fields() {
		o := o.AsField()
		oTyp := o.XType()

		// TODO: Also check elements, for array-typed arguments.
		switch {
		case oTyp.IsIOTokenType() || (oTyp.Decorator() == t.IDPtr):
			checks = append(checks, fmt.Sprintf("!%s%s", aPrefix, o.Name().Str(g.tm)))

		case oTyp.IsRefined():
			bounds := [2]*big.Int{}
			for i, bound := range oTyp.Bounds() {
				if bound != nil {
					if cv := bound.ConstValue(); cv != nil {
						bounds[i] = cv
					}
				}
			}
			if qid := oTyp.QID(); qid[0] == t.IDBase {
				if key := qid[1]; key < t.ID(len(numTypeBounds)) {
					ntb := numTypeBounds[key]
					for i := 0; i < 2; i++ {
						if bounds[i] != nil && ntb[i] != nil && bounds[i].Cmp(ntb[i]) == 0 {
							bounds[i] = nil
							continue
						}
					}
				}
			}
			for i, bound := range bounds {
				if bound != nil {
					op := '<'
					if i != 0 {
						op = '>'
					}
					checks = append(checks, fmt.Sprintf("%s%s %c %s", aPrefix, o.Name().Str(g.tm), op, bound))
				}
			}
		}
	}

	if len(checks) == 0 {
		return nil
	}

	b.writes("if (")
	for i, c := range checks {
		if i != 0 {
			b.writes(" || ")
		}
		b.writes(c)
	}
	b.writes(") {\n")
	b.writes("self->private_impl.magic = WUFFS_BASE__DISABLED;\n")
	if g.currFunk.astFunc.Effect().Coroutine() {
		b.writes("return wuffs_base__make_status(wuffs_base__error__bad_argument);\n")
	} else {
		// TODO: don't assume that the return type is empty.
		b.printf("return wuffs_base__make_empty_struct();\n")
	}
	b.writes("}\n")
	return nil
}

var numTypeBounds = [...][2]*big.Int{
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
