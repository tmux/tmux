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
	"math/big"
	"strings"

	a "github.com/google/wuffs/lang/ast"
	t "github.com/google/wuffs/lang/token"
)

var (
	errNoSuchBuiltin             = errors.New("cgen: internal error: no such built-in")
	errOptimizationNotApplicable = errors.New("cgen: internal error: optimization not applicable")
)

func (g *gen) writeBuiltinCall(b *buffer, n *a.Expr, sideEffectsOnly bool, depth uint32) error {
	if n.Operator() != a.ExprOperatorCall {
		return errNoSuchBuiltin
	}
	method := n.LHS().AsExpr()
	recv := method.LHS().AsExpr()
	recvTyp := recv.MType()

	switch recvTyp.Decorator() {
	case 0:
		// No-op.

	case t.IDNptr, t.IDPtr:
		u64ToFlicksIndex := -1

		if method.Ident() != t.IDSet {
			return errNoSuchBuiltin
		}
		recvName, err := g.recvName(recv)
		if err != nil {
			return err
		}

		switch recvTyp.Inner().QID() {
		case t.QID{t.IDBase, t.IDImageConfig}:
			b.printf("wuffs_base__image_config__set(\n%s", recvName)
		case t.QID{t.IDBase, t.IDFrameConfig}:
			b.printf("wuffs_base__frame_config__set(\n%s", recvName)
			u64ToFlicksIndex = 1
		default:
			return errNoSuchBuiltin
		}

		for i, o := range n.Args() {
			b.writes(",\n")
			if i == u64ToFlicksIndex {
				if o.AsArg().Name().Str(g.tm) != "duration" {
					return errors.New("cgen: internal error: inconsistent frame_config.set argument")
				}
				b.writes("((wuffs_base__flicks)(")
			}
			if err := g.writeExpr(b, o.AsArg().Value(), false, depth); err != nil {
				return err
			}
			if i == u64ToFlicksIndex {
				b.writes("))")
			}
		}
		b.writeb(')')
		return nil

	case t.IDRoslice, t.IDSlice:
		return g.writeBuiltinSlice(b, recv, method.Ident(), n.Args(), sideEffectsOnly, depth)
	case t.IDRotable, t.IDTable:
		return g.writeBuiltinTable(b, recv, method.Ident(), n.Args(), sideEffectsOnly, depth)
	default:
		return errNoSuchBuiltin
	}

	qid := recvTyp.QID()
	if qid[0] != t.IDBase {
		return errNoSuchBuiltin
	}

	if qid[1].IsNumType() {
		return g.writeBuiltinNumType(b, recv, method.Ident(), n.Args(), depth)
	} else if qid[1].IsBuiltInCPUArch() {
		return g.writeBuiltinCPUArch(b, recv, method.Ident(), n.MType(), n.Args(), sideEffectsOnly, depth)
	} else {
		switch qid[1] {
		case t.IDIOReader:
			return g.writeBuiltinIOReader(b, recv, method.Ident(), n.Args(), sideEffectsOnly, depth)
		case t.IDIOWriter:
			return g.writeBuiltinIOWriter(b, recv, method.Ident(), n.Args(), sideEffectsOnly, depth)
		case t.IDPixelSwizzler:
			switch method.Ident() {
			case t.IDLimitedSwizzleU32InterleavedFromReader, t.IDSwizzleInterleavedFromReader:
				b.writes("wuffs_base__pixel_swizzler__")
				if method.Ident() == t.IDLimitedSwizzleU32InterleavedFromReader {
					b.writes("limited_swizzle_u32_interleaved_from_reader")
				} else {
					b.writes("swizzle_interleaved_from_reader")
				}
				b.writes("(\n&")
				if err := g.writeExpr(b, recv, false, depth); err != nil {
					return err
				}
				args := n.Args()
				for _, o := range args[:len(args)-1] {
					b.writes(",\n")
					if err := g.writeExpr(b, o.AsArg().Value(), false, depth); err != nil {
						return err
					}
				}
				readerArgName, err := g.recvName(args[len(args)-1].AsArg().Value())
				if err != nil {
					return err
				}
				b.printf(",\n&%s%s,\n%s%s)", iopPrefix, readerArgName, io2Prefix, readerArgName)
				return nil
			}
		case t.IDTokenWriter:
			return g.writeBuiltinTokenWriter(b, recv, method.Ident(), n.Args(), sideEffectsOnly, depth)
		case t.IDUtility:
			switch method.Ident() {
			case t.IDCPUArchIs32Bit:
				b.writes("(sizeof(void*) == 4u)")
				return nil
			case t.IDEmptyIOReader, t.IDEmptyIOWriter:
				if !g.currFunk.usesEmptyIOBuffer {
					g.currFunk.usesEmptyIOBuffer = true
					g.currFunk.bPrologue.writes("wuffs_base__io_buffer empty_io_buffer = " +
						"wuffs_base__empty_io_buffer();\n\n")
				}
				b.writes("&empty_io_buffer")
				return nil
			}
		}
	}
	return errNoSuchBuiltin
}

func (g *gen) recvName(recv *a.Expr) (string, error) {
	switch recv.Operator() {
	case 0:
		return vPrefix + recv.Ident().Str(g.tm), nil
	case t.IDDot:
		if lhs := recv.LHS().AsExpr(); lhs.Operator() == 0 && lhs.Ident() == t.IDArgs {
			return aPrefix + recv.Ident().Str(g.tm), nil
		}
	}
	return "", fmt.Errorf("recvName: cannot cgen a %q expression", recv.Str(g.tm))
}

func (g *gen) writeBuiltinIO(b *buffer, recv *a.Expr, method t.ID, args []*a.Node, sideEffectsOnly bool, depth uint32) error {
	switch method {
	case t.IDLength:
		recvName, err := g.recvName(recv)
		if err != nil {
			return err
		}
		b.printf("((uint64_t)(%s%s - %s%s))", io2Prefix, recvName, iopPrefix, recvName)
		return nil

	case t.IDPeekUndoByte:
		recvName, err := g.recvName(recv)
		if err != nil {
			return err
		}
		b.printf("%s%s[-1]", iopPrefix, recvName)
		return nil

	case t.IDUndoByte:
		recvName, err := g.recvName(recv)
		if err != nil {
			return err
		}
		if !sideEffectsOnly {
			// Generate a two part expression using the comma operator: "(etc,
			// return_empty_struct call)". The final part is a function call
			// (to a static inline function) instead of a struct literal, to
			// avoid a "expression result unused" compiler error.
			b.writes("(")
		}
		b.printf("%s%s--", iopPrefix, recvName)
		if !sideEffectsOnly {
			b.writes(", wuffs_base__make_empty_struct())")
		}
		return nil

	case t.IDCanUndoByte:
		recvName, err := g.recvName(recv)
		if err != nil {
			return err
		}
		b.printf("(%s%s > %s%s)", iopPrefix, recvName, io1Prefix, recvName)
		return nil
	}
	return errNoSuchBuiltin
}

