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
	"flag"
	"fmt"
	"math/big"
	"sort"
	"strings"

	"github.com/google/wuffs/lang/builtin"
	"github.com/google/wuffs/lang/generate"
	"github.com/google/wuffs/lib/dumbindent"

	cf "github.com/google/wuffs/cmd/commonflags"

	a "github.com/google/wuffs/lang/ast"
	t "github.com/google/wuffs/lang/token"
)

var (
	zero      = big.NewInt(0)
	one       = big.NewInt(1)
	eight     = big.NewInt(8)
	sixtyFour = big.NewInt(64)

	mibi = big.NewInt(1 << 20)

	maxUint8  = big.NewInt((1 << 8) - 1)
	maxUint16 = big.NewInt((1 << 16) - 1)
	maxUint32 = big.NewInt((1 << 32) - 1)

	typeExprARMCRC32U32   = a.NewTypeExpr(0, t.IDBase, t.IDARMCRC32U32, nil, nil, nil)
	typeExprPixelSwizzler = a.NewTypeExpr(0, t.IDBase, t.IDPixelSwizzler, nil, nil, nil)
)

// Prefixes are prepended to names to form a namespace and to avoid e.g.
// "double" being a valid Wuffs variable name but not a valid C one.
const (
	aPrefix = "a_" // Function argument.
	fPrefix = "f_" // Struct field.
	iPrefix = "i_" // Iterate variable.
	oPrefix = "o_" // Temporary IOManip variable.
	pPrefix = "p_" // Coroutine suspension point (program counter).
	sPrefix = "s_" // Coroutine stack (saved local variables).
	tPrefix = "t_" // Temporary local variable.
	uPrefix = "u_" // Derived from a local variable.
	vPrefix = "v_" // Local variable.
)

// I/O (reader/writer) prefixes. In the generated C code, the variables with
// these prefixes all have type uint8_t*. The iop_etc variables are the key
// ones. For an io_reader or io_writer function argument a_src or a_dst,
// reading or writing the next byte (and advancing the stream) is essentially
// "etc = *iop_a_src++" or "*io_a_dst++ = etc".
//
// The other two prefixes, giving names like io1_etc and io2_etc, are auxiliary
// pointers: lower and upper inclusive bounds. As an iop_etc pointer advances,
// it cannot advance past io2_etc. In the rarer case that an iop_etc pointer
// retreats, undoing a read or write, it cannot retreat past io1_etc. The
// io0_etc pointer is used to calculate the history_length and is the base that
// marks are relative to.
//
// The iop_etc pointer can change over the lifetime of a function. The ioN_etc
// pointers, for numeric N, cannot (except by IOManip blocks).
//
// At the start of a function, these pointers are initialized from an
// io_buffer's fields (ptr, ri, wi, len). For an io_reader:
//   - io0_etc = ptr
//   - io1_etc = ptr + ri
//   - iop_etc = ptr + ri
//   - io2_etc = ptr + wi
//
// and for an io_writer:
//   - io0_etc = ptr
//   - io1_etc = ptr + wi
//   - iop_etc = ptr + wi
//   - io2_etc = ptr + len
const (
	io0Prefix = "io0_" // Base.
	io1Prefix = "io1_" // Lower bound.
	io2Prefix = "io2_" // Upper bound.
	iopPrefix = "iop_" // Pointer.
)

// BaseSubModules is the list of lower-cased XXX's in the base module's
// WUFFS_CONFIG__MODULE__BASE__XXX sub-modules.
var BaseSubModules = []string{
	"core",
	"floatconv",
	"intconv",
	"interfaces",
	"magic",
	"pixconv",
	"utf8",
}

// Do transpiles a Wuffs program to a C program.
//
// The arguments list the source Wuffs files. If no arguments are given, it
// reads from stdin.
//
// The generated program is written to stdout.
func Do(args []string) error {
	flags := flag.FlagSet{}
	genlinenumFlag := flags.Bool("genlinenum", cf.GenlinenumDefault, cf.GenlinenumUsage)

	return generate.Do(&flags, args, func(pkgName string, tm *t.Map, files []*a.File) ([]byte, error) {
		unformatted := []byte(nil)
		if pkgName == "base" {
			if len(files) != 0 {
				return nil, fmt.Errorf("base package shouldn't have any .wuffs files")
			}
			buf := make(buffer, 0, 128*1024)
			if err := expandBangInsert(&buf, embedBaseAllImplC.Trim(), map[string]func(*buffer) error{
				"// ¡ INSERT InterfaceDeclarations.\n":             insertInterfaceDeclarations,
				"// ¡ INSERT InterfaceDefinitions.\n":              insertInterfaceDefinitions,
				"// ¡ INSERT base/all-private.h.\n":                insertBaseAllPrivateH,
				"// ¡ INSERT base/all-public.h.\n":                 insertBaseAllPublicH,
				"// ¡ INSERT base/copyright\n":                     insertBaseCopyright,
				"// ¡ INSERT base/floatconv-submodule.c.\n":        insertBaseFloatConvSubmoduleC,
				"// ¡ INSERT base/intconv-submodule.c.\n":          insertBaseIntConvSubmoduleC,
				"// ¡ INSERT base/magic-submodule.c.\n":            insertBaseMagicSubmoduleC,
				"// ¡ INSERT base/pixconv-submodule-regular.c.\n":  insertBasePixConvSubmoduleRegularC,
				"// ¡ INSERT base/pixconv-submodule-x86-avx2.c.\n": insertBasePixConvSubmoduleX86Avx2C,
				"// ¡ INSERT base/pixconv-submodule-ycck.c.\n":     insertBasePixConvSubmoduleYcckC,
				"// ¡ INSERT base/utf8-submodule.c.\n":             insertBaseUTF8SubmoduleC,
				"// ¡ INSERT vtable names.\n": func(b *buffer) error {
					for _, n := range builtin.Interfaces {
						buf.printf("const char wuffs_base__%s__vtable_name[] = "+
							"\"{vtable}wuffs_base__%s\";\n", n, n)
					}
					return nil
				},
				"// ¡ INSERT wuffs_base__status strings.\n": func(b *buffer) error {
					for _, z := range builtin.Statuses {
						msg, _ := t.Unescape(z)
						if msg == "" {
							continue
						}
						pre := "note"
						if msg[0] == '$' {
							pre = "suspension"
						} else if msg[0] == '#' {
							pre = "error"
						}
						b.printf("const char wuffs_base__%s__%s[] = \"%sbase: %s\";\n",
							pre, cName(msg, ""), msg[:1], msg[1:])
					}
					return nil
				},
			}); err != nil {
				return nil, err
			}
			unformatted = []byte(buf)

		} else {
			g := &gen{
				PKGPREFIX:  "WUFFS_" + strings.ToUpper(pkgName) + "__",
				PKGNAME:    strings.ToUpper(pkgName),
				pkgPrefix:  "wuffs_" + pkgName + "__",
				pkgName:    pkgName,
				tm:         tm,
				files:      files,
				genlinenum: *genlinenumFlag,
			}
			var err error
			unformatted, err = g.generate()
			if err != nil {
				return nil, err
			}
		}

		// The base package is largely hand-written C, not transpiled from
		// Wuffs, and that part is presumably already formatted. The rest is
		// generated by this package. We take care here to print well indented
		// C code, so further C formatting is unnecessary.
		if pkgName == "base" {
			return unformatted, nil
		}

		return dumbindent.FormatBytes(nil, unformatted, nil), nil
	})
}

