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
	"strconv"
	"strings"

	a "github.com/google/wuffs/lang/ast"
	t "github.com/google/wuffs/lang/token"
)

func (g *gen) writeStatement(b *buffer, n *a.Node, depth uint32) error {
	if depth > a.MaxBodyDepth {
		return fmt.Errorf("body recursion depth too large")
	}
	depth++

	if n.Kind() == a.KAssert {
		// Assertions only apply at compile-time.
		return nil
	}

	if (n.Kind() == a.KAssign) && (n.AsAssign().LHS() != nil) && n.AsAssign().RHS().Effect().Coroutine() {
		// Put n's code into its own block, to restrict the scope of the
		// tPrefix temporary variables. This helps avoid "jump bypasses
		// variable initialization" compiler warnings with the coroutine
		// suspension points.
		b.writes("{\n")
		defer b.writes("}\n")
	}

	if g.genlinenum {
		filename, line := n.AsRaw().FilenameLine()
		if i := strings.LastIndexByte(filename, '/'); i >= 0 {
			filename = filename[i+1:]
		}
		if i := strings.LastIndexByte(filename, '\\'); i >= 0 {
			filename = filename[i+1:]
		}
		b.printf("// %s:%d\n", filename, line)
	}

	switch n.Kind() {
	case a.KAssign:
		n := n.AsAssign()
		return g.writeStatementAssign(b, n.Operator(), n.LHS(), n.RHS(), depth)
	case a.KChoose:
		return g.writeStatementChoose(b, n.AsChoose(), depth)
	case a.KIOManip:
		return g.writeStatementIOManip(b, n.AsIOManip(), depth)
	case a.KIf:
		return g.writeStatementIf(b, n.AsIf(), depth)
	case a.KIterate:
		return g.writeStatementIterate(b, n.AsIterate(), depth)
	case a.KJump:
		return g.writeStatementJump(b, n.AsJump(), depth)
	case a.KRet:
		return g.writeStatementRet(b, n.AsRet(), depth)
	case a.KVar:
		return nil
	case a.KWhile:
		return g.writeStatementWhile(b, n.AsWhile(), depth)
	}
	return fmt.Errorf("unrecognized ast.Kind (%s) for writeStatement", n.Kind())
}

func (g *gen) writeStatementAssign(b *buffer, op t.ID, lhs *a.Expr, rhs *a.Expr, depth uint32) error {
	if depth > a.MaxExprDepth {
		return fmt.Errorf("expression recursion depth too large")
	}
	depth++

	needWriteLoadExprDerivedVars := false
	if (len(g.currFunk.derivedVars) > 0) &&
		(rhs.Operator() == a.ExprOperatorCall) {
		method := rhs.LHS().AsExpr()
		recvTyp := method.LHS().MType().Pointee()
		if (recvTyp.Decorator() == 0) && (recvTyp.QID()[0] != t.IDBase) {
			n := len(*b)
			if err := g.writeSaveExprDerivedVars(b, rhs); err != nil {
				return err
			}
			needWriteLoadExprDerivedVars = n != len(*b)
		}
	}

	couldSuspend, skipRHS := false, false
	if rhs.Effect().Coroutine() {
		if err := g.writeBuiltinQuestionCall(b, rhs, 0); err == nil {
			skipRHS = true
		} else if err != errNoSuchBuiltin {
			return err
		} else if op != t.IDEqQuestion {
			if err := g.writeCoroSuspPoint(b, false); err != nil {
				return err
			}
			b.writes("status = ")
			couldSuspend = true
		}
	}

	if err := g.writeStatementAssign1(b, op, lhs, rhs, skipRHS); err != nil {
		return err
	}
	if needWriteLoadExprDerivedVars {
		if err := g.writeLoadExprDerivedVars(b, rhs); err != nil {
			return err
		}
	}
	if couldSuspend {
		b.writes("if (status.repr) {\ngoto suspend;\n}\n")
	}
	return nil
}