func (g *gen) writeBuiltinIOReader(b *buffer, recv *a.Expr, method t.ID, args []*a.Node, sideEffectsOnly bool, depth uint32) error {
	recvName, err := g.recvName(recv)
	if err != nil {
		return err
	}

	switch method {
	case t.IDValidUTF8Length:
		b.printf("((uint64_t)(wuffs_base__utf_8__longest_valid_prefix(%s%s,\n"+
			"((size_t)(wuffs_base__u64__min(((uint64_t)(%s%s - %s%s)), ",
			iopPrefix, recvName, io2Prefix, recvName, iopPrefix, recvName)
		if err := g.writeExpr(b, args[0].AsArg().Value(), false, depth); err != nil {
			return err
		}
		b.writes("))))))")
		return nil

	case t.IDLimitedCopyU32ToSlice:
		b.printf("wuffs_private_impl__io_reader__limited_copy_u32_to_slice(\n&%s%s, %s%s,",
			iopPrefix, recvName, io2Prefix, recvName)
		return g.writeArgs(b, args, depth)

	case t.IDCountSince:
		b.printf("wuffs_private_impl__io__count_since(")
		if err := g.writeExpr(b, args[0].AsArg().Value(), false, depth); err != nil {
			return err
		}
		b.printf(", ((uint64_t)(%s%s - %s%s)))", iopPrefix, recvName, io0Prefix, recvName)
		return nil

	case t.IDIsClosed:
		b.printf("(%s && %s->meta.closed)", recvName, recvName)
		return nil

	case t.IDMark:
		b.printf("((uint64_t)(%s%s - %s%s))", iopPrefix, recvName, io0Prefix, recvName)
		return nil

	case t.IDMatch7:
		b.printf("wuffs_private_impl__io_reader__match7(%s%s, %s%s, %s, ",
			iopPrefix, recvName, io2Prefix, recvName, recvName)
		if err := g.writeExpr(b, args[0].AsArg().Value(), false, depth); err != nil {
			return err
		}
		b.writeb(')')
		return nil

	case t.IDPeekU8At:
		b.printf("%s%s[", iopPrefix, recvName)
		if err := g.writeExpr(b, args[0].AsArg().Value(), false, depth); err != nil {
			return err
		}
		b.writeb(']')
		return nil

	case t.IDPeekU64LEAt:
		b.printf("wuffs_base__peek_u64le__no_bounds_check(%s%s + ", iopPrefix, recvName)
		if err := g.writeExpr(b, args[0].AsArg().Value(), false, depth); err != nil {
			return err
		}
		b.writeb(')')
		return nil

	case t.IDPosition:
		b.printf("wuffs_base__u64__sat_add((%s ? %s->meta.pos : 0), ((uint64_t)(%s%s - %s%s)))",
			recvName, recvName, iopPrefix, recvName, io0Prefix, recvName)
		return nil

	case t.IDSince:
		b.printf("wuffs_private_impl__io__since(")
		if err := g.writeExpr(b, args[0].AsArg().Value(), false, depth); err != nil {
			return err
		}
		b.printf(", ((uint64_t)(%s%s - %s%s)), %s%s)",
			iopPrefix, recvName, io0Prefix, recvName, io0Prefix, recvName)
		return nil

	case t.IDSkipU32Fast:
		if !sideEffectsOnly {
			// Generate a two part expression using the comma operator: "(etc,
			// return_empty_struct call)". The final part is a function call
			// (to a static inline function) instead of a struct literal, to
			// avoid a "expression result unused" compiler error.
			b.writes("(")
		}
		b.printf("%s%s += ", iopPrefix, recvName)
		if err := g.writeExpr(b, args[0].AsArg().Value(), false, depth); err != nil {
			return err
		}
		if !sideEffectsOnly {
			b.writes(", wuffs_base__make_empty_struct())")
		}
		return nil
	}

	if method >= peekMethodsBase {
		if m := method - peekMethodsBase; m < t.ID(len(peekMethods)) {
			if p := peekMethods[m]; p.n != 0 {
				if p.size != p.n {
					b.printf("((uint%d_t)(", p.size)
				}
				b.printf("wuffs_base__peek_u%d%ce__no_bounds_check(%s%s)",
					p.n, p.endianness, iopPrefix, recvName)
				if p.size != p.n {
					b.writes("))")
				}
				return nil
			}
		}
	}

	return g.writeBuiltinIO(b, recv, method, args, sideEffectsOnly, depth)
}

func (g *gen) writeBuiltinIOWriter(b *buffer, recv *a.Expr, method t.ID, args []*a.Node, sideEffectsOnly bool, depth uint32) error {
	recvName, err := g.recvName(recv)
	if err != nil {
		return err
	}

	switch method {
	case t.IDLimitedCopyU32FromHistory,
		t.IDLimitedCopyU32FromHistory8ByteChunksDistance1Fast,
		t.IDLimitedCopyU32FromHistory8ByteChunksDistance1FastReturnCusp,
		t.IDLimitedCopyU32FromHistory8ByteChunksFast,
		t.IDLimitedCopyU32FromHistory8ByteChunksFastReturnCusp,
		t.IDLimitedCopyU32FromHistoryFast,
		t.IDLimitedCopyU32FromHistoryFastReturnCusp:
		b.printf("wuffs_private_impl__io_writer__%s(\n&%s%s, %s%s, %s%s",
			method.Str(g.tm), iopPrefix, recvName, io0Prefix, recvName, io2Prefix, recvName)
		for _, o := range args {
			b.writes(", ")
			if err := g.writeExpr(b, o.AsArg().Value(), false, depth); err != nil {
				return err
			}
		}
		b.writeb(')')
		return nil

	case t.IDLimitedCopyU32FromReader:
		readerName, err := g.recvName(args[1].AsArg().Value())
		if err != nil {
			return err
		}

		b.printf("wuffs_private_impl__io_writer__limited_copy_u32_from_reader(\n&%s%s, %s%s,",
			iopPrefix, recvName, io2Prefix, recvName)
		if err := g.writeExpr(b, args[0].AsArg().Value(), false, depth); err != nil {
			return err
		}
		b.printf(", &%s%s, %s%s)", iopPrefix, readerName, io2Prefix, readerName)
		return nil

	case t.IDCopyFromSlice:
		b.printf("wuffs_private_impl__io_writer__copy_from_slice(&%s%s, %s%s,",
			iopPrefix, recvName, io2Prefix, recvName)
		return g.writeArgs(b, args, depth)

	case t.IDLimitedCopyU32FromSlice:
		b.printf("wuffs_private_impl__io_writer__limited_copy_u32_from_slice(\n&%s%s, %s%s,",
			iopPrefix, recvName, io2Prefix, recvName)
		return g.writeArgs(b, args, depth)

	case t.IDCountSince:
		b.printf("wuffs_private_impl__io__count_since(")
		if err := g.writeExpr(b, args[0].AsArg().Value(), false, depth); err != nil {
			return err
		}
		b.printf(", ((uint64_t)(%s%s - %s%s)))", iopPrefix, recvName, io0Prefix, recvName)
		return nil

	case t.IDHistoryLength, t.IDMark:
		b.printf("((uint64_t)(%s%s - %s%s))", iopPrefix, recvName, io0Prefix, recvName)
		return nil

	case t.IDHistoryPosition:
		b.printf("(%s ? %s->meta.pos : 0u)", recvName, recvName)
		return nil

	case t.IDPosition:
		b.printf("wuffs_base__u64__sat_add((%s ? %s->meta.pos : 0u), ((uint64_t)(%s%s - %s%s)))",
			recvName, recvName, iopPrefix, recvName, io0Prefix, recvName)
		return nil

	case t.IDSince:
		b.printf("wuffs_private_impl__io__since(")
		if err := g.writeExpr(b, args[0].AsArg().Value(), false, depth); err != nil {
			return err
		}
		b.printf(", ((uint64_t)(%s%s - %s%s)), %s%s)",
			iopPrefix, recvName, io0Prefix, recvName, io0Prefix, recvName)
		return nil
	}

	if method >= writeFastMethodsBase {
		if m := method - writeFastMethodsBase; m < t.ID(len(writeFastMethods)) {
			if p := writeFastMethods[m]; p.n != 0 {
				// Generate a two/three part expression using the comma
				// operator: "(store, pointer increment, return_empty_struct
				// call)". The final part is a function call (to a static
				// inline function) instead of a struct literal, to avoid a
				// "expression result unused" compiler error.
				b.printf("(wuffs_base__poke_u%d%ce__no_bounds_check(%s%s, ",
					p.n, p.endianness, iopPrefix, recvName)
				if err := g.writeExpr(b, args[0].AsArg().Value(), false, depth); err != nil {
					return err
				}
				b.printf("), %s%s += %d", iopPrefix, recvName, p.n/8)
				if !sideEffectsOnly {
					b.writes(", wuffs_base__make_empty_struct()")
				}
				b.writes(")")
				return nil
			}
		}
	}

	return g.writeBuiltinIO(b, recv, method, args, sideEffectsOnly, depth)
}