type visibility uint32

const (
	bothPubPri = visibility(iota)
	pubOnly
	priOnly
)

const (
	maxIOManips = 100
	maxTemp     = 10000
)

type status struct {
	cName       string
	msg         string
	fromThisPkg bool
	public      bool
}

func statusMsgIsError(msg string) bool {
	return (len(msg) != 0) && (msg[0] == '#')
}

func statusMsgIsNote(msg string) bool {
	return (len(msg) == 0) || (msg[0] != '$' && msg[0] != '#')
}

func statusMsgIsSuspension(msg string) bool {
	return (len(msg) != 0) && (msg[0] == '$')
}

type buffer []byte

func (b *buffer) Write(p []byte) (int, error) {
	*b = append(*b, p...)
	return len(p), nil
}

func (b *buffer) printf(format string, args ...interface{}) { fmt.Fprintf(b, format, args...) }
func (b *buffer) writeb(x byte)                             { *b = append(*b, x) }
func (b *buffer) writes(s string)                           { *b = append(*b, s...) }
func (b *buffer) writex(s []byte)                           { *b = append(*b, s...) }

func (b *buffer) undoWrites(s string) bool {
	nb := len(*b)
	ns := len(s)
	if (nb < ns) || (string((*b)[nb-ns:]) != s) {
		return false
	}
	*b = (*b)[:nb-ns]
	return true
}

func expandBangInsert(b *buffer, s string, m map[string]func(*buffer) error) error {
	for {
		remaining := ""
		if i := strings.IndexByte(s, '\n'); i >= 0 {
			s, remaining = s[:i+1], s[i+1:]
		}

		const prefix = "// ¡ INSERT "
		if !strings.HasPrefix(s, prefix) {
			b.writes(s)
		} else if f := m[s]; f == nil {
			msg := []byte(fmt.Sprintf("unrecognized line %q, want one of:\n", s))
			keys := []string(nil)
			for k := range m {
				keys = append(keys, k)
			}
			sort.Strings(keys)
			for _, k := range keys {
				msg = append(msg, '\t')
				msg = append(msg, k...)
			}
			return errors.New(string(msg))
		} else if err := f(b); err != nil {
			return err
		}

		if remaining == "" {
			break
		}
		s = remaining
	}
	return nil
}

func insertBaseAllPrivateH(buf *buffer) error {
	buf.writes(embedBaseFundamentalPrivateH.Trim())
	buf.writeb('\n')
	buf.writes(embedBaseRangePrivateH.Trim())
	buf.writeb('\n')
	buf.writes(embedBaseIOPrivateH.Trim())
	buf.writeb('\n')
	buf.writes(embedBaseTokenPrivateH.Trim())
	buf.writeb('\n')
	buf.writes(embedBaseMemoryPrivateH.Trim())
	buf.writeb('\n')
	buf.writes(embedBaseImagePrivateH.Trim())
	buf.writeb('\n')
	buf.writes(embedBaseStrConvPrivateH.Trim())
	return nil
}

func insertBaseAllPublicH(buf *buffer) error {
	if err := expandBangInsert(buf, embedBaseFundamentalPublicH.Trim(), map[string]func(*buffer) error{
		"// ¡ INSERT FourCCs.\n": func(b *buffer) error {
			for i, z := range builtin.FourCCs {
				if i != 0 {
					b.writeb('\n')
				}
				b.printf("// %s.\n#define WUFFS_BASE__FOURCC__%s 0x%02X%02X%02X%02X\n",
					z[1],
					strings.ToUpper(strings.TrimSpace(z[0])),
					z[0][0],
					z[0][1],
					z[0][2],
					z[0][3],
				)
			}
			return nil
		},
		"// ¡ INSERT Quirks.\n": func(b *buffer) error {
			first := true
			for _, z := range builtin.Consts {
				if (z.Name == "") || (z.Name[0] != 'Q') || !strings.HasPrefix(z.Name, "QUIRK_") {
					continue
				}
				if first {
					first = false
				} else {
					b.writeb('\n')
				}
				b.printf("#define WUFFS_BASE__%s %s\n", z.Name, z.Value)
			}
			return nil
		},
		"// ¡ INSERT wuffs_base__status names.\n": func(b *buffer) error {
			for _, z := range builtin.Statuses {
				msg, _ := t.Unescape(z)
				if msg == "" {
					return fmt.Errorf("bad built-in status %q", z)
				}
				pre := "note"
				if statusMsgIsError(msg) {
					pre = "error"
				} else if statusMsgIsSuspension(msg) {
					pre = "suspension"
				}
				b.printf("extern const char wuffs_base__%s__%s[];\n", pre, cName(msg, ""))
			}
			return nil
		},
	}); err != nil {
		return err
	}
	buf.writeb('\n')

	buf.writes(embedBaseRangePublicH.Trim())
	buf.writeb('\n')
	buf.writes(embedBaseIOPublicH.Trim())
	buf.writeb('\n')
	buf.writes(embedBaseTokenPublicH.Trim())
	buf.writeb('\n')
	buf.writes(embedBaseMemoryPublicH.Trim())
	buf.writeb('\n')
	buf.writes(embedBaseImagePublicH.Trim())
	buf.writeb('\n')
	buf.writes(embedBaseStrConvPublicH.Trim())
	return nil
}

func insertBaseCopyright(buf *buffer) error {
	s := string(embedBaseAllImplC)
	if i := strings.Index(s, "\n\n"); i >= 0 {
		buf.writes(s[:i+1])
	}
	return nil
}

func insertBaseFloatConvSubmoduleC(buf *buffer) error {
	buf.writes(embedBaseFloatConvSubmoduleDataC.Trim())
	buf.writeb('\n')
	buf.writes(embedBaseFloatConvSubmoduleCodeC.Trim())
	return nil
}

func insertBaseIntConvSubmoduleC(buf *buffer) error {
	buf.writes(embedBaseIntConvSubmoduleC.Trim())
	return nil
}

func insertBaseMagicSubmoduleC(buf *buffer) error {
	buf.writes(embedBaseMagicSubmoduleC.Trim())
	return nil
}

func insertBasePixConvSubmoduleRegularC(buf *buffer) error {
	buf.writes(embedBasePixConvSubmoduleRegularC.Trim())
	return nil
}

func insertBasePixConvSubmoduleX86Avx2C(buf *buffer) error {
	buf.writes(embedBasePixConvSubmoduleX86Avx2C.Trim())
	return nil
}

func insertBasePixConvSubmoduleYcckC(buf *buffer) error {
	buf.writes(embedBasePixConvSubmoduleYcckC.Trim())
	return nil
}

func insertBaseUTF8SubmoduleC(buf *buffer) error {
	buf.writes(embedBaseUTF8SubmoduleC.Trim())
	return nil
}