func (g *gen) writeStatementAssign1(b *buffer, op t.ID, lhs *a.Expr, rhs *a.Expr, skipRHS bool) error {
	lhsBuf := buffer(nil)
	opName, closer, disableWconversion := "", "", false

	if lhs != nil {
		if err := g.writeExpr(&lhsBuf, lhs, false, 0); err != nil {
			return err
		}

		if lTyp := lhs.MType(); lTyp.IsEitherArrayType() {
			b.writes("memcpy(")
			opName, closer = ",", fmt.Sprintf(", sizeof(%s))", lhsBuf)

		} else {
			switch op {
			case t.IDEqQuestion:
				opName = cOpName(t.IDEqQuestion)
				if g.currFunk.tempW > maxTemp {
					return fmt.Errorf("too many temporary variables required")
				}
				temp := g.currFunk.tempW
				g.currFunk.tempW++

				b.printf("wuffs_base__status %s%d = ", tPrefix, temp)

				if err := g.writeExpr(b, rhs, false, 0); err != nil {
					return err
				}
				b.writes(";\n")

			case t.IDTildeSatPlusEq, t.IDTildeSatMinusEq:
				uBits := uintBits(lTyp.QID())
				if uBits == 0 {
					return fmt.Errorf("unsupported tilde-operator type %q", lTyp.Str(g.tm))
				}
				uOp := "add"
				if op != t.IDTildeSatPlusEq {
					uOp = "sub"
				}
				b.printf("wuffs_private_impl__u%d__sat_%s_indirect(&", uBits, uOp)
				opName, closer = ", ", ")"

			default:
				opName = cOpName(op)
				if opName == "" {
					return fmt.Errorf("unrecognized operator %q", op.AmbiguousForm().Str(g.tm))
				}

				if op.IsAssign() && lTyp.IsSmallInteger() {
					switch op {
					case t.IDAmpEq, t.IDPipeEq, t.IDHatEq, t.IDEq, t.IDEqQuestion:
						// No-op.

					default:
						disableWconversion = true
					}
				}
			}
		}
	}

	const disableWconversion0 = "" +
		"#if defined(__GNUC__)\n" +
		"#pragma GCC diagnostic push\n" +
		"#pragma GCC diagnostic ignored \"-Wconversion\"\n" +
		"#endif\n"

	const disableWconversion1 = "" +
		"#if defined(__GNUC__)\n" +
		"#pragma GCC diagnostic pop\n" +
		"#endif\n"

	// "x += 1" triggers -Wconversion, if x is smaller than an int (i.e. a
	// uint8_t or a uint16_t). This is arguably a clang/gcc bug, but in any
	// case, we work around it in Wuffs.
	if disableWconversion && !b.undoWrites(disableWconversion1) {
		b.writes(disableWconversion0)
	}

	n := len(*b)
	b.writex(lhsBuf)
	b.writes(opName)
	if g.currFunk.tempR != g.currFunk.tempW {
		if g.currFunk.tempR != (g.currFunk.tempW - 1) {
			return fmt.Errorf("internal error: temporary variable count out of sync")
		}
		b.printf("%s%d", tPrefix, g.currFunk.tempR)
		g.currFunk.tempR++
	} else if skipRHS {
		// No-op.
	} else if err := g.writeExpr(b, rhs, lhs == nil, 0); err != nil {
		return err
	}
	b.writes(closer)
	if n != len(*b) {
		b.writes(";\n")
	}

	if disableWconversion {
		b.writes(disableWconversion1)
	}

	return nil
}

func (g *gen) writeStatementChoose(b *buffer, n *a.Choose, depth uint32) error {
	recv := g.currFunk.astFunc.Receiver()
	args := n.Args()
	if len(args) == 0 {
		return nil
	}
	b.printf("self->private_impl.choosy_%s = (\n", n.Name().Str(g.tm))

	conclusive := false
	for _, o := range args {
		id := o.AsExpr().Ident()
		suffix := ""
		if n.Name() == id {
			suffix = "__choosy_default"
		}
		caMacro, caName, _, err := cpuArchCNames(g.findAstFunc(t.QQID{recv[0], recv[1], id}).Asserts())
		if err != nil {
			return err
		}
		if caMacro == "" {
			b.printf("&%s%s__%s%s", g.pkgPrefix, recv.Str(g.tm), id.Str(g.tm), suffix)
			conclusive = true
			break
		}
		b.printf("#if defined(WUFFS_PRIVATE_IMPL__CPU_ARCH__%s)\n"+
			"wuffs_base__cpu_arch__have_%s() ? &%s%s__%s%s :\n"+
			"#endif\n",
			caMacro, caName, g.pkgPrefix, recv.Str(g.tm), id.Str(g.tm), suffix)
	}

	if !conclusive {
		b.printf("self->private_impl.choosy_%s", n.Name().Str(g.tm))
	}
	b.writes(");\n")
	return nil
}