func (g *gen) writeBuiltinTokenWriter(b *buffer, recv *a.Expr, method t.ID, args []*a.Node, sideEffectsOnly bool, depth uint32) error {
	switch method {
	case t.IDWriteSimpleTokenFast, t.IDWriteExtendedTokenFast:
		recvName, err := g.recvName(recv)
		if err != nil {
			return err
		}
		b.printf("*iop_%s++ = wuffs_base__make_token(\n", recvName)

		if method == t.IDWriteSimpleTokenFast {
			b.writes("(((uint64_t)(")
			if cv := args[0].AsArg().Value().ConstValue(); (cv == nil) || (cv.Sign() != 0) {
				if err := g.writeExpr(b, args[0].AsArg().Value(), false, depth); err != nil {
					return err
				}
				b.writes(")) << WUFFS_BASE__TOKEN__VALUE_MAJOR__SHIFT) |\n(((uint64_t)(")
			}

			if err := g.writeExpr(b, args[1].AsArg().Value(), false, depth); err != nil {
				return err
			}
			b.writes(")) << WUFFS_BASE__TOKEN__VALUE_MINOR__SHIFT) |\n(((uint64_t)(")
		} else {
			b.writes("(~")
			if err := g.writeExpr(b, args[0].AsArg().Value(), false, depth); err != nil {
				return err
			}
			b.writes(" << WUFFS_BASE__TOKEN__VALUE_EXTENSION__SHIFT) |\n(((uint64_t)(")
		}

		if cv := args[len(args)-2].AsArg().Value().ConstValue(); (cv == nil) || (cv.Sign() != 0) {
			if err := g.writeExpr(b, args[len(args)-2].AsArg().Value(), false, depth); err != nil {
				return err
			}
			b.writes(")) << WUFFS_BASE__TOKEN__CONTINUED__SHIFT) |\n(((uint64_t)(")
		}

		if err := g.writeExpr(b, args[len(args)-1].AsArg().Value(), false, depth); err != nil {
			return err
		}
		b.writes(")) << WUFFS_BASE__TOKEN__LENGTH__SHIFT))")
		return nil
	}

	return g.writeBuiltinIO(b, recv, method, args, sideEffectsOnly, depth)
}

func (g *gen) writeBuiltinCPUArch(b *buffer, recv *a.Expr, method t.ID, returnType *a.TypeExpr, args []*a.Node, sideEffectsOnly bool, depth uint32) error {
	switch id := recv.MType().QID()[1]; {
	case id == t.IDARMCRC32Utility, id == t.IDARMCRC32U32:
		return g.writeBuiltinCPUArchARMCRC32(b, recv, method, args, sideEffectsOnly, depth)
	case id.IsBuiltInCPUArchARMNeon():
		return g.writeBuiltinCPUArchARMNeon(b, recv, method, args, sideEffectsOnly, depth)
	case id == t.IDX86SSE42Utility, id == t.IDX86M128I,
		id == t.IDX86AVX2Utility, id == t.IDX86M256I:
		return g.writeBuiltinCPUArchX86(b, recv, method, returnType, args, sideEffectsOnly, depth)
	}
	return fmt.Errorf("internal error: unsupported cpu_arch method %s.%s",
		recv.MType().Str(g.tm), method.Str(g.tm))
}

func (g *gen) writeBuiltinCPUArchARMCRC32(b *buffer, recv *a.Expr, method t.ID, args []*a.Node, sideEffectsOnly bool, depth uint32) error {
	methodStr := method.Str(g.tm)
	if methodStr == "make_u32" {
		return g.writeExpr(b, args[0].AsArg().Value(), false, depth)
	} else if methodStr == "value" {
		return g.writeExpr(b, recv, false, depth)
	}

	b.writes("__")
	b.writes(methodStr)
	b.writes("(")
	if err := g.writeExpr(b, recv, false, depth); err != nil {
		return err
	}
	for _, o := range args {
		b.writes(", ")
		if err := g.writeExpr(b, o.AsArg().Value(), false, depth); err != nil {
			return err
		}
	}
	b.writes(")")
	return nil
}

func (g *gen) writeBuiltinCPUArchARMNeon(b *buffer, recv *a.Expr, method t.ID, args []*a.Node, sideEffectsOnly bool, depth uint32) error {
	methodStr := method.Str(g.tm)
	if strings.HasPrefix(methodStr, "make_") {
		before, after, ptr := "", ")", false
		if strings.HasSuffix(methodStr, "_multiple") {
			after = "})"
			switch methodStr {
			case "make_u8x8_multiple":
				before = "((uint8x8_t){"
			case "make_u16x4_multiple":
				before = "((uint16x4_t){"
			case "make_u32x2_multiple":
				before = "((uint32x2_t){"
			case "make_u64x1_multiple":
				before = "((uint64x1_t){"
			case "make_u8x16_multiple":
				before = "((uint8x16_t){"
			case "make_u16x8_multiple":
				before = "((uint16x8_t){"
			case "make_u32x4_multiple":
				before = "((uint32x4_t){"
			case "make_u64x2_multiple":
				before = "((uint64x2_t){"
			}
		} else {
			switch methodStr {
			case "make_u8x8_repeat":
				before = "vdup_n_u8("
			case "make_u16x4_repeat":
				before = "vdup_n_u16("
			case "make_u32x2_repeat":
				before = "vdup_n_u32("
			case "make_u64x1_repeat":
				before = "vdup_n_u64("
			case "make_u8x16_repeat":
				before = "vdupq_n_u8("
			case "make_u16x8_repeat":
				before = "vdupq_n_u16("
			case "make_u32x4_repeat":
				before = "vdupq_n_u32("
			case "make_u64x2_repeat":
				before = "vdupq_n_u64("
			case "make_u8x8_slice64":
				before, ptr = "vld1_u8(", true
			case "make_u8x16_slice128":
				before, ptr = "vld1q_u8(", true
			default:
				return fmt.Errorf("internal error: unsupported cpu_arch method %q", methodStr)
			}
		}
		b.writes(before)
		for i, o := range args {
			if i > 0 {
				b.writes(", ")
			}
			if ptr {
				if err := g.writeExprDotPtr(b, o.AsArg().Value(), false, depth); err != nil {
					return err
				}
			} else {
				if err := g.writeExpr(b, o.AsArg().Value(), false, depth); err != nil {
					return err
				}
			}
		}
		b.writes(after)
		return nil

	} else if strings.HasPrefix(methodStr, "as_") {
		switch recv.MType().QID()[1] {
		case t.IDARMNeonU8x8:
			switch methodStr {
			case "as_u16x4":
				methodStr = "vreinterpret_u16_u8"
			case "as_u32x2":
				methodStr = "vreinterpret_u32_u8"
			case "as_u64x1":
				methodStr = "vreinterpret_u64_u8"
			}
		case t.IDARMNeonU16x4:
			methodStr = "vreinterpret_u8_u16"
		case t.IDARMNeonU32x2:
			methodStr = "vreinterpret_u8_u32"
		case t.IDARMNeonU64x1:
			methodStr = "vreinterpret_u8_u64"
		case t.IDARMNeonU8x16:
			switch methodStr {
			case "as_u16x8":
				methodStr = "vreinterpretq_u16_u8"
			case "as_u32x4":
				methodStr = "vreinterpretq_u32_u8"
			case "as_u64x2":
				methodStr = "vreinterpretq_u64_u8"
			}
		case t.IDARMNeonU16x8:
			methodStr = "vreinterpretq_u8_u16"
		case t.IDARMNeonU32x4:
			methodStr = "vreinterpretq_u8_u32"
		case t.IDARMNeonU64x2:
			methodStr = "vreinterpretq_u8_u64"
		}
	}

	b.writes(methodStr)
	b.writes("(")
	if err := g.writeExpr(b, recv, false, depth); err != nil {
		return err
	}
	for _, o := range args {
		b.writes(", ")
		if err := g.writeExpr(b, o.AsArg().Value(), false, depth); err != nil {
			return err
		}
	}
	b.writes(")")
	return nil
}