func insertInterfaceDeclarations(buf *buffer) error {
	if err := parseBuiltInInterfaceMethods(); err != nil {
		return err
	}

	g := &gen{
		pkgPrefix: "wuffs_base__",
		pkgName:   "base",
		tm:        &builtInTokenMap,
	}

	buf.writes("// ---------------- Interface Declarations.\n\n")

	buf.writes("// For modular builds that divide the base module into sub-modules, using these\n")
	buf.writes("// functions require the WUFFS_CONFIG__MODULE__BASE__INTERFACES sub-module, not\n")
	buf.writes("// just WUFFS_CONFIG__MODULE__BASE__CORE.\n")

	for _, n := range builtin.Interfaces {
		buf.writes("\n// --------\n\n")

		qid := t.QID{t.IDBase, builtInTokenMap.ByName(n)}

		buf.printf("extern const char wuffs_base__%s__vtable_name[];\n\n", n)

		buf.printf("typedef struct wuffs_base__%s__func_ptrs__struct {\n", n)
		for _, f := range builtInInterfaceMethods[qid] {
			buf.writes("  ")
			if err := g.writeFuncSignature(buf, f, wfsCFuncPtrField); err != nil {
				return err
			}
			buf.writes(";\n")
		}
		buf.printf("} wuffs_base__%s__func_ptrs;\n\n", n)

		buf.printf("typedef struct wuffs_base__%s__struct wuffs_base__%s;\n\n", n, n)

		for _, f := range builtInInterfaceMethods[qid] {
			if err := g.writeFuncSignature(buf, f, wfsCDecl); err != nil {
				return err
			}
			buf.writes(";\n\n")
		}

		buf.writes("#if defined(__cplusplus) || defined(WUFFS_IMPLEMENTATION)\n\n")

		buf.printf("struct wuffs_base__%s__struct {\n", n)
		buf.writes("  struct {\n")
		buf.writes("    uint32_t magic;\n")
		buf.writes("    uint32_t active_coroutine;\n")
		buf.writes("    wuffs_base__vtable first_vtable;\n")
		buf.writes("  } private_impl;\n\n")

		buf.writes("#ifdef __cplusplus\n")
		buf.writes("#if defined(WUFFS_BASE__HAVE_UNIQUE_PTR)\n")
		buf.printf("  using unique_ptr = std::unique_ptr<wuffs_base__%s, wuffs_unique_ptr_deleter>;\n", n)
		buf.writes("#endif\n\n")

		for _, f := range builtInInterfaceMethods[qid] {
			if err := g.writeFuncSignature(buf, f, wfsCppDecl); err != nil {
				return err
			}
			buf.writes(" {\n    return ")
			buf.writes(g.funcCName(f))
			if len(f.In().Fields()) == 0 {
				buf.writes("(this")
			} else {
				buf.writes("(\n        this")
				for _, o := range f.In().Fields() {
					buf.writes(", ")
					buf.writes(aPrefix)
					buf.writes(o.AsField().Name().Str(g.tm))
				}
			}
			buf.writes(");\n  }\n\n")
		}
		buf.writes("#endif  // __cplusplus\n")
		buf.printf("};  // struct wuffs_base__%s__struct\n\n", n)

		buf.writes("#endif  // defined(__cplusplus) || defined(WUFFS_IMPLEMENTATION)\n")
	}
	return nil
}

func insertInterfaceDefinitions(buf *buffer) error {
	if err := parseBuiltInInterfaceMethods(); err != nil {
		return err
	}

	g := &gen{
		pkgPrefix: "wuffs_base__",
		pkgName:   "base",
		tm:        &builtInTokenMap,
	}

	buf.writes("// ---------------- Interface Definitions.\n")
	for i, n := range builtin.Interfaces {
		if i > 0 {
			buf.writes("// --------\n")
		}

		qid := t.QID{t.IDBase, builtInTokenMap.ByName(n)}

		for _, f := range builtInInterfaceMethods[qid] {
			returnsStatus := f.Effect().Coroutine() ||
				((f.Out() != nil) && f.Out().IsStatus())

			buf.writeb('\n')
			if err := g.writeFuncSignature(buf, f, wfsCDecl); err != nil {
				return err
			}
			buf.writes(" {\n")
			if err := writeFuncImplSelfMagicCheck(buf, g.tm, f); err != nil {
				return err
			}

			buf.writes("\n  const wuffs_base__vtable* v = &self->private_impl.first_vtable;\n")
			buf.writes("  int i;\n")
			buf.printf("  for (i = 0; i < %d; i++) {\n", a.MaxImplements)
			buf.printf("    if (v->vtable_name == wuffs_base__%s__vtable_name) {\n", n)
			buf.printf("      const wuffs_base__%s__func_ptrs* func_ptrs =\n"+
				"          (const wuffs_base__%s__func_ptrs*)(v->function_pointers);\n", n, n)
			buf.printf("      return (*func_ptrs->%s)(self", f.FuncName().Str(g.tm))
			for _, o := range f.In().Fields() {
				buf.writes(", ")
				buf.writes(aPrefix)
				buf.writes(o.AsField().Name().Str(g.tm))
			}
			buf.writes(");\n")
			buf.writes("    } else if (v->vtable_name == NULL) {\n")
			buf.writes("      break;\n")
			buf.writes("    }\n")
			buf.writes("    v++;\n")
			buf.writes("  }\n\n")

			buf.writes("  return ")
			if returnsStatus {
				buf.writes("wuffs_base__make_status(wuffs_base__error__bad_vtable)")
			} else if err := writeOutParamZeroValue(buf, g.tm, f.Out()); err != nil {
				return err
			}
			buf.writes(";\n}\n")
		}
		if (i + 1) < len(builtin.Interfaces) {
			buf.writeb('\n')
		}
	}

	return nil
}

var (
	builtInTokenMap         = t.Map{}
	builtInInterfaceMethods = map[t.QID][]*a.Func{}
)

func parseBuiltInInterfaceMethods() error {
	if len(builtInInterfaceMethods) != 0 {
		return nil
	}
	return builtin.ParseFuncs(&builtInTokenMap, builtin.InterfaceFuncs, func(f *a.Func) error {
		qid := f.Receiver()
		builtInInterfaceMethods[qid] = append(builtInInterfaceMethods[qid], f)
		return nil
	})
}

type gen struct {
	PKGPREFIX string // e.g. "WUFFS_JPEG__"
	PKGNAME   string // e.g. "JPEG"
	pkgPrefix string // e.g. "wuffs_jpeg__"
	pkgName   string // e.g. "jpeg"

	tm    *t.Map
	files []*a.File

	// genlinenum is whether to print "// foo.wuffs:123" comments in the
	// generated C code. This can be useful for debugging, although it is not
	// enabled by default as it can lead to many spurious changes in the
	// generated C code (due to line numbers changing) when editing Wuffs code.
	genlinenum bool

	privateDataFields map[t.QQID]struct{}
	scalarConstsMap   map[t.QID]*a.Const
	statusList        []status
	statusMap         map[t.QID]status
	structList        []*a.Struct
	structMap         map[t.QID]*a.Struct

	currFunk funk
	funks    map[t.QQID]funk

	numPublicCoroutines map[t.QID]uint32
}