func cpuArchCNames(asserts []*a.Node) (caMacro string, caName string, caAttribute string, retErr error) {
	match := false
	for _, o := range asserts {
		if o := o.AsAssert(); o.IsChooseCPUArch() {
			if match {
				// TODO: support multiple choose-cpu_arch preconditions?
				return "", "", "", fmt.Errorf("too many choose-cpu_arch preconditions")
			}
			match = true

			switch o.Condition().RHS().AsExpr().Ident() {
			case t.IDARMCRC32:
				caMacro, caName, caAttribute = "ARM_CRC32", "arm_crc32", ""
			case t.IDARMNeon:
				caMacro, caName, caAttribute = "ARM_NEON", "arm_neon", ""
			case t.IDX86SSE42:
				caMacro, caName, caAttribute =
					"X86_64_V2",
					"x86_sse42",
					"WUFFS_BASE__MAYBE_ATTRIBUTE_TARGET(\"pclmul,popcnt,sse4.2\")"
			case t.IDX86AVX2:
				caMacro, caName, caAttribute =
					"X86_64_V3",
					"x86_avx2",
					"WUFFS_BASE__MAYBE_ATTRIBUTE_TARGET(\"pclmul,popcnt,sse4.2,avx2\")"
			case t.IDX86BMI2:
				caMacro, caName, caAttribute =
					"X86_64_V3",
					"x86_bmi2",
					"WUFFS_BASE__MAYBE_ATTRIBUTE_TARGET(\"bmi2\")"
			}
		}
	}
	return caMacro, caName, caAttribute, nil
}