func (g *gen) writeBuiltinCPUArchX86(b *buffer, recv *a.Expr, method t.ID, returnType *a.TypeExpr, args []*a.Node, sideEffectsOnly bool, depth uint32) error {
	methodStr := method.Str(g.tm)
	if strings.HasPrefix(methodStr, "make_") {
		fName, tName, ptr := "", "", false
		switch methodStr {
		case "make_u64_slice_u16lex4":
			fName, tName, ptr = "wuffs_base__peek_u64le__no_bounds_check", "const uint8_t*)(const void*", true

		case "make_m128i_multiple_u8":
			fName, tName = "_mm_set_epi8", "int8_t"
		case "make_m128i_multiple_u16":
			fName, tName = "_mm_set_epi16", "int16_t"
		case "make_m128i_multiple_u32":
			fName, tName = "_mm_set_epi32", "int32_t"
		case "make_m128i_multiple_u64":
			fName, tName = "_mm_set_epi64x", "int64_t"
		case "make_m128i_repeat_u8":
			fName, tName = "_mm_set1_epi8", "int8_t"
		case "make_m128i_repeat_u16":
			fName, tName = "_mm_set1_epi16", "int16_t"
		case "make_m128i_repeat_u32":
			fName, tName = "_mm_set1_epi32", "int32_t"
		case "make_m128i_repeat_u64":
			fName, tName = "_mm_set1_epi64x", "int64_t"
		case "make_m128i_single_u32":
			fName, tName = "_mm_cvtsi32_si128", "int32_t"
		case "make_m128i_single_u64":
			fName, tName = "_mm_cvtsi64_si128", "int64_t"
		case "make_m128i_slice128", "make_m128i_slice_u16lex8":
			fName, tName, ptr = "_mm_lddqu_si128", "const __m128i*)(const void*", true
		case "make_m128i_zeroes":
			fName, tName = "_mm_setzero_si128", ""

		case "make_m256i_multiple_u8":
			fName, tName = "_mm256_set_epi8", "int8_t"
		case "make_m256i_multiple_u16":
			fName, tName = "_mm256_set_epi16", "int16_t"
		case "make_m256i_multiple_u32":
			fName, tName = "_mm256_set_epi32", "int32_t"
		case "make_m256i_multiple_u64":
			fName, tName = "_mm256_set_epi64x", "int64_t"
		case "make_m256i_repeat_u8":
			fName, tName = "_mm256_set1_epi8", "int8_t"
		case "make_m256i_repeat_u16":
			fName, tName = "_mm256_set1_epi16", "int16_t"
		case "make_m256i_repeat_u32":
			fName, tName = "_mm256_set1_epi32", "int32_t"
		case "make_m256i_repeat_u64":
			fName, tName = "_mm256_set1_epi64x", "int64_t"
		case "make_m256i_slice256", "make_m256i_slice_u16lex16":
			fName, tName, ptr = "_mm256_lddqu_si256", "const __m256i*)(const void*", true
		case "make_m256i_zeroes":
			fName, tName = "_mm256_setzero_si256", ""
		default:
			return fmt.Errorf("internal error: unsupported cpu_arch method %q", methodStr)
		}
		b.printf("%s(", fName)
		for i := range args {
			// Iterate backwards to match _mm_setetc arg order.
			o := args[len(args)-1-i].AsArg()

			if i > 0 {
				b.writes(", ")
			}
			b.printf("(%s)(", tName)
			if ptr {
				if err := g.writeExprDotPtr(b, o.Value(), false, depth); err != nil {
					return err
				}
			} else {
				if err := g.writeExpr(b, o.Value(), false, depth); err != nil {
					return err
				}
			}
			b.writes(")")
		}
		b.writes(")")
		return nil

	} else if strings.HasPrefix(methodStr, "store_") {
		if !sideEffectsOnly {
			// Generate a two part expression using the comma operator: "(etc,
			// return_empty_struct call)". The final part is a function call
			// (to a static inline function) instead of a struct literal, to
			// avoid a "expression result unused" compiler error.
			b.writes("(")
		}
		prefix, cast := "", ""
		switch recv.MType().QID() {
		case [2]t.ID{t.IDBase, t.IDX86M128I}:
			switch methodStr {
			case "store_slice64":
				prefix = "_mm_storeu_si64((void*)("
			case "store_slice128":
				prefix = "_mm_storeu_si128((__m128i*)(void*)("
			}
		case [2]t.ID{t.IDBase, t.IDX86M256I}:
			switch methodStr {
			case "store_slice64":
				prefix = "_mm_storeu_si64((void*)("
				cast = "_mm256_castsi256_si128("
			case "store_slice128":
				prefix = "_mm_storeu_si128((__m128i*)(void*)("
				cast = "_mm256_castsi256_si128("
			case "store_slice256":
				prefix = "_mm256_storeu_si256((__m256i*)(void*)("
			}
		}
		if prefix == "" {
			return fmt.Errorf("internal error: unsupported cpu_arch method %q", methodStr)
		}
		b.writes(prefix)
		if err := g.writeExprDotPtr(b, args[0].AsArg().Value(), false, depth); err != nil {
			return err
		}
		b.writes("), ")
		b.writes(cast)
		if err := g.writeExpr(b, recv, false, depth); err != nil {
			return err
		}
		if cast != "" {
			b.writes(")")
		}
		b.writes(")")
		if !sideEffectsOnly {
			b.writes(", wuffs_base__make_empty_struct())")
		}
		return nil

	} else if strings.HasPrefix(methodStr, "truncate_u") {
		size := methodStr[len("truncate_u"):]
		b.printf("((uint%s_t)(_mm_cvtsi128_si%s(", size, size)
		if err := g.writeExpr(b, recv, false, depth); err != nil {
			return err
		}
		b.writes(")))")
		return nil

	}

	if returnType.IsNumType() {
		b.writes("((")
		b.writes(cTypeNames[returnType.QID()[1]])
		b.writes(")(")
	}

	b.writes(methodStr)
	b.writes("(")
	if err := g.writeExpr(b, recv, false, depth); err != nil {
		return err
	}
	for _, o := range args {
		b.writes(", ")
		argAfter := ""
		if o.AsArg().Value().MType().IsNumTypeOrIdeal() {
			b.writes("(int32_t)(")
			argAfter = ")"
		}
		if err := g.writeExpr(b, o.AsArg().Value(), false, depth); err != nil {
			return err
		}
		b.writes(argAfter)
	}
	b.writes(")")

	if returnType.IsNumType() {
		b.writes("))")
	}
	return nil
}