func (g *gen) generate() ([]byte, error) {
	b := new(buffer)

	g.statusMap = map[t.QID]status{}
	if err := g.forEachStatus(b, bothPubPri, (*gen).gatherStatuses); err != nil {
		return nil, err
	}
	for _, z := range builtin.Statuses {
		id, err := g.tm.Insert(z)
		if err != nil {
			return nil, err
		}
		msg, _ := t.Unescape(z)
		if msg == "" {
			return nil, fmt.Errorf("bad built-in status %q", z)
		}
		if err := g.addStatus(t.QID{t.IDBase, id}, msg, true); err != nil {
			return nil, err
		}
	}

	g.scalarConstsMap = map[t.QID]*a.Const{}
	if err := g.forEachConst(b, bothPubPri, (*gen).gatherScalarConsts); err != nil {
		return nil, err
	}

	// Make a topologically sorted list of structs.
	unsortedStructs := []*a.Struct(nil)
	for _, file := range g.files {
		for _, tld := range file.TopLevelDecls() {
			if tld.Kind() == a.KStruct {
				unsortedStructs = append(unsortedStructs, tld.AsStruct())
			}
		}
	}
	var ok bool
	g.structList, ok = a.TopologicalSortStructs(unsortedStructs)
	if !ok {
		return nil, fmt.Errorf("cyclical struct definitions")
	}
	g.structMap = map[t.QID]*a.Struct{}
	g.privateDataFields = map[t.QQID]struct{}{}
	g.numPublicCoroutines = map[t.QID]uint32{}
	for _, n := range g.structList {
		qid := n.QID()
		g.structMap[qid] = n
		for _, f := range n.Fields() {
			f := f.AsField()
			if f.PrivateData() {
				g.privateDataFields[t.QQID{qid[0], qid[1], f.Name()}] = struct{}{}
			}
		}
	}

	g.funks = map[t.QQID]funk{}
	if err := g.forEachFunc(nil, bothPubPri, (*gen).gatherFuncImpl); err != nil {
		return nil, err
	}

	includeGuard := "WUFFS_INCLUDE_GUARD__" + g.PKGNAME
	b.printf("#ifndef %s\n#define %s\n\n", includeGuard, includeGuard)

	if err := g.genIncludes(b); err != nil {
		return nil, err
	}

	b.writes("// ¡ WUFFS MONOLITHIC RELEASE DISCARDS EVERYTHING ABOVE.\n\n")

	if err := g.genHeader(b); err != nil {
		return nil, err
	}
	b.writex(wiStartImpl)
	if err := g.genImpl(b); err != nil {
		return nil, err
	}
	b.writex(wiEnd)

	b.writes("// ¡ WUFFS MONOLITHIC RELEASE DISCARDS EVERYTHING BELOW.\n\n")

	b.printf("#endif  // %s\n\n", includeGuard)
	return *b, nil
}

var (
	wiStartImpl = []byte("\n// ‼ WUFFS C HEADER ENDS HERE.\n#ifdef WUFFS_IMPLEMENTATION\n\n")
	wiEnd       = []byte("\n#endif  // WUFFS_IMPLEMENTATION\n\n")
)

func (g *gen) genIncludes(b *buffer) error {
	b.writes("#if defined(WUFFS_IMPLEMENTATION) && !defined(WUFFS_CONFIG__MODULES)\n")
	b.writes("#define WUFFS_CONFIG__MODULES\n")
	b.printf("#define WUFFS_CONFIG__MODULE__%s\n", g.PKGNAME)
	b.writes("#define WUFFS_NONMONOLITHIC\n")
	b.writes("#endif\n\n")

	usesList := []string(nil)
	usesMap := map[string]struct{}{}

	for _, file := range g.files {
		for _, tld := range file.TopLevelDecls() {
			if tld.Kind() != a.KUse {
				continue
			}
			useDirname := g.tm.ByID(tld.AsUse().Path())
			useDirname, _ = t.Unescape(useDirname)

			// TODO: coherence check useDirname via commonflags.IsValidUsePath?

			if _, ok := usesMap[useDirname]; ok {
				continue
			}
			usesMap[useDirname] = struct{}{}
			usesList = append(usesList, useDirname)
		}
	}
	sort.Strings(usesList)

	b.writes("#include \"./wuffs-base.c\"\n")
	for _, use := range usesList {
		b.printf("#include \"./wuffs-%s.c\"\n",
			strings.Replace(use, "/", "-", -1))
	}

	b.writeb('\n')
	return nil
}

func (g *gen) genHeader(b *buffer) error {
	module := "!defined(WUFFS_CONFIG__MODULES) || defined(WUFFS_CONFIG__MODULE__" + g.PKGNAME +
		") || defined(WUFFS_NONMONOLITHIC)"
	b.printf("#if %s\n\n", module)

	b.writes("// ---------------- Status Codes\n\n")

	wroteStatus := false
	for _, z := range g.statusList {
		if !z.fromThisPkg || !z.public {
			continue
		}
		b.printf("extern const char %s[];\n", z.cName)
		wroteStatus = true
	}
	if wroteStatus {
		b.writes("\n")
	}

	b.writes("// ---------------- Public Consts\n\n")
	if err := g.forEachConst(b, pubOnly, (*gen).writeConst); err != nil {
		return err
	}

	b.writes("// ---------------- Struct Declarations\n\n")
	for _, n := range g.structList {
		structName := n.QID().Str(g.tm)
		b.printf("typedef struct %s%s__struct %s%s;\n\n", g.pkgPrefix, structName, g.pkgPrefix, structName)
	}

	b.writes("#ifdef __cplusplus\nextern \"C\" {\n#endif\n\n")

	b.writes("// ---------------- Public Initializer Prototypes\n\n")
	b.writes("// For any given \"wuffs_foo__bar* self\", \"wuffs_foo__bar__initialize(self,\n")
	b.writes("// etc)\" should be called before any other \"wuffs_foo__bar__xxx(self, etc)\".\n")
	b.writes("//\n")
	b.writes("// Pass sizeof(*self) and WUFFS_VERSION for sizeof_star_self and wuffs_version.\n")
	b.writes("// Pass 0 (or some combination of WUFFS_INITIALIZE__XXX) for options.\n\n")
	for _, n := range g.structList {
		if n.Public() {
			if err := g.writeInitializerPrototype(b, n); err != nil {
				return err
			}
		}
	}

	b.writes("// ---------------- Allocs\n\n")

	b.writes("// These functions allocate and initialize Wuffs structs. They return NULL if\n")
	b.writes("// memory allocation fails. If they return non-NULL, there is no need to call\n")
	b.writes("// wuffs_foo__bar__initialize, but the caller is responsible for eventually\n")
	b.writes("// calling free on the returned pointer. That pointer is effectively a C++\n")
	b.writes("// std::unique_ptr<T, wuffs_unique_ptr_deleter>.\n\n")

	for _, n := range g.structList {
		if !n.Public() {
			continue
		}
		if err := g.writeAllocSignature(b, n); err != nil {
			return err
		}
		b.writes(";\n\n")
		structName := n.QID().Str(g.tm)
		for _, impl := range n.Implements() {
			iQID := impl.AsTypeExpr().QID()
			iName := fmt.Sprintf("wuffs_%s__%s", iQID[0].Str(g.tm), iQID[1].Str(g.tm))
			b.printf("static inline %s*\n", iName)
			b.printf("%s%s__alloc_as__%s(void) {\n", g.pkgPrefix, structName, iName)
			b.printf("return (%s*)(%s%s__alloc());\n", iName, g.pkgPrefix, structName)
			b.printf("}\n\n")
		}
	}

	b.writes("// ---------------- Upcasts\n\n")
	for _, n := range g.structList {
		structName := n.QID().Str(g.tm)
		for _, impl := range n.Implements() {
			iQID := impl.AsTypeExpr().QID()
			iName := fmt.Sprintf("wuffs_%s__%s", iQID[0].Str(g.tm), iQID[1].Str(g.tm))
			b.printf("static inline %s*\n", iName)
			b.printf("%s%s__upcast_as__%s(\n    %s%s* p) {\n",
				g.pkgPrefix, structName, iName, g.pkgPrefix, structName)
			b.printf("return (%s*)p;\n", iName)
			b.printf("}\n\n")
		}
	}

	b.writes("// ---------------- Public Function Prototypes\n\n")
	if err := g.forEachFunc(b, pubOnly, (*gen).writeFuncPrototype); err != nil {
		return err
	}

	b.writes("#ifdef __cplusplus\n}  // extern \"C\"\n#endif\n\n")

	b.writes("// ---------------- Struct Definitions\n\n")
	b.writes("// These structs' fields, and the sizeof them, are private implementation\n")
	b.writes("// details that aren't guaranteed to be stable across Wuffs versions.\n")
	b.writes("//\n")
	b.writes("// See https://en.wikipedia.org/wiki/Opaque_pointer#C\n\n")
	b.writes("#if defined(__cplusplus) || defined(WUFFS_IMPLEMENTATION)\n\n")

	for _, n := range g.structList {
		if err := g.writeStruct(b, n); err != nil {
			return err
		}
	}
	b.writes("#endif  // defined(__cplusplus) || defined(WUFFS_IMPLEMENTATION)\n\n")

	b.printf("#endif  // %s\n", module)
	return nil
}