func (g *gen) writeStatementIOManip(b *buffer, n *a.IOManip, depth uint32) error {
	if g.currFunk.ioManips > maxIOManips {
		return fmt.Errorf("too many temporary variables required")
	}
	ioBindNum := g.currFunk.ioManips
	g.currFunk.ioManips++

	e := n.IO()
	prefix := vPrefix
	if e.Operator() != 0 {
		prefix = aPrefix
	}
	cTyp, end, qualifier, isWriter := "reader", "meta.wi", "const ", false
	if e.MType().QID()[1] == t.IDIOWriter {
		cTyp, end, qualifier, isWriter = "writer", "data.len", "", true
	}
	name := e.Ident().Str(g.tm)

	// TODO: do these variables need to be func-scoped (bigger scope)
	// instead of block-scoped (smaller scope) if the coro_susp_point
	// switch can jump past this initialization??
	b.writes("{\n")
	switch n.Keyword() {
	case t.IDIOBind:
		b.printf("wuffs_base__io_buffer* %s%d_%s%s = %s%s;\n",
			oPrefix, ioBindNum, prefix, name,
			prefix, name)
		b.printf("%suint8_t* %s%d_%s%s%s = %s%s%s;\n",
			qualifier, oPrefix, ioBindNum, iopPrefix, prefix, name,
			iopPrefix, prefix, name)
		b.printf("%suint8_t* %s%d_%s%s%s = %s%s%s;\n",
			qualifier, oPrefix, ioBindNum, io0Prefix, prefix, name,
			io0Prefix, prefix, name)
		b.printf("%suint8_t* %s%d_%s%s%s = %s%s%s;\n",
			qualifier, oPrefix, ioBindNum, io1Prefix, prefix, name,
			io1Prefix, prefix, name)
		b.printf("%suint8_t* %s%d_%s%s%s = %s%s%s;\n",
			qualifier, oPrefix, ioBindNum, io2Prefix, prefix, name,
			io2Prefix, prefix, name)
		b.printf("%s%s = wuffs_private_impl__io_%s__set("+
			"\n&%s%s,\n&%s%s%s,\n&%s%s%s,\n&%s%s%s,\n&%s%s%s,\n",
			prefix, name, cTyp,
			uPrefix, name,
			iopPrefix, prefix, name,
			io0Prefix, prefix, name,
			io1Prefix, prefix, name,
			io2Prefix, prefix, name)
		if err := g.writeExpr(b, n.Arg1(), false, 0); err != nil {
			return err
		}
		b.writes(",\n")
		if err := g.writeExpr(b, n.HistoryPosition(), false, 0); err != nil {
			return err
		}
		b.writes(");\n")

	case t.IDIOForgetHistory:
		if !isWriter {
			return fmt.Errorf("unsupported io_forget_history for an io_reader")
		}
		b.printf("%suint8_t* %s%d_%s%s%s = %s%s%s;\n",
			qualifier, oPrefix, ioBindNum, io0Prefix, prefix, name,
			io0Prefix, prefix, name)
		b.printf("%suint8_t* %s%d_%s%s%s = %s%s%s;\n",
			qualifier, oPrefix, ioBindNum, io1Prefix, prefix, name,
			io1Prefix, prefix, name)
		b.printf("%s%s%s = %s%s%s;\n",
			io0Prefix, prefix, name, iopPrefix, prefix, name)
		b.printf("%s%s%s = %s%s%s;\n",
			io1Prefix, prefix, name, iopPrefix, prefix, name)
		b.printf("wuffs_base__io_buffer %s%d_%s%s;\n",
			oPrefix, ioBindNum, prefix, name)
		b.printf("if (%s%s) {\n",
			prefix, name)
		if isWriter {
			b.printf("memcpy(&%s%d_%s%s, %s%s, sizeof(*%s%s));\n",
				oPrefix, ioBindNum, prefix, name,
				prefix, name,
				prefix, name)
			b.printf("size_t wi%d = %s%s->meta.wi;\n",
				ioBindNum, prefix, name)
			b.printf("%s%s->data.ptr += wi%d;\n",
				prefix, name, ioBindNum)
			b.printf("%s%s->data.len -= wi%d;\n",
				prefix, name, ioBindNum)
			b.printf("%s%s->meta.ri = 0;\n",
				prefix, name)
			b.printf("%s%s->meta.wi = 0;\n",
				prefix, name)
			b.printf("%s%s->meta.pos = wuffs_base__u64__sat_add(%s%s->meta.pos, wi%d);\n",
				prefix, name, prefix, name, ioBindNum)
		} else {
			// TODO.
		}
		b.printf("}\n")

	case t.IDIOLimit:
		if !isWriter {
			b.printf("const bool %s%d_closed_%s%s = %s%s->meta.closed;\n",
				oPrefix, ioBindNum, prefix, name, prefix, name)
		}
		b.printf("%suint8_t* %s%d_%s%s%s = %s%s%s;\n",
			qualifier, oPrefix, ioBindNum, io2Prefix, prefix, name, io2Prefix, prefix, name)
		b.printf("wuffs_private_impl__io_%s__limit(&%s%s%s, %s%s%s,\n",
			cTyp,
			io2Prefix, prefix, name,
			iopPrefix, prefix, name)
		if err := g.writeExpr(b, n.Arg1(), false, 0); err != nil {
			return err
		}
		b.writes(");\n")
		b.printf("if (%s%s) {\n", prefix, name)
		b.printf("size_t n = ((size_t)(%s%s%s - %s%s->data.ptr));\n",
			io2Prefix, prefix, name, prefix, name)
		if !isWriter {
			b.printf("%s%s->meta.closed = %s%s->meta.closed && (%s%s->%s <= n);\n",
				prefix, name, prefix, name, prefix, name, end)
		}
		b.printf("%s%s->%s = n;\n",
			prefix, name, end)
		b.printf("}\n")
	}

	for _, o := range n.Body() {
		if err := g.writeStatement(b, o, depth); err != nil {
			return err
		}
	}

	switch n.Keyword() {
	case t.IDIOBind:
		b.printf("%s%s = %s%d_%s%s;\n",
			prefix, name,
			oPrefix, ioBindNum, prefix, name)
		b.printf("%s%s%s = %s%d_%s%s%s;\n",
			iopPrefix, prefix, name,
			oPrefix, ioBindNum, iopPrefix, prefix, name)
		b.printf("%s%s%s = %s%d_%s%s%s;\n",
			io0Prefix, prefix, name,
			oPrefix, ioBindNum, io0Prefix, prefix, name)
		b.printf("%s%s%s = %s%d_%s%s%s;\n",
			io1Prefix, prefix, name,
			oPrefix, ioBindNum, io1Prefix, prefix, name)
		b.printf("%s%s%s = %s%d_%s%s%s;\n",
			io2Prefix, prefix, name,
			oPrefix, ioBindNum, io2Prefix, prefix, name)

	case t.IDIOForgetHistory:
		b.printf("if (%s%s) {\n",
			prefix, name)
		if isWriter {
			b.printf("memcpy(%s%s, &%s%d_%s%s, sizeof(*%s%s));\n",
				prefix, name,
				oPrefix, ioBindNum, prefix, name,
				prefix, name)
			b.printf("%s%s->meta.wi = ((size_t)(%s%s%s - %s%s->data.ptr));\n",
				prefix, name, iopPrefix, prefix, name, prefix, name)
			b.printf("%s%s%s = %s%d_%s%s%s;\n",
				io0Prefix, prefix, name,
				oPrefix, ioBindNum, io0Prefix, prefix, name)
			b.printf("%s%s%s = %s%d_%s%s%s;\n",
				io1Prefix, prefix, name,
				oPrefix, ioBindNum, io1Prefix, prefix, name)
		} else {
			// TODO.
		}
		b.printf("}\n")

	case t.IDIOLimit:
		b.printf("%s%s%s = %s%d_%s%s%s;\n",
			io2Prefix, prefix, name,
			oPrefix, ioBindNum, io2Prefix, prefix, name)
		b.printf("if (%s%s) {\n", prefix, name)
		if !isWriter {
			b.printf("%s%s->meta.closed = %s%d_closed_%s%s;\n",
				prefix, name, oPrefix, ioBindNum, prefix, name)
		}
		b.printf("%s%s->%s = ((size_t)(%s%s%s - %s%s->data.ptr));\n",
			prefix, name, end,
			io2Prefix, prefix, name,
			prefix, name)
		b.printf("}\n")
	}
	b.writes("}\n")
	return nil
}