func (g *gen) writeExprDotPtr(b *buffer, n *a.Expr, sideEffectsOnly bool, depth uint32) error {
	if arrayOrSlice, lo, _, ok := n.IsSlice(); ok {
		if err := g.writeExpr(b, arrayOrSlice, sideEffectsOnly, depth); err != nil {
			return err
		}
		if arrayOrSlice.MType().IsEitherSliceType() {
			b.writes(".ptr")
		}
		if lo != nil {
			b.writes(" + ")
			if err := g.writeExpr(b, lo, sideEffectsOnly, depth); err != nil {
				return err
			}
		}
		return nil
	}

	if err := g.writeExpr(b, n, sideEffectsOnly, depth); err != nil {
		return err
	}
	if n.MType().IsEitherSliceType() {
		b.writes(".ptr")
	}
	return nil
}

func (g *gen) writeBuiltinNumType(b *buffer, recv *a.Expr, method t.ID, args []*a.Node, depth uint32) error {
	switch method {
	case t.IDLowBits:
		// "recv.low_bits(n:etc)" in C is one of:
		//  - "((recv) & constant)"
		//  - "((recv) & WUFFS_PRIVATE_IMPL__LOW_BITS_MASK__UXX(n))"
		b.writes("((")
		if err := g.writeExpr(b, recv, false, depth); err != nil {
			return err
		}
		b.writes(") & ")

		if cv := args[0].AsArg().Value().ConstValue(); cv != nil && cv.Sign() >= 0 && cv.Cmp(sixtyFour) <= 0 {
			mask := big.NewInt(0)
			mask.Lsh(one, uint(cv.Uint64()))
			mask.Sub(mask, one)
			b.printf("0x%su", strings.ToUpper(mask.Text(16)))
		} else {
			if sz, err := g.sizeof(recv.MType()); err != nil {
				return err
			} else {
				b.printf("WUFFS_PRIVATE_IMPL__LOW_BITS_MASK__U%d(", 8*sz)
			}
			if err := g.writeExpr(b, args[0].AsArg().Value(), false, depth); err != nil {
				return err
			}
			b.writes(")")
		}

		b.writes(")")
		return nil

	case t.IDHighBits:
		// "recv.high_bits(n:etc)" in C is "((recv) >> (8*sizeof(recv) - (n)))".
		b.writes("((")
		if err := g.writeExpr(b, recv, false, depth); err != nil {
			return err
		}
		b.writes(") >> (")
		if sz, err := g.sizeof(recv.MType()); err != nil {
			return err
		} else {
			b.printf("%du", 8*sz)
		}
		b.writes(" - ")
		if err := g.writeExpr(b, args[0].AsArg().Value(), false, depth); err != nil {
			return err
		}
		b.writes("))")
		return nil

	case t.IDMax:
		b.writes("wuffs_base__u")
		if sz, err := g.sizeof(recv.MType()); err != nil {
			return err
		} else {
			b.printf("%d", 8*sz)
		}
		b.writes("__max(")
		if err := g.writeExpr(b, recv, false, depth); err != nil {
			return err
		}
		b.writes(", ")
		if err := g.writeExpr(b, args[0].AsArg().Value(), false, depth); err != nil {
			return err
		}
		b.writes(")")
		return nil

	case t.IDMin:
		b.writes("wuffs_base__u")
		if sz, err := g.sizeof(recv.MType()); err != nil {
			return err
		} else {
			b.printf("%d", 8*sz)
		}
		b.writes("__min(")
		if err := g.writeExpr(b, recv, false, depth); err != nil {
			return err
		}
		b.writes(", ")
		if err := g.writeExpr(b, args[0].AsArg().Value(), false, depth); err != nil {
			return err
		}
		b.writes(")")
		return nil
	}
	return errNoSuchBuiltin
}

func (g *gen) writeBuiltinSlice(b *buffer, recv *a.Expr, method t.ID, args []*a.Node, sideEffectsOnly bool, depth uint32) error {
	switch method {
	case t.IDBulkLoadHostEndian:
		b.writes("wuffs_private_impl__bulk_load_host_endian(")
		if err := g.writeBuiltinSliceBulkMethodRecv(b, recv, depth); err != nil {
			return err
		}
		return g.writeArgs(b, args, depth)

	case t.IDBulkMemset:
		b.writes("wuffs_private_impl__bulk_memset(")
		if err := g.writeBuiltinSliceBulkMethodRecv(b, recv, depth); err != nil {
			return err
		}
		return g.writeArgs(b, args, depth)

	case t.IDBulkSaveHostEndian:
		b.writes("wuffs_private_impl__bulk_save_host_endian(")
		if err := g.writeBuiltinSliceBulkMethodRecv(b, recv, depth); err != nil {
			return err
		}
		return g.writeArgs(b, args, depth)

	case t.IDCopyFromSlice:
		if err := g.writeBuiltinSliceCopyFromSlice8(b, recv, method, args, depth); err != errOptimizationNotApplicable {
			return err
		}

		// TODO: don't assume that the slice is a slice of base.u8.
		b.writes("wuffs_private_impl__slice_u8__copy_from_slice(")
		if err := g.writeExpr(b, recv, false, depth); err != nil {
			return err
		}
		b.writes(", ")
		return g.writeArgs(b, args, depth)

	case t.IDLength:
		b.writes("((uint64_t)(")
		if err := g.writeExpr(b, recv, false, depth); err != nil {
			return err
		}
		b.writes(".len))")
		return nil

	case t.IDUintptrLow12Bits:
		b.writes("((uint32_t)(0xFFFu & (uintptr_t)(")
		if err := g.writeExpr(b, recv, false, depth); err != nil {
			return err
		}
		b.writes(".ptr)))")
		return nil

	case t.IDSuffix:
		// TODO: don't assume that the slice is a slice of base.u8.
		b.writes("wuffs_private_impl__slice_u8__suffix(")
		if err := g.writeExpr(b, recv, false, depth); err != nil {
			return err
		}
		b.writes(", ")
		return g.writeArgs(b, args, depth)
	}

	if (t.IDPeekU8 <= method) && (method <= t.IDPeekU64LE) {
		s := method.Str(g.tm)
		if i := strings.Index(s, "_as_"); i >= 0 {
			s = s[:i]
		}
		b.printf("wuffs_base__%s__no_bounds_check(", s)
		if err := g.writeExpr(b, recv, false, depth); err != nil {
			return err
		}
		b.writes(".ptr)")
		return nil
	}

	if (t.IDPokeU8 <= method) && (method <= t.IDPokeU64LE) {
		if !sideEffectsOnly {
			// Generate a two part expression using the comma operator: "(etc,
			// return_empty_struct call)". The final part is a function call
			// (to a static inline function) instead of a struct literal, to
			// avoid a "expression result unused" compiler error.
			b.writes("(")
		}
		b.printf("wuffs_base__%s__no_bounds_check(", method.Str(g.tm))
		if err := g.writeExpr(b, recv, false, depth); err != nil {
			return err
		}
		b.writes(".ptr, ")
		if err := g.writeExpr(b, args[0].AsArg().Value(), false, depth); err != nil {
			return err
		}
		b.writes(")")
		if !sideEffectsOnly {
			b.writes(", wuffs_base__make_empty_struct())")
		}
		return nil
	}

	return errNoSuchBuiltin
}