func (g *gen) genImpl(b *buffer) error {
	module := "!defined(WUFFS_CONFIG__MODULES) || defined(WUFFS_CONFIG__MODULE__" + g.PKGNAME + ")"
	b.printf("#if %s\n\n", module)

	b.writes("// ---------------- Status Codes Implementations\n\n")

	wroteStatus := false
	for _, z := range g.statusList {
		if !z.fromThisPkg || z.msg == "" {
			continue
		}
		b.printf("const char %s[] = \"%s%s: %s\";\n", z.cName, z.msg[:1], g.pkgName, z.msg[1:])
		wroteStatus = true
	}
	if wroteStatus {
		b.writes("\n")
	}

	b.writes("// ---------------- Private Consts\n\n")
	if err := g.forEachConst(b, priOnly, (*gen).writeConst); err != nil {
		return err
	}

	b.writes("// ---------------- Private Initializer Prototypes\n\n")
	for _, n := range g.structList {
		if !n.Public() {
			if err := g.writeInitializerPrototype(b, n); err != nil {
				return err
			}
		}
	}

	b.writes("// ---------------- Private Function Prototypes\n\n")
	if err := g.forEachFunc(b, priOnly, (*gen).writeFuncPrototype); err != nil {
		return err
	}

	b.writes("// ---------------- VTables\n\n")
	for _, n := range g.structList {
		if err := g.writeVTableImpl(b, n); err != nil {
			return err
		}
	}

	b.writes("// ---------------- Initializer Implementations\n\n")
	for _, n := range g.structList {
		if err := g.writeInitializerImpl(b, n); err != nil {
			return err
		}
	}

	b.writes("// ---------------- Function Implementations\n\n")
	if err := g.forEachFunc(b, bothPubPri, (*gen).writeFuncImpl); err != nil {
		return err
	}

	b.printf("#endif  // %s\n", module)
	return nil
}

func (g *gen) forEachConst(b *buffer, v visibility, f func(*gen, *buffer, *a.Const) error) error {
	for _, file := range g.files {
		for _, tld := range file.TopLevelDecls() {
			if tld.Kind() != a.KConst ||
				((v == pubOnly) && !tld.AsConst().Public()) ||
				((v == priOnly) && tld.AsConst().Public()) {
				continue
			}
			if err := f(g, b, tld.AsConst()); err != nil {
				return err
			}
		}
	}
	return nil
}

func (g *gen) forEachFunc(b *buffer, v visibility, f func(*gen, *buffer, *a.Func) error) error {
	for _, file := range g.files {
		for _, tld := range file.TopLevelDecls() {
			if tld.Kind() != a.KFunc ||
				((v == pubOnly) && !tld.AsFunc().Public()) ||
				((v == priOnly) && tld.AsFunc().Public()) {
				continue
			}
			if err := f(g, b, tld.AsFunc()); err != nil {
				return err
			}
		}
	}
	return nil
}

func (g *gen) forEachStatus(b *buffer, v visibility, f func(*gen, *buffer, *a.Status) error) error {
	for _, file := range g.files {
		for _, tld := range file.TopLevelDecls() {
			if tld.Kind() != a.KStatus ||
				((v == pubOnly) && !tld.AsStatus().Public()) ||
				((v == priOnly) && tld.AsStatus().Public()) {
				continue
			}
			if err := f(g, b, tld.AsStatus()); err != nil {
				return err
			}
		}
	}
	return nil
}

func (g *gen) findAstFunc(qqid t.QQID) *a.Func {
	for _, file := range g.files {
		for _, tld := range file.TopLevelDecls() {
			if (tld.Kind() == a.KFunc) && (tld.AsFunc().QQID() == qqid) {
				return tld.AsFunc()
			}
		}
	}
	return nil
}

func (g *gen) cName(name string) string {
	return cName(name, g.pkgPrefix)
}

func cName(name string, pkgPrefix string) string {
	s := []byte(pkgPrefix)
	underscore := true
	for _, r := range name {
		if 'A' <= r && r <= 'Z' {
			s = append(s, byte(r+'a'-'A'))
			underscore = false
		} else if ('a' <= r && r <= 'z') || ('0' <= r && r <= '9') {
			s = append(s, byte(r))
			underscore = false
		} else if !underscore {
			s = append(s, '_')
			underscore = true
		}
	}
	if underscore {
		s = s[:len(s)-1]
	}
	return string(s)
}

func uintBits(qid t.QID) uint32 {
	if qid[0] == t.IDBase {
		switch qid[1] {
		case t.IDU8:
			return 8
		case t.IDU16:
			return 16
		case t.IDU32:
			return 32
		case t.IDU64:
			return 64
		}
	}
	return 0
}

func (g *gen) sizeof(typ *a.TypeExpr) (uint32, error) {
	if typ.Decorator() == 0 {
		if n := uintBits(typ.QID()); n != 0 {
			return n / 8, nil
		}
	}
	return 0, fmt.Errorf("unknown sizeof for %q", typ.Str(g.tm))
}