func (g *gen) writeStatementIf(b *buffer, n *a.If, depth uint32) error {
	// For an "if true { etc }", just write the "etc".
	if cv := n.Condition().ConstValue(); (cv != nil) && (cv.Cmp(one) == 0) &&
		(n.ElseIf() == nil) && (len(n.BodyIfFalse()) == 0) {

		for _, o := range n.BodyIfTrue() {
			if err := g.writeStatement(b, o, depth); err != nil {
				return err
			}
		}
		return nil
	}

	for {
		prefix, suffix := "", ""
		switch n.Likelihood() {
		case t.IDLikely:
			prefix, suffix = "WUFFS_BASE__LIKELY(", ")"
		case t.IDUnlikely:
			prefix, suffix = "WUFFS_BASE__UNLIKELY(", ")"
		}
		condition := buffer(nil)
		if err := g.writeExpr(&condition, n.Condition(), false, 0); err != nil {
			return err
		}
		// Calling trimParens avoids clang's -Wparentheses-equality warning.
		b.printf("if (%s%s%s) {\n", prefix, trimParens(condition), suffix)
		for _, o := range n.BodyIfTrue() {
			if err := g.writeStatement(b, o, depth); err != nil {
				return err
			}
		}
		if bif := n.BodyIfFalse(); len(bif) > 0 {
			b.writes("} else {\n")
			for _, o := range bif {
				if err := g.writeStatement(b, o, depth); err != nil {
					return err
				}
			}
			break
		}
		n = n.ElseIf()
		if n == nil {
			break
		}
		b.writes("} else ")
	}
	b.writes("}\n")
	return nil
}

func (g *gen) writeStatementIterate(b *buffer, n *a.Iterate, depth uint32) error {
	assigns := n.Assigns()
	if len(assigns) == 0 {
		return nil
	}
	name0 := assigns[0].AsAssign().LHS().Ident().Str(g.tm)
	g.currFunk.activeLoops.Push(n)
	b.writes("{\n")

	// TODO: don't assume that the slice is a slice of base.u8. In
	// particular, the code gen can be subtle if the slice element type has
	// zero size, such as the empty struct.
	for i, o := range assigns {
		o := o.AsAssign()
		name := o.LHS().Ident().Str(g.tm)
		b.printf("wuffs_base__slice_u8 %sslice_%s = ", iPrefix, name)
		if err := g.writeExpr(b, o.RHS(), false, 0); err != nil {
			return err
		}
		b.writes(";\n")
		b.printf("%s%s.ptr = %sslice_%s.ptr;\n", vPrefix, name, iPrefix, name)
		if i > 0 {
			b.printf("%sslice_%s.len = ((size_t)(wuffs_base__u64__min(%sslice_%s.len, %sslice_%s.len)));\n",
				iPrefix, name0, iPrefix, name0, iPrefix, name)
		}
	}
	// TODO: look at n.HasContinue() and n.HasBreak().

	round := uint32(0)
	for ; n != nil; n = n.ElseIterate() {
		length, err := strconv.Atoi(n.Length().Str(g.tm))
		if err != nil {
			return err
		}
		advance, err := strconv.Atoi(n.Advance().Str(g.tm))
		if err != nil {
			return err
		}
		unroll, err := strconv.Atoi(n.Unroll().Str(g.tm))
		if err != nil {
			return err
		}
		for {
			if err := g.writeIterateRound(b, assigns, n.Body(), round, depth, length, advance, unroll); err != nil {
				return err
			}
			round++

			if unroll == 1 {
				break
			}
			unroll = 1
		}
	}
	for _, o := range assigns {
		name := o.AsAssign().LHS().Ident().Str(g.tm)
		b.printf("%s%s.len = 0;\n", vPrefix, name)
	}

	b.writes("}\n")
	g.currFunk.activeLoops.Pop()
	return nil
}