func (g *gen) writeBuiltinSliceBulkMethodRecv(b *buffer, recv *a.Expr, depth uint32) error {
	if arrayOrSlice, lo, hi, ok := recv.IsSlice(); !ok || (hi == nil) {
		// No-op.

	} else if arrayOrSlice.MType().IsBulkNumType() {
		b.writes("&")
		if err := g.writeExpr(b, arrayOrSlice, false, depth); err != nil {
			return err
		}
		if lo == nil {
			b.writes("[0], ")
			if err := g.writeExpr(b, hi, false, depth); err != nil {
				return err
			}
		} else {
			b.writes("[")
			if err := g.writeExpr(b, lo, false, depth); err != nil {
				return err
			}
			b.writes("], ")
			if err := g.writeExprXMinusY(b, hi, lo, depth); err != nil {
				return err
			}
		}
		if bs := arrayOrSlice.MType().Inner().BulkSize(); (bs != nil) && (bs.Cmp(one) != 0) {
			b.writes(" * (size_t)")
			b.writes(bs.String())
			b.writes("u")
		}
		b.writes(", ")
		return nil

	} else if recv.MType().IsContainerOfSpecificNumType(t.IDSlice, t.IDU8) {
		if arrayOrSlice.MType().IsContainerOfSpecificNumType(t.IDSlice, t.IDU8) {
			if err := g.writeExpr(b, arrayOrSlice, false, depth); err != nil {
				return err
			}
			b.writes(".ptr")
			if mhs := recv.MHS(); mhs != nil {
				b.writes(" + ")
				if err := g.writeExpr(b, mhs.AsExpr(), false, depth); err != nil {
					return err
				}
			}
			b.writes(", ")
		} else {
			if err := g.writeExpr(b, recv, false, depth); err != nil {
				return err
			}
			b.writes(".ptr, ")
		}

		if mhs, rhs := recv.MHS(), recv.RHS(); (mhs != nil) && (rhs != nil) {
			if err := g.writeExprXMinusY(b, rhs.AsExpr(), mhs.AsExpr(), depth); err != nil {
				return err
			}
		} else if rhs := recv.RHS(); rhs != nil {
			if err := g.writeExpr(b, rhs.AsExpr(), false, depth); err != nil {
				return err
			}
		} else {
			if err := g.writeExpr(b, recv, false, depth); err != nil {
				return err
			}
			b.writes(".len")
		}
		b.writes(", ")
		return nil
	}

	return fmt.Errorf("TODO: bulk_memset for general expressions")
}

// writeBuiltinSliceCopyFromSlice8 writes an optimized version of:
//
// foo[fIndex .. fIndex + 8].copy_from_slice!(s:bar[bIndex .. bIndex + 8])
func (g *gen) writeBuiltinSliceCopyFromSlice8(b *buffer, recv *a.Expr, method t.ID, args []*a.Node, depth uint32) error {
	if method != t.IDCopyFromSlice || len(args) != 1 {
		return errOptimizationNotApplicable
	}
	foo, fIndex := matchFooIndexIndexPlus8(recv)
	bar, bIndex := matchFooIndexIndexPlus8(args[0].AsArg().Value())
	if foo == nil || bar == nil {
		return errOptimizationNotApplicable
	}
	b.writes("memcpy((")
	if err := g.writeExpr(b, foo, false, depth); err != nil {
		return err
	}
	if foo.MType().IsEitherSliceType() {
		b.writes(".ptr")
	}
	if fIndex != nil {
		b.writes(")+(")
		if err := g.writeExpr(b, fIndex, false, depth); err != nil {
			return err
		}
	}
	b.writes("), (")
	if err := g.writeExpr(b, bar, false, depth); err != nil {
		return err
	}
	if bar.MType().IsEitherSliceType() {
		b.writes(".ptr")
	}
	if bIndex != nil {
		b.writes(")+(")
		if err := g.writeExpr(b, bIndex, false, depth); err != nil {
			return err
		}
	}
	// TODO: don't assume that the slice is a slice of base.u8.
	b.writes("), 8u)")
	return nil
}

// matchFooIndexIndexPlus8 matches n with "foo[index .. index + 8]" or "foo[..
// 8]". It returns a nil foo if there isn't a match.
func matchFooIndexIndexPlus8(n *a.Expr) (foo *a.Expr, index *a.Expr) {
	foo, index, rhs, ok := n.IsSlice()
	if !ok || (rhs == nil) {
		return nil, nil
	}

	if index == nil {
		// No-op.
	} else if rhs.Operator() != t.IDXBinaryPlus || !rhs.LHS().AsExpr().Eq(index) {
		return nil, nil
	} else {
		rhs = rhs.RHS().AsExpr()
	}

	if cv := rhs.ConstValue(); cv == nil || cv.Cmp(eight) != 0 {
		return nil, nil
	}
	return foo, index
}

func (g *gen) writeBuiltinTable(b *buffer, recv *a.Expr, method t.ID, args []*a.Node, sideEffectsOnly bool, depth uint32) error {
	field := ""

	switch method {
	case t.IDHeight:
		field = "height"
	case t.IDStride:
		field = "stride"
	case t.IDWidth:
		field = "width"

	case t.IDRowU32:
		// TODO: don't assume that the table is a table of base.u8.
		b.writes("wuffs_private_impl__table_u8__row_u32(")
		if err := g.writeExpr(b, recv, false, depth); err != nil {
			return err
		}
		b.writes(", ")
		return g.writeArgs(b, args, depth)

	case t.IDSubtable:
		// TODO: don't assume that the table is a table of base.u8.
		b.writes("wuffs_base__table_u8__subtable_ij(")
		if err := g.writeExpr(b, recv, false, depth); err != nil {
			return err
		}
		b.writes(", ")
		return g.writeArgs(b, args, depth)
	}

	if field != "" {
		b.writes("((uint64_t)(")
		if err := g.writeExpr(b, recv, false, depth); err != nil {
			return err
		}
		b.printf(".%s))", field)
		return nil
	}

	return errNoSuchBuiltin
}

func (g *gen) writeArgs(b *buffer, args []*a.Node, depth uint32) error {
	if len(args) >= 4 {
		b.writeb('\n')
	}
	for i, o := range args {
		if i > 0 {
			if len(args) >= 4 {
				b.writes(",\n")
			} else {
				b.writes(", ")
			}
		}
		if err := g.writeExpr(b, o.AsArg().Value(), false, depth); err != nil {
			return err
		}
	}
	b.writes(")")
	return nil
}