func (g *gen) gatherStatuses(b *buffer, n *a.Status) error {
	raw := n.QID()[1].Str(g.tm)
	msg, ok := t.Unescape(raw)
	if !ok || msg == "" {
		return fmt.Errorf("bad status message %q", raw)
	}
	return g.addStatus(n.QID(), msg, n.Public())
}

func (g *gen) addStatus(qid t.QID, msg string, public bool) error {
	category := "note__"
	if msg[0] == '$' {
		category = "suspension__"
	} else if msg[0] == '#' {
		category = "error__"
	}
	z := status{
		cName:       g.packagePrefix(qid) + category + cName(msg, ""),
		msg:         msg,
		fromThisPkg: qid[0] == 0,
		public:      public,
	}
	g.statusList = append(g.statusList, z)
	g.statusMap[qid] = z
	return nil
}

func (g *gen) gatherScalarConsts(b *buffer, n *a.Const) error {
	if cv := n.Value().ConstValue(); cv != nil {
		g.scalarConstsMap[n.QID()] = n
	}
	return nil
}

func (g *gen) writeConst(b *buffer, n *a.Const) error {
	if cv := n.Value().ConstValue(); cv != nil {
		suffix := ""
		if cv.Sign() >= 0 {
			suffix = "u"
		}
		b.printf("#define %s%s %v%s\n\n", g.PKGPREFIX, n.QID()[1].Str(g.tm), cv, suffix)
	} else {
		b.writes("static const ")
		if err := g.writeCTypeName(b, n.XType(), "\n"+g.PKGPREFIX, n.QID()[1].Str(g.tm)); err != nil {
			return err
		}
		b.writes(" WUFFS_BASE__POTENTIALLY_UNUSED = ")
		if err := g.writeConstList(b, n.Value()); err != nil {
			return err
		}
		b.writes(";\n\n")
	}
	return nil
}

func (g *gen) writeConstList(b *buffer, n *a.Expr) error {
	if args, ok := n.IsList(); ok {
		b.writeb('{')
		for i, o := range args {
			if i&7 == 0 {
				b.writeb('\n')
			}
			if err := g.writeConstList(b, o.AsExpr()); err != nil {
				return err
			}
			b.writes(", ")
		}
		b.writes("\n}")
	} else if cv := n.ConstValue(); cv != nil {
		b.writes(cv.String())
		if cv.Sign() >= 0 {
			b.writeb('u')
		}
	} else {
		return fmt.Errorf("invalid const value %q", n.Str(g.tm))
	}
	return nil
}

func (g *gen) writeStructPrivateImpl(b *buffer, n *a.Struct) error {
	b.writes("// Do not access the private_impl's or private_data's fields directly. There\n")
	b.writes("// is no API/ABI compatibility or safety guarantee if you do so. Instead, use\n")
	b.writes("// the wuffs_foo__bar__baz functions.\n")
	b.writes("//\n")
	b.writes("// It is a struct, not a struct*, so that the outermost wuffs_foo__bar struct\n")
	b.writes("// can be stack allocated when WUFFS_IMPLEMENTATION is defined.\n\n")

	b.writes("struct {\n")
	if n.Classy() {
		b.writes("uint32_t magic;\n")
		b.writes("uint32_t active_coroutine;\n")
		for _, impl := range n.Implements() {
			qid := impl.AsTypeExpr().QID()
			b.printf("wuffs_base__vtable vtable_for__wuffs_%s__%s;\n",
				qid[0].Str(g.tm), qid[1].Str(g.tm))
		}
		b.writes("wuffs_base__vtable null_vtable;\n")
		b.writes("\n")
	}

	for _, o := range n.Fields() {
		o := o.AsField()
		if o.PrivateData() || o.XType().IsEtcUtilityType() {
			continue
		}
		if err := g.writeCTypeName(b, o.XType(), fPrefix, o.Name().Str(g.tm)); err != nil {
			return err
		}
		b.writes(";\n")
	}

	if n.Classy() {
		needEmptyLine := true
		for _, file := range g.files {
			for _, tld := range file.TopLevelDecls() {
				if tld.Kind() != a.KFunc {
					continue
				}
				o := tld.AsFunc()
				if o.Receiver() != n.QID() {
					continue

				} else if o.Effect().Coroutine() {
					k := g.funks[o.QQID()]
					if k.coroSuspPoint == 0 {
						continue
					}
					if needEmptyLine {
						needEmptyLine = false
						b.writeb('\n')
					}
					b.printf("uint32_t %s%s;\n", pPrefix, o.FuncName().Str(g.tm))

				} else if o.Choosy() {
					if needEmptyLine {
						needEmptyLine = false
						b.writeb('\n')
					}
					if err := g.writeFuncSignature(b, o, wfsCFuncPtrFieldChoosy); err != nil {
						return err
					}
					b.writes(";\n")
				}
			}
		}
	}
	b.writes("} private_impl;\n\n")

	{
		oldOuterLenB0 := len(*b)
		b.writes("struct {\n")
		oldOuterLenB1 := len(*b)

		for _, o := range n.Fields() {
			o := o.AsField()
			if !o.PrivateData() || o.XType().IsEtcUtilityType() {
				continue
			}
			if err := g.writeCTypeName(b, o.XType(), fPrefix, o.Name().Str(g.tm)); err != nil {
				return err
			}
			b.writes(";\n")
		}

		needEmptyLine := oldOuterLenB1 != len(*b)
		for _, file := range g.files {
			for _, tld := range file.TopLevelDecls() {
				if tld.Kind() != a.KFunc {
					continue
				}
				o := tld.AsFunc()
				if o.Receiver() != n.QID() || !o.Effect().Coroutine() {
					continue
				}
				k := g.funks[o.QQID()]
				if k.coroSuspPoint == 0 && !k.usesScratch {
					continue
				}

				oldInnerLenB0 := len(*b)
				oldNeedEmptyLine := needEmptyLine
				if needEmptyLine {
					needEmptyLine = false
					b.writeb('\n')
				}
				b.writes("struct {\n")
				oldInnerLenB1 := len(*b)
				if k.coroSuspPoint != 0 {
					if err := g.writeVars(b, &k, true); err != nil {
						return err
					}
				}
				if k.usesScratch {
					b.writes("uint64_t scratch;\n")
				}
				if oldInnerLenB1 != len(*b) {
					b.printf("} %s%s;\n", sPrefix, o.FuncName().Str(g.tm))
				} else {
					*b = (*b)[:oldInnerLenB0]
					needEmptyLine = oldNeedEmptyLine
				}
			}
		}

		if oldOuterLenB1 != len(*b) {
			b.writes("} private_data;\n\n")
		} else {
			*b = (*b)[:oldOuterLenB0]
		}
	}

	return nil
}

func (g *gen) writeStruct(b *buffer, n *a.Struct) error {
	structName := n.QID().Str(g.tm)
	fullStructName := g.pkgPrefix + structName + "__struct"
	b.printf("struct %s {\n", fullStructName)

	if err := g.writeStructPrivateImpl(b, n); err != nil {
		return err
	}

	if n.Public() {
		if err := g.writeCppMethods(b, n); err != nil {
			return err
		}
	}

	b.printf("};  // struct %s\n\n", fullStructName)
	return nil
}