func (g *gen) writeStatementJump(b *buffer, n *a.Jump, depth uint32) error {
	keyword := "continue"
	if n.Keyword() == t.IDBreak {
		keyword = "break"
	}
	if n.JumpTarget() == g.currFunk.activeLoops.Top() {
		b.printf("%s;\n", keyword)
	} else if jt, err := g.currFunk.jumpTarget(g.tm, n.JumpTarget()); err != nil {
		return err
	} else {
		b.printf("goto label__%s__%s;\n", jt, keyword)
	}
	return nil
}

func (g *gen) writeStatementRet(b *buffer, n *a.Ret, depth uint32) error {
	retExpr := n.Value()

	if g.currFunk.astFunc.Effect().Coroutine() ||
		(g.currFunk.returnsStatus && (len(g.currFunk.derivedVars) > 0)) {

		isComplete := false
		b.writes("status = ")
		if retExpr.Operator() == 0 && retExpr.Ident() == t.IDOk {
			b.writes("wuffs_base__make_status(NULL)")
			isComplete = true
		} else {
			if retExpr.Ident().IsDQStrLiteral(g.tm) {
				msg, _ := t.Unescape(retExpr.Ident().Str(g.tm))
				isComplete = statusMsgIsNote(msg)
			}
			if err := g.writeExpr(b, retExpr, false, depth); err != nil {
				return err
			}
		}
		b.writes(";\n")

		if n.Keyword() == t.IDYield {
			return g.writeCoroSuspPoint(b, true)
		}

		if n.RetsError() {
			b.writes("goto exit;\n")
		} else if isComplete {
			g.currFunk.hasGotoOK = true
			b.writes("goto ok;\n")
		} else {
			g.currFunk.hasGotoOK = true
			// TODO: the "goto exit"s can be "goto ok".
			b.writes("if (wuffs_base__status__is_error(&status)) {\ngoto exit;\n} " +
				"else if (wuffs_base__status__is_suspension(&status)) {\n" +
				"status = wuffs_base__make_status(wuffs_base__error__cannot_return_a_suspension);\ngoto exit;\n}\ngoto ok;\n")
		}
		return nil
	}

	if g.currFunk.derivedVars != nil {
		for _, o := range g.currFunk.astFunc.In().Fields() {
			o := o.AsField()
			if _, ok := g.currFunk.derivedVars[o.Name()]; ok {
				if err := g.writeFinalSaveDerivedVar(b, o); err != nil {
					return err
				}
			}
		}
	}

	b.writes("return ")
	if g.currFunk.astFunc.Out() == nil {
		b.writes("wuffs_base__make_empty_struct()")
	} else {
		couldBeSuspension := false
		if retExpr.MType().IsStatus() && !n.RetsError() {
			couldBeSuspension = true
			if s := g.tm.ByID(retExpr.Ident()); (len(s) > 1) && (s[0] == '"') {
				couldBeSuspension = s[1] == '$'
			} else if (retExpr.Operator() == 0) && (retExpr.Ident() == t.IDOk) {
				couldBeSuspension = false
			}
		}

		if couldBeSuspension {
			b.writes("wuffs_private_impl__status__ensure_not_a_suspension(")
		}
		if err := g.writeExpr(b, retExpr, false, depth); err != nil {
			return err
		}
		if couldBeSuspension {
			b.writeb(')')
		}
	}

	b.writes(";\n")
	return nil
}