func (g *gen) writeBuiltinQuestionCall(b *buffer, n *a.Expr, depth uint32) error {
	// TODO: also handle (or reject??) being on the RHS of an =? operator.
	if (n.Operator() != a.ExprOperatorCall) || (!n.Effect().Coroutine()) {
		return errNoSuchBuiltin
	}
	method := n.LHS().AsExpr()
	recv := method.LHS().AsExpr()
	recvTyp := recv.MType()
	if !recvTyp.IsIOTokenType() {
		return errNoSuchBuiltin
	}
	recvName, err := g.recvName(recv)
	if err != nil {
		return err
	}

	switch recvTyp.QID()[1] {
	case t.IDIOReader:
		switch method.Ident() {
		case t.IDReadU8, t.IDReadU8AsU16, t.IDReadU8AsU32, t.IDReadU8AsU64:
			if err := g.writeCoroSuspPoint(b, false); err != nil {
				return err
			}
			if g.currFunk.tempW > maxTemp {
				return fmt.Errorf("too many temporary variables required")
			}
			temp := g.currFunk.tempW
			g.currFunk.tempW++

			b.printf("if (WUFFS_BASE__UNLIKELY(iop_%s == io2_%s)) {\n"+
				"status = wuffs_base__make_status(wuffs_base__suspension__short_read);\n"+
				"goto suspend;\n}\n",
				recvName, recvName)

			// TODO: watch for passing an array type to writeCTypeName? In C, an
			// array type can decay into a pointer.
			if err := g.writeCTypeName(b, n.MType(), tPrefix, fmt.Sprint(temp)); err != nil {
				return err
			}
			b.printf(" = *iop_%s++;\n", recvName)
			return nil

		case t.IDSkip, t.IDSkipU32:
			x := n.Args()[0].AsArg().Value()
			if cv := x.ConstValue(); cv != nil && cv.Cmp(one) == 0 {
				if err := g.writeCoroSuspPoint(b, false); err != nil {
					return err
				}
				b.printf("if (WUFFS_BASE__UNLIKELY(iop_%s == io2_%s)) {\n"+
					"status = wuffs_base__make_status(wuffs_base__suspension__short_read);\n"+
					"goto suspend;\n}\n",
					recvName, recvName)
				b.printf("iop_%s++;\n", recvName)
				return nil
			}

			g.currFunk.usesScratch = true
			scratchName := fmt.Sprintf("self->private_data.%s%s.scratch",
				sPrefix, g.currFunk.astFunc.FuncName().Str(g.tm))

			b.printf("%s = ", scratchName)
			if err := g.writeExpr(b, x, false, depth); err != nil {
				return err
			}
			b.writes(";\n")

			if err := g.writeCoroSuspPoint(b, false); err != nil {
				return err
			}

			b.printf("if (%s > ((uint64_t)(io2_%s - iop_%s))) {\n", scratchName, recvName, recvName)
			b.printf("%s -= ((uint64_t)(io2_%s - iop_%s));\n", scratchName, recvName, recvName)
			b.printf("iop_%s = io2_%s;\n", recvName, recvName)

			b.writes("status = wuffs_base__make_status(wuffs_base__suspension__short_read);\ngoto suspend;\n}\n")
			b.printf("iop_%s += %s;\n", recvName, scratchName)
			return nil
		}

		if method.Ident() >= readMethodsBase {
			if m := method.Ident() - readMethodsBase; m < t.ID(len(readMethods)) {
				if p := readMethods[m]; p.n != 0 {
					if err := g.writeCoroSuspPoint(b, false); err != nil {
						return err
					}
					return g.writeReadUxxAsUyy(b, n, recvName, p.n, p.size, p.endianness)
				}
			}
		}

	case t.IDIOWriter:
		switch method.Ident() {
		case t.IDWriteU8:
			g.currFunk.usesScratch = true
			scratchName := fmt.Sprintf("self->private_data.%s%s.scratch",
				sPrefix, g.currFunk.astFunc.FuncName().Str(g.tm))

			b.printf("%s = ", scratchName)
			x := n.Args()[0].AsArg().Value()
			if err := g.writeExpr(b, x, false, depth); err != nil {
				return err
			}
			b.writes(";\n")

			if err := g.writeCoroSuspPoint(b, false); err != nil {
				return err
			}
			b.printf("if (iop_%s == io2_%s) {\n"+
				"status = wuffs_base__make_status(wuffs_base__suspension__short_write);\n"+
				"goto suspend;\n}\n"+
				"*iop_%s++ = ((uint8_t)(%s));\n",
				recvName, recvName, recvName, scratchName)
			return nil
		}

	}
	return errNoSuchBuiltin
}

func (g *gen) writeReadUxxAsUyy(b *buffer, n *a.Expr, preName string, xx uint8, yy uint8, endianness uint8) error {
	if (xx&7 != 0) || (xx < 16) || (xx > 64) {
		return fmt.Errorf("internal error: bad writeReadUXX size %d", xx)
	}
	if endianness != 'b' && endianness != 'l' {
		return fmt.Errorf("internal error: bad writeReadUXX endianness %q", endianness)
	}

	if g.currFunk.tempW > maxTemp {
		return fmt.Errorf("too many temporary variables required")
	}
	temp := g.currFunk.tempW
	g.currFunk.tempW++

	if err := g.writeCTypeName(b, n.MType(), tPrefix, fmt.Sprint(temp)); err != nil {
		return err
	}
	b.writes(";\n")

	method := n.LHS().AsExpr()
	recv := method.LHS().AsExpr()
	recvName, err := g.recvName(recv)
	if err != nil {
		return err
	}

	g.currFunk.usesScratch = true
	scratchName := fmt.Sprintf("self->private_data.%s%s.scratch",
		sPrefix, g.currFunk.astFunc.FuncName().Str(g.tm))

	b.printf("if (WUFFS_BASE__LIKELY(io2_%s - iop_%s >= %d)) {\n", recvName, recvName, xx/8)
	b.printf("%s%d = ", tPrefix, temp)
	if xx != yy {
		b.printf("((uint%d_t)(", yy)
	}
	b.printf("wuffs_base__peek_u%d%ce__no_bounds_check(iop_%s)", xx, endianness, recvName)
	if xx != yy {
		b.writes("))")
	}
	b.printf(";\niop_%s += %d;\n", recvName, xx/8)
	b.printf("} else {\n")

	b.printf("%s = 0;\n", scratchName)
	if err := g.writeCoroSuspPoint(b, false); err != nil {
		return err
	}
	b.printf("while (true) {\n")

	b.printf("if (WUFFS_BASE__UNLIKELY(iop_%s == io2_%s)) {\n"+
		"status = wuffs_base__make_status(wuffs_base__suspension__short_read);\ngoto suspend;\n}\n",
		preName, preName)

	b.printf("uint64_t* scratch = &%s;\n", scratchName)
	b.printf("uint32_t num_bits_%d = ((uint32_t)(*scratch", temp)
	switch endianness {
	case 'b':
		b.writes(" & 0xFFu));\n*scratch >>= 8;\n*scratch <<= 8;\n")
		b.printf("*scratch |= ((uint64_t)(*%s%s++)) << (56 - num_bits_%d);\n",
			iopPrefix, preName, temp)
	case 'l':
		b.writes(" >> 56));\n*scratch <<= 8;\n*scratch >>= 8;\n")
		b.printf("*scratch |= ((uint64_t)(*%s%s++)) << num_bits_%d;\n",
			iopPrefix, preName, temp)
	}

	b.printf("if (num_bits_%d == %d) {\n%s%d = ((uint%d_t)(", temp, xx-8, tPrefix, temp, yy)
	switch endianness {
	case 'b':
		b.printf("*scratch >> %d));\n", 64-xx)
	case 'l':
		b.printf("*scratch));\n")
	}
	b.printf("break;\n")
	b.printf("}\n")

	b.printf("num_bits_%d += 8u;\n", temp)
	switch endianness {
	case 'b':
		b.printf("*scratch |= ((uint64_t)(num_bits_%d));\n", temp)
	case 'l':
		b.printf("*scratch |= ((uint64_t)(num_bits_%d)) << 56;\n", temp)
	}

	b.writes("}\n}\n")
	return nil
}