func (g *gen) writeCppMethods(b *buffer, n *a.Struct) error {
	structName := n.QID().Str(g.tm)
	fullStructName := g.pkgPrefix + structName + "__struct"
	b.writes("#ifdef __cplusplus\n")

	b.writes("#if defined(WUFFS_BASE__HAVE_UNIQUE_PTR)\n")
	b.printf("using unique_ptr = std::unique_ptr<%s%s, wuffs_unique_ptr_deleter>;\n\n", g.pkgPrefix, structName)
	b.writes("// On failure, the alloc_etc functions return nullptr. They don't throw.\n\n")
	b.writes("static inline unique_ptr\n")
	b.writes("alloc() {\n")
	b.printf("return unique_ptr(%s%s__alloc());\n", g.pkgPrefix, structName)
	b.writes("}\n")
	for _, impl := range n.Implements() {
		iQID := impl.AsTypeExpr().QID()
		iName := fmt.Sprintf("wuffs_%s__%s", iQID[0].Str(g.tm), iQID[1].Str(g.tm))
		b.printf("\nstatic inline %s::unique_ptr\n", iName)
		b.printf("alloc_as__%s() {\n", iName)
		b.printf("return %s::unique_ptr(\n%s%s__alloc_as__%s());\n",
			iName, g.pkgPrefix, structName, iName)
		b.printf("}\n")
	}
	b.writes("#endif  // defined(WUFFS_BASE__HAVE_UNIQUE_PTR)\n\n")

	b.writes("#if defined(WUFFS_BASE__HAVE_EQ_DELETE) && !defined(WUFFS_IMPLEMENTATION)\n")
	b.writes("// Disallow constructing or copying an object via standard C++ mechanisms,\n")
	b.writes("// e.g. the \"new\" operator, as this struct is intentionally opaque. Its total\n")
	b.writes("// size and field layout is not part of the public, stable, memory-safe API.\n")
	b.writes("// Use malloc or memcpy and the sizeof__wuffs_foo__bar function instead, and\n")
	b.writes("// call wuffs_foo__bar__baz methods (which all take a \"this\"-like pointer as\n")
	b.writes("// their first argument) rather than tweaking bar.private_impl.qux fields.\n")
	b.writes("//\n")
	b.writes("// In C, we can just leave wuffs_foo__bar as an incomplete type (unless\n")
	b.writes("// WUFFS_IMPLEMENTATION is #define'd). In C++, we define a complete type in\n")
	b.writes("// order to provide convenience methods. These forward on \"this\", so that you\n")
	b.writes("// can write \"bar->baz(etc)\" instead of \"wuffs_foo__bar__baz(bar, etc)\".\n")
	b.printf("%s() = delete;\n", fullStructName)
	b.printf("%s(const %s&) = delete;\n", fullStructName, fullStructName)
	b.printf("%s& operator=(\nconst %s&) = delete;\n", fullStructName, fullStructName)
	b.writes("#endif  // defined(WUFFS_BASE__HAVE_EQ_DELETE) && !defined(WUFFS_IMPLEMENTATION)\n\n")

	b.writes("#if !defined(WUFFS_IMPLEMENTATION)\n")
	b.writes("// As above, the size of the struct is not part of the public API, and unless\n")
	b.writes("// WUFFS_IMPLEMENTATION is #define'd, this struct type T should be heap\n")
	b.writes("// allocated, not stack allocated. Its size is not intended to be known at\n")
	b.writes("// compile time, but it is unfortunately divulged as a side effect of\n")
	b.writes("// defining C++ convenience methods. Use \"sizeof__T()\", calling the function,\n")
	b.writes("// instead of \"sizeof T\", invoking the operator. To make the two values\n")
	b.writes("// different, so that passing the latter will be rejected by the initialize\n")
	b.writes("// function, we add an arbitrary amount of dead weight.\n")
	b.writes("uint8_t dead_weight[123000000];  // 123 MB.\n")
	b.writes("#endif  // !defined(WUFFS_IMPLEMENTATION)\n\n")

	b.writes("inline wuffs_base__status WUFFS_BASE__WARN_UNUSED_RESULT\n" +
		"initialize(\nsize_t sizeof_star_self,\nuint64_t wuffs_version,\nuint32_t options) {\n")
	b.printf("return %s%s__initialize(\nthis, sizeof_star_self, wuffs_version, options);\n}\n\n",
		g.pkgPrefix, structName)

	for _, impl := range n.Implements() {
		iQID := impl.AsTypeExpr().QID()
		iName := fmt.Sprintf("wuffs_%s__%s", iQID[0].Str(g.tm), iQID[1].Str(g.tm))
		b.printf("inline %s*\n", iName)
		b.printf("upcast_as__%s() {\n", iName)
		b.printf("return (%s*)this;\n", iName)
		b.printf("}\n\n")
	}

	structID := n.QID()[1]
	for _, file := range g.files {
		for _, tld := range file.TopLevelDecls() {
			if (tld.Kind() != a.KFunc) || !tld.AsFunc().Public() {
				continue
			}
			f := tld.AsFunc()
			qqid := f.QQID()
			if qqid[1] != structID {
				continue
			}

			if err := g.writeFuncSignature(b, f, wfsCppDecl); err != nil {
				return err
			}
			b.writes(" {\n    return ")
			b.writes(g.funcCName(f))
			b.writes("(this")
			for _, o := range f.In().Fields() {
				b.writes(", ")
				b.writes(aPrefix)
				b.writes(o.AsField().Name().Str(g.tm))
			}
			b.writes(");\n  }\n\n")
		}
	}

	b.writes("#endif  // __cplusplus\n")
	return nil
}

func (g *gen) writeVTableImpl(b *buffer, n *a.Struct) error {
	impls := n.Implements()
	if len(impls) == 0 {
		return nil
	}

	if err := parseBuiltInInterfaceMethods(); err != nil {
		return err
	}

	nQID := n.QID()
	for _, impl := range impls {
		iQID := impl.AsTypeExpr().QID()
		b.printf("const wuffs_%s__%s__func_ptrs\n%s%s__func_ptrs_for__wuffs_%s__%s = {\n",
			iQID[0].Str(g.tm), iQID[1].Str(g.tm),
			g.pkgPrefix, nQID[1].Str(g.tm),
			iQID[0].Str(g.tm), iQID[1].Str(g.tm),
		)

		// Note the two t.Map values: g.tm and builtInTokenMap.
		altQID := t.QID{
			builtInTokenMap.ByName(iQID[0].Str(g.tm)),
			builtInTokenMap.ByName(iQID[1].Str(g.tm)),
		}
		for _, f := range builtInInterfaceMethods[altQID] {
			b.writeb('(')
			if err := g.writeFuncSignature(b, f, wfsCFuncPtrType); err != nil {
				return err
			}
			b.printf(")(&%s%s__%s),\n",
				g.pkgPrefix, nQID[1].Str(g.tm),
				f.FuncName().Str(&builtInTokenMap),
			)
		}
		b.writes("};\n\n")
	}
	return nil
}