func (g *gen) writeStatementWhile(b *buffer, n *a.While, depth uint32) error {
	body, isTrivialLoop := n.Body(), false
	if n.IsWhileTrue() && !n.HasContinue() && (len(body) > 0) {
		if finalStatement := body[len(body)-1]; finalStatement.Kind() != a.KJump {
			// No-op.
		} else if j := finalStatement.AsJump(); (j.Keyword() == t.IDBreak) && (j.JumpTarget() == n) {
			body, isTrivialLoop = body[:len(body)-1], true
		}
	}

	if n.HasDeepContinue() {
		jt, err := g.currFunk.jumpTarget(g.tm, n)
		if err != nil {
			return err
		}
		b.printf("label__%s__continue:;\n", jt)
	}
	g.currFunk.activeLoops.Push(n)

	if isTrivialLoop {
		b.writes("do {\n")
	} else {
		condition := buffer(nil)
		if err := g.writeExpr(&condition, n.Condition(), false, 0); err != nil {
			return err
		}
		// Calling trimParens avoids clang's -Wparentheses-equality warning.
		b.printf("while (%s) {\n", trimParens(condition))
	}

	for _, o := range body {
		if err := g.writeStatement(b, o, depth); err != nil {
			return err
		}
	}

	if isTrivialLoop {
		b.writes("} while (0);\n")
	} else {
		b.writes("}\n")
	}

	g.currFunk.activeLoops.Pop()
	if n.HasDeepBreak() {
		jt, err := g.currFunk.jumpTarget(g.tm, n)
		if err != nil {
			return err
		}
		b.printf("label__%s__break:;\n", jt)
	}
	return nil
}

func (g *gen) writeIterateRound(b *buffer, assigns []*a.Node, body []*a.Node, round uint32, depth uint32, length int, advance int, unroll int) error {
	for _, o := range assigns {
		name := o.AsAssign().LHS().Ident().Str(g.tm)
		b.printf("%s%s.len = %d;\n", vPrefix, name, length)
	}
	name0 := assigns[0].AsAssign().LHS().Ident().Str(g.tm)
	b.printf("const uint8_t* %send%d_%s = wuffs_private_impl__ptr_u8_plus_len(", iPrefix, round, name0)
	if (length == 1) && (advance == 1) && (unroll == 1) {
		b.printf("%sslice_%s.ptr, %sslice_%s.len);\n",
			iPrefix, name0, iPrefix, name0)
	} else if length == advance {
		b.printf("%s%s.ptr, (((%sslice_%s.len - (size_t)(%s%s.ptr - %sslice_%s.ptr)) / %d) * %d));\n",
			vPrefix, name0, iPrefix, name0,
			vPrefix, name0, iPrefix, name0,
			length*unroll, length*unroll)
	} else {
		b.printf("%s%s.ptr, wuffs_private_impl__iterate_total_advance("+
			"(%sslice_%s.len - (size_t)(%s%s.ptr - %sslice_%s.ptr)), %d, %d));\n",
			vPrefix, name0, iPrefix, name0,
			vPrefix, name0, iPrefix, name0,
			length+(advance*(unroll-1)), advance*unroll)
	}
	b.printf("while (%s%s.ptr < %send%d_%s) {\n", vPrefix, name0, iPrefix, round, name0)
	for i := 0; i < unroll; i++ {
		for _, o := range body {
			if err := g.writeStatement(b, o, depth); err != nil {
				return err
			}
		}
		for _, o := range assigns {
			name := o.AsAssign().LHS().Ident().Str(g.tm)
			b.printf("%s%s.ptr += %d;\n", vPrefix, name, advance)
		}
	}
	b.writes("}\n")
	return nil
}

func (g *gen) writeCoroSuspPoint(b *buffer, maybeSuspend bool) error {
	const maxCoroSuspPoint = 0xFFFFFFFF
	g.currFunk.coroSuspPoint++
	if g.currFunk.coroSuspPoint == maxCoroSuspPoint {
		return fmt.Errorf("too many coroutine suspension points required")
	}

	macro := ""
	if maybeSuspend {
		macro = "_MAYBE_SUSPEND"
		g.currFunk.hasGotoOK = true
	}
	b.printf("WUFFS_BASE__COROUTINE_SUSPENSION_POINT%s(%d);\n", macro, g.currFunk.coroSuspPoint)
	return nil
}

func trimParens(b []byte) []byte {
	if len(b) > 1 && b[0] == '(' && b[len(b)-1] == ')' {
		return b[1 : len(b)-1]
	}
	return b
}