const readMethodsBase = t.IDReadU8

var readMethods = [...]struct {
	size       uint8
	n          uint8
	endianness uint8
}{
	t.IDReadU8 - readMethodsBase: {8, 8, 'b'},

	t.IDReadU8AsU16 - readMethodsBase: {16, 8, 'b'},
	t.IDReadU16BE - readMethodsBase:   {16, 16, 'b'},
	t.IDReadU16LE - readMethodsBase:   {16, 16, 'l'},

	t.IDReadU8AsU32 - readMethodsBase:    {32, 8, 'b'},
	t.IDReadU16BEAsU32 - readMethodsBase: {32, 16, 'b'},
	t.IDReadU16LEAsU32 - readMethodsBase: {32, 16, 'l'},
	t.IDReadU24BEAsU32 - readMethodsBase: {32, 24, 'b'},
	t.IDReadU24LEAsU32 - readMethodsBase: {32, 24, 'l'},
	t.IDReadU32BE - readMethodsBase:      {32, 32, 'b'},
	t.IDReadU32LE - readMethodsBase:      {32, 32, 'l'},

	t.IDReadU8AsU64 - readMethodsBase:    {64, 8, 'b'},
	t.IDReadU16BEAsU64 - readMethodsBase: {64, 16, 'b'},
	t.IDReadU16LEAsU64 - readMethodsBase: {64, 16, 'l'},
	t.IDReadU24BEAsU64 - readMethodsBase: {64, 24, 'b'},
	t.IDReadU24LEAsU64 - readMethodsBase: {64, 24, 'l'},
	t.IDReadU32BEAsU64 - readMethodsBase: {64, 32, 'b'},
	t.IDReadU32LEAsU64 - readMethodsBase: {64, 32, 'l'},
	t.IDReadU40BEAsU64 - readMethodsBase: {64, 40, 'b'},
	t.IDReadU40LEAsU64 - readMethodsBase: {64, 40, 'l'},
	t.IDReadU48BEAsU64 - readMethodsBase: {64, 48, 'b'},
	t.IDReadU48LEAsU64 - readMethodsBase: {64, 48, 'l'},
	t.IDReadU56BEAsU64 - readMethodsBase: {64, 56, 'b'},
	t.IDReadU56LEAsU64 - readMethodsBase: {64, 56, 'l'},
	t.IDReadU64BE - readMethodsBase:      {64, 64, 'b'},
	t.IDReadU64LE - readMethodsBase:      {64, 64, 'l'},
}

const peekMethodsBase = t.IDPeekU8

var peekMethods = [...]struct {
	size       uint8
	n          uint8
	endianness uint8
}{
	t.IDPeekU8 - peekMethodsBase: {8, 8, 'b'},

	t.IDPeekU8AsU16 - peekMethodsBase: {16, 8, 'b'},
	t.IDPeekU16BE - peekMethodsBase:   {16, 16, 'b'},
	t.IDPeekU16LE - peekMethodsBase:   {16, 16, 'l'},

	t.IDPeekU8AsU32 - peekMethodsBase:    {32, 8, 'b'},
	t.IDPeekU16BEAsU32 - peekMethodsBase: {32, 16, 'b'},
	t.IDPeekU16LEAsU32 - peekMethodsBase: {32, 16, 'l'},
	t.IDPeekU24BEAsU32 - peekMethodsBase: {32, 24, 'b'},
	t.IDPeekU24LEAsU32 - peekMethodsBase: {32, 24, 'l'},
	t.IDPeekU32BE - peekMethodsBase:      {32, 32, 'b'},
	t.IDPeekU32LE - peekMethodsBase:      {32, 32, 'l'},

	t.IDPeekU8AsU64 - peekMethodsBase:    {64, 8, 'b'},
	t.IDPeekU16BEAsU64 - peekMethodsBase: {64, 16, 'b'},
	t.IDPeekU16LEAsU64 - peekMethodsBase: {64, 16, 'l'},
	t.IDPeekU24BEAsU64 - peekMethodsBase: {64, 24, 'b'},
	t.IDPeekU24LEAsU64 - peekMethodsBase: {64, 24, 'l'},
	t.IDPeekU32BEAsU64 - peekMethodsBase: {64, 32, 'b'},
	t.IDPeekU32LEAsU64 - peekMethodsBase: {64, 32, 'l'},
	t.IDPeekU40BEAsU64 - peekMethodsBase: {64, 40, 'b'},
	t.IDPeekU40LEAsU64 - peekMethodsBase: {64, 40, 'l'},
	t.IDPeekU48BEAsU64 - peekMethodsBase: {64, 48, 'b'},
	t.IDPeekU48LEAsU64 - peekMethodsBase: {64, 48, 'l'},
	t.IDPeekU56BEAsU64 - peekMethodsBase: {64, 56, 'b'},
	t.IDPeekU56LEAsU64 - peekMethodsBase: {64, 56, 'l'},
	t.IDPeekU64BE - peekMethodsBase:      {64, 64, 'b'},
	t.IDPeekU64LE - peekMethodsBase:      {64, 64, 'l'},
}

const writeFastMethodsBase = t.IDWriteU8Fast

var writeFastMethods = [...]struct {
	n          uint8
	endianness uint8
}{
	t.IDWriteU8Fast - writeFastMethodsBase:    {8, 'b'},
	t.IDWriteU16BEFast - writeFastMethodsBase: {16, 'b'},
	t.IDWriteU16LEFast - writeFastMethodsBase: {16, 'l'},
	t.IDWriteU24BEFast - writeFastMethodsBase: {24, 'b'},
	t.IDWriteU24LEFast - writeFastMethodsBase: {24, 'l'},
	t.IDWriteU32BEFast - writeFastMethodsBase: {32, 'b'},
	t.IDWriteU32LEFast - writeFastMethodsBase: {32, 'l'},
	t.IDWriteU40BEFast - writeFastMethodsBase: {40, 'b'},
	t.IDWriteU40LEFast - writeFastMethodsBase: {40, 'l'},
	t.IDWriteU48BEFast - writeFastMethodsBase: {48, 'b'},
	t.IDWriteU48LEFast - writeFastMethodsBase: {48, 'l'},
	t.IDWriteU56BEFast - writeFastMethodsBase: {56, 'b'},
	t.IDWriteU56LEFast - writeFastMethodsBase: {56, 'l'},
	t.IDWriteU64BEFast - writeFastMethodsBase: {64, 'b'},
	t.IDWriteU64LEFast - writeFastMethodsBase: {64, 'l'},
}