func (g *gen) writeInitializerSignature(b *buffer, n *a.Struct, public bool) error {
	structName := n.QID().Str(g.tm)
	b.printf("wuffs_base__status WUFFS_BASE__WARN_UNUSED_RESULT\n"+
		"%s%s__initialize(\n"+
		"    %s%s* self,\n"+
		"    size_t sizeof_star_self,\n"+
		"    uint64_t wuffs_version,\n"+
		"    uint32_t options)",
		g.pkgPrefix, structName, g.pkgPrefix, structName)
	return nil
}

func (g *gen) writeAllocSignature(b *buffer, n *a.Struct) error {
	structName := n.QID().Str(g.tm)
	b.printf("%s%s*\n%s%s__alloc(void)", g.pkgPrefix, structName, g.pkgPrefix, structName)
	return nil
}

func (g *gen) writeSizeofSignature(b *buffer, n *a.Struct) error {
	structName := n.QID().Str(g.tm)
	b.printf("size_t\nsizeof__%s%s(void)", g.pkgPrefix, structName)
	return nil
}

func (g *gen) writeInitializerPrototype(b *buffer, n *a.Struct) error {
	if !n.Classy() {
		return nil
	}
	if err := g.writeInitializerSignature(b, n, n.Public()); err != nil {
		return err
	}
	b.writes(";\n\n")

	if n.Public() {
		if err := g.writeSizeofSignature(b, n); err != nil {
			return err
		}
		b.writes(";\n\n")
	}
	return nil
}

func (g *gen) writeInitializerImpl(b *buffer, n *a.Struct) error {
	if !n.Classy() {
		return nil
	}
	if err := g.writeInitializerSignature(b, n, false); err != nil {
		return err
	}
	b.writes("{\n")
	b.writes("if (!self) {\n")
	b.writes("  return wuffs_base__make_status(wuffs_base__error__bad_receiver);\n")
	b.writes("}\n")

	b.writes("if (sizeof(*self) != sizeof_star_self) {\n")
	b.writes("  return wuffs_base__make_status(wuffs_base__error__bad_sizeof_receiver);\n")
	b.writes("}\n")
	b.writes("if (((wuffs_version >> 32) != WUFFS_VERSION_MAJOR) ||\n" +
		"(((wuffs_version >> 16) & 0xFFFF) > WUFFS_VERSION_MINOR)) {\n")
	b.writes("  return wuffs_base__make_status(wuffs_base__error__bad_wuffs_version);\n")
	b.writes("}\n\n")

	b.writes("if ((options & WUFFS_INITIALIZE__ALREADY_ZEROED) != 0) {\n")
	b.writes("  // The whole point of this if-check is to detect an uninitialized *self.\n")
	b.writes("  // We disable the warning on GCC. Clang-5.0 does not have this warning.\n")
	b.writes("  #if !defined(__clang__) && defined(__GNUC__)\n")
	b.writes("  #pragma GCC diagnostic push\n")
	b.writes("  #pragma GCC diagnostic ignored \"-Wmaybe-uninitialized\"\n")
	b.writes("  #endif\n")
	b.writes("  if (self->private_impl.magic != 0) {\n")
	b.writes("    return wuffs_base__make_status(wuffs_base__error__initialize_falsely_claimed_already_zeroed);\n")
	b.writes("  }\n")
	b.writes("  #if !defined(__clang__) && defined(__GNUC__)\n")
	b.writes("  #pragma GCC diagnostic pop\n")
	b.writes("  #endif\n")
	b.writes("} else {\n")
	b.writes("  if ((options & WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED) == 0) {\n")
	b.writes("    memset(self, 0, sizeof(*self));\n")
	b.writes("    options |= WUFFS_INITIALIZE__ALREADY_ZEROED;\n")
	b.writes("  } else {\n")
	b.writes("    memset(&(self->private_impl), 0, sizeof(self->private_impl));\n")
	b.writes("  }\n")
	b.writes("}\n\n")

	// Initialize any choosy function pointers.
	hasChoosy := false
	for _, file := range g.files {
		for _, tld := range file.TopLevelDecls() {
			if tld.Kind() != a.KFunc {
				continue
			}
			o := tld.AsFunc()
			if (o.Receiver() != n.QID()) || !o.Choosy() {
				continue
			}
			hasChoosy = true
			b.printf("self->private_impl.choosy_%s = &%s__choosy_default;\n",
				o.FuncName().Str(g.tm), g.funcCName(o))
		}
	}
	if hasChoosy {
		b.writes("\n")
	}

	// Call any ctors on sub-structs.
	for _, f := range n.Fields() {
		f := f.AsField()
		x := f.XType()
		if x != x.Innermost() {
			// TODO: arrays of sub-structs.
			continue
		}

		prefix := g.pkgPrefix
		qid := x.QID()
		if qid[0] == t.IDBase {
			// Base types don't need further initialization.
			continue
		} else if qid[0] != 0 {
			// See gen.packagePrefix for a related TODO with otherPkg.
			otherPkg := g.tm.ByID(qid[0])
			prefix = "wuffs_" + otherPkg + "__"
		} else if g.structMap[qid] == nil {
			continue
		}

		b.printf("{\n")
		b.printf("wuffs_base__status z = %s%s__initialize(\n"+
			"&self->private_data.%s%s, sizeof(self->private_data.%s%s), WUFFS_VERSION, options);\n",
			prefix, qid[1].Str(g.tm), fPrefix, f.Name().Str(g.tm), fPrefix, f.Name().Str(g.tm))
		b.printf("if (z.repr) {\nreturn z;\n}\n")
		b.printf("}\n")
	}

	b.writes("self->private_impl.magic = WUFFS_BASE__MAGIC;\n")
	for _, impl := range n.Implements() {
		qid := impl.AsTypeExpr().QID()
		iName := fmt.Sprintf("wuffs_%s__%s", qid[0].Str(g.tm), qid[1].Str(g.tm))
		b.printf("self->private_impl.vtable_for__%s.vtable_name =\n"+
			"%s__vtable_name;\n", iName, iName)
		b.printf("self->private_impl.vtable_for__%s.function_pointers =\n"+
			"(const void*)(&%s%s__func_ptrs_for__%s);\n",
			iName, g.pkgPrefix, n.QID().Str(g.tm), iName)
	}
	b.writes("return wuffs_base__make_status(NULL);\n")
	b.writes("}\n\n")

	if n.Public() {
		structName := n.QID().Str(g.tm)
		if err := g.writeAllocSignature(b, n); err != nil {
			return err
		}
		b.writes(" {\n")
		b.printf("%s%s* x =\n(%s%s*)(calloc(1, sizeof(%s%s)));\n",
			g.pkgPrefix, structName, g.pkgPrefix, structName, g.pkgPrefix, structName)
		b.writes("if (!x) {\nreturn NULL;\n}\n")
		b.printf("if (%s%s__initialize(\nx, sizeof(%s%s), "+
			"WUFFS_VERSION, WUFFS_INITIALIZE__ALREADY_ZEROED).repr) {\n",
			g.pkgPrefix, structName, g.pkgPrefix, structName)
		b.writes("free(x);\nreturn NULL;\n}\n")
		b.writes("return x;\n")
		b.writes("}\n\n")

		if err := g.writeSizeofSignature(b, n); err != nil {
			return err
		}
		b.printf(" {\nreturn sizeof(%s%s);\n}\n\n", g.pkgPrefix, structName)
	}
	return nil
}
