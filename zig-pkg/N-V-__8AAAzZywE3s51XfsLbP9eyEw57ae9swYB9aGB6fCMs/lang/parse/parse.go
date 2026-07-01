// Copyright 2017 The Wuffs Authors.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

package parse

// TODO: write a formal grammar for the language.

import (
	"fmt"

	a "github.com/google/wuffs/lang/ast"
	t "github.com/google/wuffs/lang/token"
)

type Options struct {
	AllowBuiltInNames          bool
	AllowDoubleUnderscoreNames bool
}

func validConstName(s string) bool {
	if (len(s) >= 2) && (s[0] == '_') && (s[1] == '_') {
		return false
	}
	for i := 0; i < len(s); i++ {
		switch c := s[i]; {
		default:
			return false
		case c == '_':
		case ('0' <= c) && (c <= '9'):
		case ('A' <= c) && (c <= 'Z'): // Upper A-Z but not lower a-z.
		}
	}
	return true
}

func containsDoubleUnderscore(s string) bool {
	for i := 1; i < len(s); i++ {
		if (s[i-1] == '_') && (s[i] == '_') {
			return true
		}
	}
	return false
}

func isStatusMessage(s string) bool {
	if s == "" {
		return false
	}
	c := s[0]
	return (c == '@') || (c == '#') || (c == '$')
}

func Parse(tm *t.Map, filename string, src []t.Token, opts *Options) (*a.File, error) {
	p := &parser{
		tm:       tm,
		filename: filename,
		src:      src,
	}
	if len(src) > 0 {
		p.lastLine = src[len(src)-1].Line
	}
	if opts != nil {
		p.opts = *opts
	}
	return p.parseFile()
}

func ParseExpr(tm *t.Map, filename string, src []t.Token, opts *Options) (*a.Expr, error) {
	p := &parser{
		tm:       tm,
		filename: filename,
		src:      src,
	}
	if len(src) > 0 {
		p.lastLine = src[len(src)-1].Line
	}
	if opts != nil {
		p.opts = *opts
	}
	return p.parseExpr()
}

type parser struct {
	tm         *t.Map
	filename   string
	src        []t.Token
	opts       Options
	lastLine   uint32
	funcEffect a.Effect
	loops      a.LoopStack
	allowVar   bool
}

func (p *parser) line() uint32 {
	if len(p.src) != 0 {
		return p.src[0].Line
	}
	return p.lastLine
}

func (p *parser) peek1() t.ID {
	if len(p.src) > 0 {
		return p.src[0].ID
	}
	return 0
}

func (p *parser) parseFile() (*a.File, error) {
	topLevelDecls := []*a.Node(nil)
	for len(p.src) > 0 {
		d, err := p.parseTopLevelDecl()
		if err != nil {
			return nil, err
		}
		topLevelDecls = append(topLevelDecls, d)
	}
	return a.NewFile(p.filename, topLevelDecls), nil
}

func (p *parser) parseTopLevelDecl() (*a.Node, error) {
	flags := a.Flags(0)
	line := p.src[0].Line
	switch k := p.peek1(); k {
	case t.IDUse:
		p.src = p.src[1:]
		path := p.peek1()
		if !path.IsDQStrLiteral(p.tm) {
			got := p.tm.ByID(path)
			return nil, fmt.Errorf(`parse: expected "-string literal, got %q at %s:%d`, got, p.filename, p.line())
		}
		p.src = p.src[1:]
		if x := p.peek1(); x != t.IDSemicolon {
			got := p.tm.ByID(x)
			return nil, fmt.Errorf(`parse: expected (implicit) ";", got %q at %s:%d`, got, p.filename, p.line())
		}
		p.src = p.src[1:]
		return a.NewUse(p.filename, line, path).AsNode(), nil

	case t.IDPub:
		flags |= a.FlagsPublic
		fallthrough
	case t.IDPri:
		p.src = p.src[1:]
		switch p.peek1() {
		case t.IDConst:
			p.src = p.src[1:]
			id, err := p.parseIdent()
			if err != nil {
				return nil, err
			}
			if !validConstName(p.tm.ByID(id)) {
				return nil, fmt.Errorf(`parse: invalid const name %q at %s:%d`,
					p.tm.ByID(id), p.filename, p.line())
			}

			if x := p.peek1(); x != t.IDColon {
				got := p.tm.ByID(x)
				return nil, fmt.Errorf(`parse: expected ":", got %q at %s:%d`, got, p.filename, p.line())
			}
			p.src = p.src[1:]

			typ, err := p.parseTypeExpr()
			if err != nil {
				return nil, err
			}
			if p.peek1() != t.IDEq {
				return nil, fmt.Errorf(`parse: const %q has no value at %s:%d`,
					p.tm.ByID(id), p.filename, p.line())
			}
			p.src = p.src[1:]
			value, err := p.parsePossibleListExpr()
			if err != nil {
				return nil, err
			}
			if x := p.peek1(); x != t.IDSemicolon {
				got := p.tm.ByID(x)
				return nil, fmt.Errorf(`parse: expected (implicit) ";", got %q at %s:%d`, got, p.filename, p.line())
			}
			p.src = p.src[1:]
			return a.NewConst(flags, p.filename, line, id, typ, value).AsNode(), nil

		case t.IDFunc:
			p.src = p.src[1:]
			id0, id1, err := p.parseQualifiedIdent()
			if err != nil {
				return nil, err
			}
			if !p.opts.AllowBuiltInNames {
				switch id1 {
				case t.IDInitialize, t.IDReset:
					return nil, fmt.Errorf(`parse: cannot have a method named %q at %s:%d`,
						id1.Str(p.tm), p.filename, p.line())
				}
			}
			// TODO: should we require id0 != 0? In other words, always methods
			// (attached to receivers) and never free standing functions?
			if !p.opts.AllowDoubleUnderscoreNames && containsDoubleUnderscore(p.tm.ByID(id1)) {
				return nil, fmt.Errorf(`parse: double-underscore %q used for func name at %s:%d`,
					p.tm.ByID(id1), p.filename, p.line())
			}

			p.funcEffect = p.parseEffect()
			flags |= p.funcEffect.AsFlags()
			argFields, err := p.parseList(t.IDCloseParen, (*parser).parseFieldNode)
			if err != nil {
				return nil, err
			}
			out := (*a.TypeExpr)(nil)
			if x := p.peek1(); (x != t.IDOpenCurly) && (x != t.IDComma) {
				out, err = p.parseTypeExpr()
				if err != nil {
					return nil, err
				}
			}
			asserts := []*a.Node(nil)
			if p.peek1() == t.IDComma {
				p.src = p.src[1:]
				if p.peek1() == t.IDChoosy {
					p.src = p.src[1:]
					if (flags & a.FlagsPublic) != 0 {
						return nil, fmt.Errorf(`parse: choosy function cannot be pub at %s:%d`,
							p.filename, p.line())
					} else if p.funcEffect.Coroutine() {
						return nil, fmt.Errorf(`parse: choosy function cannot be a coroutine at %s:%d`,
							p.filename, p.line())
					}
					flags |= a.FlagsChoosy
					if p.peek1() != t.IDOpenCurly {
						if x := p.peek1(); x != t.IDComma {
							return nil, fmt.Errorf(`parse: expected ",", got %q at %s:%d`,
								p.tm.ByID(x), p.filename, p.line())
						}
						p.src = p.src[1:]
					}
				}

				asserts, err = p.parseList(t.IDOpenCurly, (*parser).parseAssertNode)
				if err != nil {
					return nil, err
				}
				if err := p.assertsSorted(asserts, true); err != nil {
					return nil, err
				}
				for _, o := range asserts {
					o := o.AsAssert()
					if o.Keyword() != t.IDChoose {
						continue
					} else if o.IsChooseCPUArch() {
						flags |= a.FlagsHasChooseCPUArch
					} else {
						return nil, fmt.Errorf(`parse: invalid "choose" condition at %s:%d`,
							p.filename, p.line())
					}
				}
			}

			p.allowVar = true
			body, err := p.parseBlock(false)
			if err != nil {
				return nil, err
			}
			p.allowVar = false

			if x := p.peek1(); x != t.IDSemicolon {
				got := p.tm.ByID(x)
				return nil, fmt.Errorf(`parse: expected (implicit) ";", got %q at %s:%d`, got, p.filename, p.line())
			}
			p.src = p.src[1:]

			if (flags & a.FlagsHasChooseCPUArch) != 0 {
				if (flags & a.FlagsPublic) != 0 {
					return nil, fmt.Errorf(`parse: cpu_arch function cannot be public at %s:%d`,
						p.filename, p.line())
				}
				if (flags & a.FlagsChoosy) != 0 {
					return nil, fmt.Errorf(`parse: cpu_arch function cannot be choosy at %s:%d`,
						p.filename, p.line())
				}
			}
			p.funcEffect = 0
			in := a.NewStruct(0, p.filename, line, t.IDArgs, nil, argFields)
			return a.NewFunc(flags, p.filename, line, id0, id1, in, out, asserts, body).AsNode(), nil

		case t.IDStatus:
			p.src = p.src[1:]

			message := p.peek1()
			if !message.IsDQStrLiteral(p.tm) {
				got := p.tm.ByID(message)
				return nil, fmt.Errorf(`parse: expected "-string literal, got %q at %s:%d`, got, p.filename, p.line())
			}
			if s, _ := t.Unescape(p.tm.ByID(message)); !isStatusMessage(s) {
				return nil, fmt.Errorf(`parse: status message %q does not start with `+
					`@, # or $ at %s:%d`, s, p.filename, p.line())
			}
			p.src = p.src[1:]
			if x := p.peek1(); x != t.IDSemicolon {
				got := p.tm.ByID(x)
				return nil, fmt.Errorf(`parse: expected (implicit) ";", got %q at %s:%d`, got, p.filename, p.line())
			}
			p.src = p.src[1:]
			return a.NewStatus(flags, p.filename, line, message).AsNode(), nil

		case t.IDStruct:
			p.src = p.src[1:]
			name, err := p.parseIdent()
			if err != nil {
				return nil, err
			}
			if !p.opts.AllowDoubleUnderscoreNames && containsDoubleUnderscore(p.tm.ByID(name)) {
				return nil, fmt.Errorf(`parse: double-underscore %q used for struct name at %s:%d`,
					p.tm.ByID(name), p.filename, p.line())
			}

			if p.peek1() == t.IDQuestion {
				flags |= a.FlagsClassy
				p.src = p.src[1:]
			}

			implements := []*a.Node(nil)
			if p.peek1() == t.IDImplements {
				p.src = p.src[1:]
				implements, err = p.parseList(t.IDOpenParen, (*parser).parseQualifiedIdentAsTypeExprNode)
				if err != nil {
					return nil, err
				}
				if len(implements) > a.MaxImplements {
					return nil, fmt.Errorf(`parse: too many implements listed at %s:%d`, p.filename, p.line())
				}
			}

			fields, err := p.parseList(t.IDCloseParen, (*parser).parseFieldNode)
			if err != nil {
				return nil, err
			}
			if x := p.peek1(); x == t.IDPlus {
				p.src = p.src[1:]
				if x := p.peek1(); x != t.IDOpenParen {
					return nil, fmt.Errorf(`parse: expected "(", got %q at %s:%d`,
						p.tm.ByID(x), p.filename, p.line())
				}
				extraFields, err := p.parseList(t.IDCloseParen, (*parser).parseExtraFieldNode)
				if err != nil {
					return nil, err
				}
				fields = append(fields, extraFields...)
			}
			if x := p.peek1(); x != t.IDSemicolon {
				got := p.tm.ByID(x)
				return nil, fmt.Errorf(`parse: expected (implicit) ";", got %q at %s:%d`, got, p.filename, p.line())
			}
			p.src = p.src[1:]
			return a.NewStruct(flags, p.filename, line, name, implements, fields).AsNode(), nil
		}
	}
	return nil, fmt.Errorf(`parse: unrecognized top level declaration at %s:%d`, p.filename, line)
}

func (p *parser) parseQualifiedIdentAsTypeExprNode() (*a.Node, error) {
	pkg, name, err := p.parseQualifiedIdent()
	if err != nil {
		return nil, err
	}
	return a.NewTypeExpr(0, pkg, name, nil, nil, nil).AsNode(), nil
}

// parseQualifiedIdent parses "foo.bar" or "bar".
func (p *parser) parseQualifiedIdent() (t.ID, t.ID, error) {
	x, err := p.parseIdent()
	if err != nil {
		return 0, 0, err
	}

	if p.peek1() != t.IDDot {
		return 0, x, nil
	}
	p.src = p.src[1:]

	y, err := p.parseIdent()
	if err != nil {
		return 0, 0, err
	}
	return x, y, nil
}

func (p *parser) parseIdentAsExprNode() (*a.Node, error) {
	id, err := p.parseIdent()
	if err != nil {
		return nil, err
	}
	return a.NewExpr(0, 0, id, nil, nil, nil, nil).AsNode(), nil
}

func (p *parser) parseIdent() (t.ID, error) {
	if len(p.src) == 0 {
		return 0, fmt.Errorf(`parse: expected identifier at %s:%d`, p.filename, p.line())
	}
	x := p.src[0]
	if !x.ID.IsIdent(p.tm) {
		got := p.tm.ByID(x.ID)
		return 0, fmt.Errorf(`parse: expected identifier, got %q at %s:%d`, got, p.filename, p.line())
	}
	p.src = p.src[1:]
	return x.ID, nil
}

func (p *parser) parseList(stop t.ID, parseElem func(*parser) (*a.Node, error)) ([]*a.Node, error) {
	if stop == t.IDCloseParen {
		if x := p.peek1(); x != t.IDOpenParen {
			return nil, fmt.Errorf(`parse: expected "(", got %q at %s:%d`,
				p.tm.ByID(x), p.filename, p.line())
		}
		p.src = p.src[1:]
	}

	ret := []*a.Node(nil)
	for len(p.src) > 0 {
		if id := p.src[0].ID; id == stop {
			if stop == t.IDCloseParen || stop == t.IDCloseBracket {
				p.src = p.src[1:]
			}
			return ret, nil
		} else if (stop == t.IDOpenDoubleCurly) && (id == t.IDOpenCurly) {
			return ret, nil
		}

		elem, err := parseElem(p)
		if err != nil {
			return nil, err
		}
		ret = append(ret, elem)

		switch x := p.peek1(); x {
		case stop:
			if stop == t.IDCloseParen || stop == t.IDCloseBracket {
				p.src = p.src[1:]
			}
			return ret, nil
		case t.IDComma:
			p.src = p.src[1:]
		default:
			return nil, fmt.Errorf(`parse: expected %q, got %q at %s:%d`,
				p.tm.ByID(stop), p.tm.ByID(x), p.filename, p.line())
		}
	}
	return nil, fmt.Errorf(`parse: expected %q at %s:%d`, p.tm.ByID(stop), p.filename, p.line())
}

func (p *parser) parseFieldNode() (*a.Node, error) {
	return p.parseFieldNode1(0)
}

func (p *parser) parseExtraFieldNode() (*a.Node, error) {
	n, err := p.parseFieldNode1(a.FlagsPrivateData)
	if err != nil {
		return nil, err
	}
	typ := n.AsField().XType()
	for typ.Decorator() == t.IDArray {
		typ = typ.Inner()
	}
	if (typ.Decorator() != 0) ||
		(typ.QID()[0] == t.IDBase) && (!typ.IsNumType() || typ.IsRefined()) {

		return nil, fmt.Errorf(`parse: invalid extra-field type %q at %s:%d`,
			n.AsField().XType().Str(p.tm), p.filename, p.line())
	}
	return n, nil
}

func (p *parser) parseFieldNode1(flags a.Flags) (*a.Node, error) {
	name, err := p.parseIdent()
	if err != nil {
		return nil, err
	}
	if x := p.peek1(); x != t.IDColon {
		got := p.tm.ByID(x)
		return nil, fmt.Errorf(`parse: expected ":", got %q at %s:%d`, got, p.filename, p.line())
	}
	p.src = p.src[1:]
	typ, err := p.parseTypeExpr()
	if err != nil {
		return nil, err
	}
	if pkg := typ.Innermost().QID()[0]; (pkg != 0) && (pkg != t.IDBase) {
		flags |= a.FlagsPrivateData
	}
	return a.NewField(flags, name, typ).AsNode(), nil
}

func (p *parser) parseTypeExpr() (*a.TypeExpr, error) {
	if x := p.peek1(); x == t.IDNptr || x == t.IDPtr {
		p.src = p.src[1:]
		rhs, err := p.parseTypeExpr()
		if err != nil {
			return nil, err
		}
		return a.NewTypeExpr(x, 0, 0, nil, nil, rhs), nil
	}

	decorator, arrayLength := t.ID(0), (*a.Expr)(nil)
	switch peek1 := p.peek1(); peek1 {
	case t.IDArray, t.IDRoarray:
		decorator = peek1
		p.src = p.src[1:]

		if x := p.peek1(); x != t.IDOpenBracket {
			got := p.tm.ByID(x)
			return nil, fmt.Errorf(`parse: expected "[", got %q at %s:%d`, got, p.filename, p.line())
		}
		p.src = p.src[1:]

		var err error
		arrayLength, err = p.parseExpr()
		if err != nil {
			return nil, err
		}

		if x := p.peek1(); x != t.IDCloseBracket {
			got := p.tm.ByID(x)
			return nil, fmt.Errorf(`parse: expected "]", got %q at %s:%d`, got, p.filename, p.line())
		}
		p.src = p.src[1:]

	case t.IDRoslice, t.IDRotable, t.IDSlice, t.IDTable:
		decorator = peek1
		p.src = p.src[1:]
	}

	if decorator != 0 {
		rhs, err := p.parseTypeExpr()
		if err != nil {
			return nil, err
		}
		return a.NewTypeExpr(decorator, 0, 0, arrayLength.AsNode(), nil, rhs), nil
	}

	pkg, name, err := p.parseQualifiedIdent()
	if err != nil {
		return nil, err
	}

	lhs, mhs := (*a.Expr)(nil), (*a.Expr)(nil)
	if p.peek1() == t.IDOpenBracket {
		_, lhs, mhs, err = p.parseBracket(t.IDDotDotEq)
		if err != nil {
			return nil, err
		}
		if name.IsNumType() &&
			((pkg == t.IDBase) || ((pkg == 0) && p.opts.AllowBuiltInNames)) {
			// No-op.
		} else {
			return nil, fmt.Errorf(`parse: cannot refine non-numeric type at %s:%d`, p.filename, p.line())
		}
	}

	return a.NewTypeExpr(0, pkg, name, lhs.AsNode(), mhs, nil), nil
}

// parseBracket parses "[i .. j]", "[i ..]", "[.. j]" and "[..]". A "..="
// replaces the ".." if sep is t.IDDotDotEq instead of t.IDDotDot. If sep is
// t.IDDotDot, it also parses "[x]". The returned op is sep for a range or
// refinement and t.IDOpenBracket for an index.
func (p *parser) parseBracket(sep t.ID) (op t.ID, ei *a.Expr, ej *a.Expr, err error) {
	if x := p.peek1(); x != t.IDOpenBracket {
		got := p.tm.ByID(x)
		return 0, nil, nil, fmt.Errorf(`parse: expected "[", got %q at %s:%d`, got, p.filename, p.line())
	}
	p.src = p.src[1:]

	if p.peek1() != sep {
		ei, err = p.parseExpr()
		if err != nil {
			return 0, nil, nil, err
		}
	}

	switch x := p.peek1(); {
	case x == sep:
		p.src = p.src[1:]

	case x == t.IDCloseBracket && sep == t.IDDotDot:
		p.src = p.src[1:]
		return a.ExprOperatorIndex, nil, ei, nil

	default:
		extra := ``
		if sep == t.IDDotDot {
			extra = ` or "]"`
		}
		got := p.tm.ByID(x)
		return 0, nil, nil, fmt.Errorf(`parse: expected %q%s, got %q at %s:%d`,
			p.tm.ByID(sep), extra, got, p.filename, p.line())
	}

	if p.peek1() != t.IDCloseBracket {
		ej, err = p.parseExpr()
		if err != nil {
			return 0, nil, nil, err
		}
	}

	if x := p.peek1(); x != t.IDCloseBracket {
		got := p.tm.ByID(x)
		return 0, nil, nil, fmt.Errorf(`parse: expected "]", got %q at %s:%d`, got, p.filename, p.line())
	}
	p.src = p.src[1:]

	return sep, ei, ej, nil
}

func (p *parser) parseBlock(doubleCurly bool) ([]*a.Node, error) {
	if doubleCurly {
		if x := p.peek1(); x != t.IDOpenDoubleCurly {
			got := p.tm.ByID(x)
			return nil, fmt.Errorf(`parse: expected "{{", got %q at %s:%d`, got, p.filename, p.line())
		}
	} else {
		if x := p.peek1(); x != t.IDOpenCurly {
			got := p.tm.ByID(x)
			return nil, fmt.Errorf(`parse: expected "{", got %q at %s:%d`, got, p.filename, p.line())
		}
	}
	p.src = p.src[1:]

	block := []*a.Node(nil)
	for {
		if len(p.src) == 0 {
			return nil, fmt.Errorf(`parse: expected "}" or "}}" at %s:%d`, p.filename, p.line())
		}

		if doubleCurly {
			if p.src[0].ID == t.IDCloseDoubleCurly {
				break
			}
		} else {
			if p.src[0].ID == t.IDCloseCurly {
				break
			}
		}

		s, err := p.parseStatement()
		if err != nil {
			return nil, err
		}
		block = append(block, s)

		if x := p.peek1(); x != t.IDSemicolon {
			got := p.tm.ByID(x)
			return nil, fmt.Errorf(`parse: expected (implicit) ";", got %q at %s:%d`, got, p.filename, p.line())
		}
		p.src = p.src[1:]
	}

	p.src = p.src[1:]
	return block, nil
}

func (p *parser) assertsSorted(asserts []*a.Node, allowChoose bool) error {
	seenPre, seenInv, seenPost := false, false, false
	for _, o := range asserts {
		switch o.AsAssert().Keyword() {
		case t.IDAssert:
			return fmt.Errorf(`parse: assertion chain cannot contain "assert", `+
				`only "pre", "inv" and "post" at %s:%d`, p.filename, p.line())
		case t.IDChoose:
			if !allowChoose {
				return fmt.Errorf(`parse: invalid "choose" at %s:%d`, p.filename, p.line())
			}
			if seenPre || seenPost || seenInv {
				break
			}
			continue
		case t.IDPre:
			if seenPost || seenInv {
				break
			}
			seenPre = true
			continue
		case t.IDInv:
			if seenPost {
				break
			}
			seenInv = true
			continue
		default:
			seenPost = true
			continue
		}
		return fmt.Errorf(`parse: assertion chain not in "choose", "pre", "inv", "post" order at %s:%d`,
			p.filename, p.line())
	}
	return nil
}

func (p *parser) parseAssertNode() (*a.Node, error) {
	switch x := p.peek1(); x {
	case t.IDAssert, t.IDChoose, t.IDPre, t.IDInv, t.IDPost:
		p.src = p.src[1:]
		condition, err := p.parseExpr()
		if err != nil {
			return nil, err
		}
		if condition.Effect() != 0 {
			return nil, fmt.Errorf(`parse: assert-condition %q is not effect-free at %s:%d`,
				condition.Str(p.tm), p.filename, p.line())
		}
		reason, args := t.ID(0), []*a.Node(nil)
		if p.peek1() == t.IDVia {
			p.src = p.src[1:]
			reason = p.peek1()
			if !reason.IsDQStrLiteral(p.tm) {
				got := p.tm.ByID(reason)
				return nil, fmt.Errorf(`parse: expected "-string literal, got %q at %s:%d`, got, p.filename, p.line())
			}
			p.src = p.src[1:]
			args, err = p.parseList(t.IDCloseParen, (*parser).parseArgNode)
			if err != nil {
				return nil, err
			}
		}
		return a.NewAssert(x, condition, reason, args).AsNode(), nil
	}
	return nil, fmt.Errorf(`parse: expected "assert", "pre" or "post" at %s:%d`, p.filename, p.line())
}

func (p *parser) parseStatement() (*a.Node, error) {
	line := uint32(0)
	if len(p.src) > 0 {
		line = p.src[0].Line
	}
	n, err := p.parseStatement1()
	if n != nil {
		n.AsRaw().SetFilenameLine(p.filename, line)
		if n.Kind() == a.KIterate {
			for _, o := range n.AsIterate().Assigns() {
				o.AsRaw().SetFilenameLine(p.filename, line)
			}
		}
	}
	return n, err
}

func (p *parser) parseLabel() (t.ID, error) {
	if p.peek1() == t.IDDot {
		p.src = p.src[1:]
		return p.parseIdent()
	}
	return 0, nil
}

func (p *parser) parseStatement1() (*a.Node, error) {
	x := p.peek1()
	if x == t.IDVar {
		if !p.allowVar {
			return nil, fmt.Errorf(`parse: var statement not at the top of a function at %s:%d`,
				p.filename, p.line())
		}
		p.src = p.src[1:]
		return p.parseVarNode()
	}
	p.allowVar = false

	switch x {
	case t.IDAssert:
		return p.parseAssertNode()

	case t.IDBreak, t.IDContinue:
		p.src = p.src[1:]
		label, err := p.parseLabel()
		if err != nil {
			return nil, err
		}

		loop := a.Loop(nil)
		if len(p.loops) == 0 {
			// No-op.
		} else if label == 0 {
			loop = p.loops.Top()
			if loop.Label() != 0 {
				return nil, fmt.Errorf(`parse: unlabeled %s for labeled %s.%s at %s:%d`,
					x.Str(p.tm), loop.Keyword().Str(p.tm), loop.Label().Str(p.tm), p.filename, p.line())
			}
		} else {
			for i := len(p.loops) - 1; i >= 0; i-- {
				if l := p.loops[i]; label == l.Label() {
					loop = l
					break
				}
			}
		}
		if loop == nil {
			sepStr, labelStr := "", ""
			if label != 0 {
				sepStr, labelStr = ".", label.Str(p.tm)
			}
			return nil, fmt.Errorf(`parse: no matching while/iterate statement for %s%s%s at %s:%d`,
				x.Str(p.tm), sepStr, labelStr, p.filename, p.line())
		}

		deep := loop != p.loops.Top()
		if x == t.IDBreak {
			loop.SetHasBreak(deep)
		} else {
			loop.SetHasContinue(deep)
		}
		n := a.NewJump(x, label)
		n.SetJumpTarget(loop)
		return n.AsNode(), nil

	case t.IDChoose:
		p.src = p.src[1:]
		if p.funcEffect.Pure() {
			return nil, fmt.Errorf(`parse: choose within pure function at %s:%d`, p.filename, p.line())
		}
		name, err := p.parseIdent()
		if err != nil {
			return nil, err
		}
		if x := p.peek1(); x != t.IDEq {
			got := p.tm.ByID(x)
			return nil, fmt.Errorf(`parse: expected "=", got %q at %s:%d`, got, p.filename, p.line())
		}
		p.src = p.src[1:]
		if x := p.peek1(); x != t.IDOpenBracket {
			got := p.tm.ByID(x)
			return nil, fmt.Errorf(`parse: expected "[", got %q at %s:%d`, got, p.filename, p.line())
		}
		p.src = p.src[1:]
		args, err := p.parseList(t.IDCloseBracket, (*parser).parseIdentAsExprNode)
		if err != nil {
			return nil, err
		}
		return a.NewChoose(name, args).AsNode(), nil

	case t.IDIOBind, t.IDIOForgetHistory, t.IDIOLimit:
		return p.parseIOManipNode()

	case t.IDIf:
		o, err := p.parseIf()
		return o.AsNode(), err

	case t.IDIterate:
		return p.parseIterateNode()

	case t.IDReturn, t.IDYield:
		p.src = p.src[1:]
		if x == t.IDYield {
			if !p.funcEffect.Coroutine() {
				return nil, fmt.Errorf(`parse: yield within non-coroutine at %s:%d`, p.filename, p.line())
			}
			if p.peek1() != t.IDQuestion {
				return nil, fmt.Errorf(`parse: yield not followed by '?' at %s:%d`, p.filename, p.line())
			}
			p.src = p.src[1:]
		}
		value, err := p.parseExpr()
		if err != nil {
			return nil, err
		}
		if value.Effect().Impure() {
			return nil, fmt.Errorf(`parse: %s an impure expression at %s:%d`,
				x.Str(p.tm), p.filename, p.line())
		}
		if (x == t.IDReturn) && (value.Operator() == 0) {
			if s := p.tm.ByID(value.Ident()); (len(s) > 1) && (s[0] == '"') && (s[1] == '$') {
				return nil, fmt.Errorf(`parse: cannot return a suspension at %s:%d`, p.filename, p.line())
			}
		}
		return a.NewRet(x, value).AsNode(), nil

	case t.IDWhile:
		p.src = p.src[1:]
		label, err := p.parseLabel()
		if err != nil {
			return nil, err
		}
		condition, err := p.parseExpr()
		if err != nil {
			return nil, err
		}
		if condition.Effect() != 0 {
			return nil, fmt.Errorf(`parse: while-condition %q is not effect-free at %s:%d`,
				condition.Str(p.tm), p.filename, p.line())
		}
		asserts, err := p.parseAsserts()
		if err != nil {
			return nil, err
		}

		n := a.NewWhile(label, condition, asserts)
		if !p.loops.Push(n) {
			return nil, fmt.Errorf(`parse: duplicate loop label %s at %s:%d`,
				label.Str(p.tm), p.filename, p.line())
		}
		doubleCurly := p.peek1() == t.IDOpenDoubleCurly
		if doubleCurly && !n.IsWhileTrue() {
			return nil, fmt.Errorf(`parse: double {{ }} while loop condition isn't "true" at %s:%d`,
				p.filename, p.line())
		}
		body, err := p.parseBlock(doubleCurly)
		if err != nil {
			return nil, err
		}
		n.SetBody(body)
		p.loops.Pop()

		if label != 0 {
			seenDotLabel := false
			if p.peek1() == t.IDDot {
				p.src = p.src[1:]
				if p.peek1() == label {
					p.src = p.src[1:]
					seenDotLabel = true
				}
			}
			if !seenDotLabel {
				return nil, fmt.Errorf(`parse: expected .%s at %s:%d`,
					label.Str(p.tm), p.filename, p.line())
			}
		}

		if !doubleCurly {
			// No-op.
		} else if n.HasContinue() {
			return nil, fmt.Errorf(`parse: double {{ }} while loop has explicit continue at %s:%d`,
				p.filename, p.line())
		} else if !a.Terminates(body) {
			return nil, fmt.Errorf(`parse: double {{ }} while loop doesn't terminate at %s:%d`,
				p.filename, p.line())
		}
		return n.AsNode(), nil
	}
	return p.parseAssignNode()
}

func (p *parser) parseAssignNode() (*a.Node, error) {
	lhs := (*a.Expr)(nil)
	rhs, err := p.parseExpr()
	if err != nil {
		return nil, err
	}

	op := p.peek1()
	if op.IsAssign() {
		p.src = p.src[1:]
		lhs = rhs
		if lhs.Effect() != 0 {
			return nil, fmt.Errorf(`parse: assignment LHS %q is not effect-free at %s:%d`,
				lhs.Str(p.tm), p.filename, p.line())
		}

		for l := lhs; l != nil; l = l.LHS().AsExpr() {
			switch l.Operator() {
			case 0:
				if id := l.Ident(); id.IsLiteral(p.tm) {
					return nil, fmt.Errorf(`parse: assignment LHS %q is a literal at %s:%d`,
						l.Str(p.tm), p.filename, p.line())
				} else if id.IsCannotAssignTo() {
					if l == lhs {
						return nil, fmt.Errorf(`parse: cannot assign to %q at %s:%d`,
							id.Str(p.tm), p.filename, p.line())
					}
					if !p.funcEffect.Impure() {
						return nil, fmt.Errorf(`parse: cannot assign to %q in a pure function at %s:%d`,
							lhs.Str(p.tm), p.filename, p.line())
					}
				}
			case t.IDDot, t.IDOpenBracket:
				// No-op.
			default:
				return nil, fmt.Errorf(`parse: invalid assignment LHS %q at %s:%d`,
					lhs.Str(p.tm), p.filename, p.line())
			}
		}

		rhs, err = p.parseExpr()
		if err != nil {
			return nil, err
		}

		if op == t.IDEqQuestion {
			if (rhs.Operator() != a.ExprOperatorCall) || (!rhs.Effect().Coroutine()) {
				return nil, fmt.Errorf(`parse: expected ?-function call after "=?", got %q at %s:%d`,
					rhs.Str(p.tm), p.filename, p.line())
			}
		}
	} else {
		op = t.IDEq
	}

	if p.funcEffect.WeakerThan(rhs.Effect()) {
		return nil, fmt.Errorf(`parse: value %q's effect %q is stronger than the func's effect %q at %s:%d`,
			rhs.Str(p.tm), rhs.Effect(), p.funcEffect, p.filename, p.line())
	}

	return a.NewAssign(op, lhs, rhs).AsNode(), nil
}

func (p *parser) parseIterateAssignNode() (*a.Node, error) {
	n, err := p.parseAssignNode()
	if err != nil {
		return nil, err
	}
	o := n.AsAssign()
	if op := o.Operator(); op != t.IDEq {
		return nil, fmt.Errorf(`parse: expected "=", got %q at %s:%d`, op.Str(p.tm), p.filename, p.line())
	}
	if lhs := o.LHS(); lhs.Operator() != 0 {
		return nil, fmt.Errorf(`parse: expected variable, got %q at %s:%d`, lhs.Str(p.tm), p.filename, p.line())
	}
	if rhs := o.RHS(); rhs.Effect() != 0 {
		return nil, fmt.Errorf(`parse: value %q is not effect-free at %s:%d`, rhs.Str(p.tm), p.filename, p.line())
	}
	return o.AsNode(), nil
}

func (p *parser) parseAsserts() ([]*a.Node, error) {
	asserts := []*a.Node(nil)
	if p.peek1() == t.IDComma {
		p.src = p.src[1:]
		var err error
		if asserts, err = p.parseList(t.IDOpenDoubleCurly, (*parser).parseAssertNode); err != nil {
			return nil, err
		}
		if err := p.assertsSorted(asserts, false); err != nil {
			return nil, err
		}
	}
	return asserts, nil
}

func (p *parser) parseIOManipNode() (*a.Node, error) {
	keyword := p.peek1()
	p.src = p.src[1:]

	if x := p.peek1(); x != t.IDOpenParen {
		got := p.tm.ByID(x)
		return nil, fmt.Errorf(`parse: expected "(", got %q at %s:%d`, got, p.filename, p.line())
	}
	p.src = p.src[1:]

	if x := p.peek1(); x != t.IDIO {
		got := p.tm.ByID(x)
		return nil, fmt.Errorf(`parse: expected "io", got %q at %s:%d`, got, p.filename, p.line())
	}
	p.src = p.src[1:]

	if x := p.peek1(); x != t.IDColon {
		got := p.tm.ByID(x)
		return nil, fmt.Errorf(`parse: expected ":", got %q at %s:%d`, got, p.filename, p.line())
	}
	p.src = p.src[1:]

	io, err := p.parseExpr()
	if err != nil {
		return nil, err
	}
	if io.Effect() != 0 {
		return nil, fmt.Errorf(`parse: argument %q is not effect-free at %s:%d`,
			io.Str(p.tm), p.filename, p.line())
	}

	arg1Name := t.ID(0)
	switch keyword {
	case t.IDIOBind:
		arg1Name = t.IDData
		if io.Operator() != 0 {
			return nil, fmt.Errorf(`parse: invalid %s argument %q at %s:%d`,
				keyword.Str(p.tm), io.Str(p.tm), p.filename, p.line())
		}
	case t.IDIOForgetHistory:
		// No-op.
	case t.IDIOLimit:
		arg1Name = t.IDLimit
		if (io.Operator() != 0) && (io.IsArgsDotFoo() == 0) {
			return nil, fmt.Errorf(`parse: invalid %s argument %q at %s:%d`,
				keyword.Str(p.tm), io.Str(p.tm), p.filename, p.line())
		}
	}

	arg1 := (*a.Expr)(nil)
	if arg1Name != 0 {
		if x := p.peek1(); x != t.IDComma {
			got := p.tm.ByID(x)
			return nil, fmt.Errorf(`parse: expected ",", got %q at %s:%d`, got, p.filename, p.line())
		}
		p.src = p.src[1:]

		if x := p.peek1(); x != arg1Name {
			got := p.tm.ByID(x)
			return nil, fmt.Errorf(`parse: expected %q, got %q at %s:%d`, arg1Name.Str(p.tm), got, p.filename, p.line())
		}
		p.src = p.src[1:]

		if x := p.peek1(); x != t.IDColon {
			got := p.tm.ByID(x)
			return nil, fmt.Errorf(`parse: expected ":", got %q at %s:%d`, got, p.filename, p.line())
		}
		p.src = p.src[1:]

		arg1, err = p.parseExpr()
		if err != nil {
			return nil, err
		}
		if arg1.Effect() != 0 {
			return nil, fmt.Errorf(`parse: argument %q is not effect-free at %s:%d`,
				io.Str(p.tm), p.filename, p.line())
		}
	}

	histPos := (*a.Expr)(nil)
	if keyword == t.IDIOBind {
		if x := p.peek1(); x != t.IDComma {
			got := p.tm.ByID(x)
			return nil, fmt.Errorf(`parse: expected ",", got %q at %s:%d`, got, p.filename, p.line())
		}
		p.src = p.src[1:]

		if x := p.peek1(); x != t.IDHistoryPosition {
			got := p.tm.ByID(x)
			return nil, fmt.Errorf(`parse: expected "history_position", got %q at %s:%d`, got, p.filename, p.line())
		}
		p.src = p.src[1:]

		if x := p.peek1(); x != t.IDColon {
			got := p.tm.ByID(x)
			return nil, fmt.Errorf(`parse: expected ":", got %q at %s:%d`, got, p.filename, p.line())
		}
		p.src = p.src[1:]

		histPos, err = p.parseExpr()
		if err != nil {
			return nil, err
		}
		if histPos.Effect() != 0 {
			return nil, fmt.Errorf(`parse: argument %q is not effect-free at %s:%d`,
				io.Str(p.tm), p.filename, p.line())
		}
	}

	if x := p.peek1(); x != t.IDCloseParen {
		got := p.tm.ByID(x)
		return nil, fmt.Errorf(`parse: expected ")", got %q at %s:%d`, got, p.filename, p.line())
	}
	p.src = p.src[1:]

	body, err := p.parseBlock(false)
	if err != nil {
		return nil, err
	}

	return a.NewIOManip(keyword, io, arg1, histPos, body).AsNode(), nil
}

func (p *parser) parseIf() (*a.If, error) {
	if x := p.peek1(); x != t.IDIf {
		got := p.tm.ByID(x)
		return nil, fmt.Errorf(`parse: expected "if", got %q at %s:%d`, got, p.filename, p.line())
	}
	p.src = p.src[1:]
	likelihood, err := p.parseLabel()
	if err != nil {
		return nil, err
	}
	switch likelihood {
	case 0, t.IDLikely, t.IDUnlikely:
	default:
		got := p.tm.ByID(likelihood)
		return nil, fmt.Errorf(`parse: expected "if.likely" or "if.unlikely", got %q at %s:%d`, "if."+got, p.filename, p.line())
	}
	condition, err := p.parseExpr()
	if err != nil {
		return nil, err
	}
	if condition.Effect() != 0 {
		return nil, fmt.Errorf(`parse: if-condition %q is not effect-free at %s:%d`,
			condition.Str(p.tm), p.filename, p.line())
	}
	bodyIfTrue, err := p.parseBlock(false)
	if err != nil {
		return nil, err
	}
	elseIf, bodyIfFalse := (*a.If)(nil), ([]*a.Node)(nil)
	if p.peek1() == t.IDElse {
		p.src = p.src[1:]
		if p.peek1() == t.IDIf {
			elseIf, err = p.parseIf()
			if err != nil {
				return nil, err
			}
		} else {
			bodyIfFalse, err = p.parseBlock(false)
			if err != nil {
				return nil, err
			}
		}
	}
	return a.NewIf(likelihood, condition, bodyIfTrue, bodyIfFalse, elseIf), nil
}

func (p *parser) parseIterateNode() (*a.Node, error) {
	if p.funcEffect.Coroutine() {
		return nil, fmt.Errorf(`parse: "iterate" inside coroutine at %s:%d`, p.filename, p.line())
	} else if x := p.peek1(); x != t.IDIterate {
		got := p.tm.ByID(x)
		return nil, fmt.Errorf(`parse: expected "iterate", got %q at %s:%d`, got, p.filename, p.line())
	}
	p.src = p.src[1:]
	label, err := p.parseLabel()
	if err != nil {
		return nil, err
	}
	assigns, err := p.parseList(t.IDCloseParen, (*parser).parseIterateAssignNode)
	if err != nil {
		return nil, err
	}
	n, err := p.parseIterateBlock(label, assigns)
	if err != nil {
		return nil, err
	}
	return n.AsNode(), nil
}

func (p *parser) parseIterateBlock(label t.ID, assigns []*a.Node) (*a.Iterate, error) {
	if x := p.peek1(); x != t.IDOpenParen {
		got := p.tm.ByID(x)
		return nil, fmt.Errorf(`parse: expected "(", got %q at %s:%d`, got, p.filename, p.line())
	}
	p.src = p.src[1:]

	if x := p.peek1(); x != t.IDLength {
		got := p.tm.ByID(x)
		return nil, fmt.Errorf(`parse: expected "length", got %q at %s:%d`, got, p.filename, p.line())
	}
	p.src = p.src[1:]

	if x := p.peek1(); x != t.IDColon {
		got := p.tm.ByID(x)
		return nil, fmt.Errorf(`parse: expected ":", got %q at %s:%d`, got, p.filename, p.line())
	}
	p.src = p.src[1:]

	length := p.peek1()
	lengthInt := asSmallPositiveInt256(p.tm, length)
	if lengthInt == 0 {
		return nil, fmt.Errorf(`parse: expected length count in [1 ..= 256], got %q at %s:%d`,
			p.tm.ByID(length), p.filename, p.line())
	}
	p.src = p.src[1:]

	if x := p.peek1(); x != t.IDComma {
		got := p.tm.ByID(x)
		return nil, fmt.Errorf(`parse: expected ",", got %q at %s:%d`, got, p.filename, p.line())
	}
	p.src = p.src[1:]

	if x := p.peek1(); x != t.IDAdvance {
		got := p.tm.ByID(x)
		return nil, fmt.Errorf(`parse: expected "advance", got %q at %s:%d`, got, p.filename, p.line())
	}
	p.src = p.src[1:]

	if x := p.peek1(); x != t.IDColon {
		got := p.tm.ByID(x)
		return nil, fmt.Errorf(`parse: expected ":", got %q at %s:%d`, got, p.filename, p.line())
	}
	p.src = p.src[1:]

	advance := p.peek1()
	advanceInt := asSmallPositiveInt256(p.tm, advance)
	if advanceInt == 0 {
		return nil, fmt.Errorf(`parse: expected advance count in [1 ..= 256], got %q at %s:%d`,
			p.tm.ByID(advance), p.filename, p.line())
	} else if advanceInt > lengthInt {
		return nil, fmt.Errorf(`parse: advance %d is larger than length %d at %s:%d`,
			advanceInt, lengthInt, p.filename, p.line())
	}
	p.src = p.src[1:]

	if x := p.peek1(); x != t.IDComma {
		got := p.tm.ByID(x)
		return nil, fmt.Errorf(`parse: expected ",", got %q at %s:%d`, got, p.filename, p.line())
	}
	p.src = p.src[1:]

	if x := p.peek1(); x != t.IDUnroll {
		got := p.tm.ByID(x)
		return nil, fmt.Errorf(`parse: expected "unroll", got %q at %s:%d`, got, p.filename, p.line())
	}
	p.src = p.src[1:]

	if x := p.peek1(); x != t.IDColon {
		got := p.tm.ByID(x)
		return nil, fmt.Errorf(`parse: expected ":", got %q at %s:%d`, got, p.filename, p.line())
	}
	p.src = p.src[1:]

	unroll := p.peek1()
	if asSmallPositiveInt256(p.tm, unroll) == 0 {
		return nil, fmt.Errorf(`parse: expected unroll count in [1 ..= 256], got %q at %s:%d`,
			p.tm.ByID(unroll), p.filename, p.line())
	}
	p.src = p.src[1:]

	if x := p.peek1(); x != t.IDCloseParen {
		got := p.tm.ByID(x)
		return nil, fmt.Errorf(`parse: expected ")", got %q at %s:%d`, got, p.filename, p.line())
	}
	p.src = p.src[1:]

	asserts, err := p.parseAsserts()
	if err != nil {
		return nil, err
	}
	n := a.NewIterate(label, assigns, length, advance, unroll, asserts)
	// TODO: decide how break/continue work with iterate loops.
	if !p.loops.Push(n) {
		return nil, fmt.Errorf(`parse: duplicate loop label %s at %s:%d`,
			label.Str(p.tm), p.filename, p.line())
	}
	body, err := p.parseBlock(false)
	if err != nil {
		return nil, err
	}
	n.SetBody(body)
	p.loops.Pop()

	if x := p.peek1(); x == t.IDElse {
		p.src = p.src[1:]
		elseIterate, err := p.parseIterateBlock(0, nil)
		if err != nil {
			return nil, err
		}
		n.SetElseIterate(elseIterate)
	}

	return n, nil
}

// asSmallPositiveInt256 returns id's value when id is a numeric literal in the
// range [1 ..= 256]. Otherwise, it returns 0.
func asSmallPositiveInt256(tm *t.Map, id t.ID) int {
	if !id.IsNumLiteral(tm) {
		return 0
	}
	s := id.Str(tm)
	if (len(s) > 3) || (len(s) == 0) || (s[0] < '1') || ('9' < s[0]) {
		return 0
	}
	n, s := int(s[0]-'0'), s[1:]
	for ; len(s) > 0; s = s[1:] {
		if (s[0] < '0') || ('9' < s[0]) {
			return 0
		}
		n = (10 * n) + int(s[0]-'0')
	}
	if n > 256 {
		return 0
	}
	return n
}

func (p *parser) parseArgNode() (*a.Node, error) {
	name, err := p.parseIdent()
	if err != nil {
		return nil, err
	}
	if x := p.peek1(); x != t.IDColon {
		got := p.tm.ByID(x)
		return nil, fmt.Errorf(`parse: expected ":", got %q at %s:%d`, got, p.filename, p.line())
	}
	p.src = p.src[1:]
	value, err := p.parseExpr()
	if err != nil {
		return nil, err
	}
	if value.Effect() != 0 {
		return nil, fmt.Errorf(`parse: arg-value %q is not effect-free at %s:%d`,
			value.Str(p.tm), p.filename, p.line())
	}
	return a.NewArg(name, value).AsNode(), nil
}

func (p *parser) parseVarNode() (*a.Node, error) {
	id, err := p.parseIdent()
	if err != nil {
		return nil, err
	}
	if x := p.peek1(); x != t.IDColon {
		got := p.tm.ByID(x)
		return nil, fmt.Errorf(`parse: expected ":", got %q at %s:%d`, got, p.filename, p.line())
	}
	p.src = p.src[1:]
	typ, err := p.parseTypeExpr()
	if err != nil {
		return nil, err
	}
	return a.NewVar(id, typ).AsNode(), nil
}

func (p *parser) parsePossibleListExprNode() (*a.Node, error) {
	n, err := p.parsePossibleListExpr()
	if err != nil {
		return nil, err
	}
	return n.AsNode(), err
}

func (p *parser) parsePossibleListExpr() (*a.Expr, error) {
	// TODO: put the [ and ] parsing into parseExpr.
	if x := p.peek1(); x != t.IDOpenBracket {
		return p.parseExpr()
	}
	p.src = p.src[1:]
	args, err := p.parseList(t.IDCloseBracket, (*parser).parsePossibleListExprNode)
	if err != nil {
		return nil, err
	}
	return a.NewExpr(0, a.ExprOperatorList, 0, nil, nil, nil, args), nil
}

func (p *parser) parseExpr() (*a.Expr, error) {
	e, err := p.parseExpr1()
	if err != nil {
		return nil, err
	}
	if e.SubExprHasEffect() {
		return nil, fmt.Errorf(`parse: expression %q has an effect-ful sub-expression at %s:%d`,
			e.Str(p.tm), p.filename, p.line())
	}
	return e, nil
}

func (p *parser) parseExpr1() (*a.Expr, error) {
	lhs, err := p.parseOperand()
	if err != nil {
		return nil, err
	}
	if x := p.peek1(); x.IsBinaryOp() {
		p.src = p.src[1:]
		rhs := (*a.Node)(nil)
		if x == t.IDAs {
			o, err := p.parseTypeExpr()
			if err != nil {
				return nil, err
			}
			rhs = o.AsNode()
		} else {
			o, err := p.parseOperand()
			if err != nil {
				return nil, err
			}
			rhs = o.AsNode()
		}

		if !x.IsAssociativeOp() || x != p.peek1() {
			op := x.BinaryForm()
			if op == 0 {
				return nil, fmt.Errorf(`parse: internal error: no binary form for token 0x%02X`, x)
			}
			return a.NewExpr(0, op, 0, lhs.AsNode(), nil, rhs, nil), nil
		}

		args := []*a.Node{lhs.AsNode(), rhs}
		for p.peek1() == x {
			p.src = p.src[1:]
			arg, err := p.parseOperand()
			if err != nil {
				return nil, err
			}
			args = append(args, arg.AsNode())
		}
		op := x.AssociativeForm()
		if op == 0 {
			return nil, fmt.Errorf(`parse: internal error: no associative form for token 0x%02X`, x)
		}
		return a.NewExpr(0, op, 0, nil, nil, nil, args), nil
	}
	return lhs, nil
}

func (p *parser) parseOperand() (*a.Expr, error) {
	switch x := p.peek1(); {
	case x.IsUnaryOp():
		p.src = p.src[1:]
		rhs, err := p.parseOperand()
		if err != nil {
			return nil, err
		}
		op := x.UnaryForm()
		if op == 0 {
			return nil, fmt.Errorf(`parse: internal error: no unary form for token 0x%02X`, x)
		}
		return a.NewExpr(0, op, 0, nil, nil, rhs.AsNode(), nil), nil

	case x.IsLiteral(p.tm):
		p.src = p.src[1:]
		return a.NewExpr(0, 0, x, nil, nil, nil, nil), nil

	case x == t.IDOpenParen:
		p.src = p.src[1:]
		expr, err := p.parseExpr()
		if err != nil {
			return nil, err
		}
		if x := p.peek1(); x != t.IDCloseParen {
			got := p.tm.ByID(x)
			return nil, fmt.Errorf(`parse: expected ")", got %q at %s:%d`, got, p.filename, p.line())
		}
		p.src = p.src[1:]
		return expr, nil
	}

	id, err := p.parseIdent()
	if err != nil {
		return nil, err
	}
	lhs := a.NewExpr(0, 0, id, nil, nil, nil, nil)

	for first := true; ; first = false {
		flags := a.Flags(0)
		switch p.peek1() {
		default:
			return lhs, nil

		case t.IDExclam, t.IDQuestion:
			flags |= p.parseEffect().AsFlags()
			fallthrough

		case t.IDOpenParen:
			args, err := p.parseList(t.IDCloseParen, (*parser).parseArgNode)
			if err != nil {
				return nil, err
			}
			lhs = a.NewExpr(flags, a.ExprOperatorCall, 0, lhs.AsNode(), nil, nil, args)

		case t.IDOpenBracket:
			id0, mhs, rhs, err := p.parseBracket(t.IDDotDot)
			if err != nil {
				return nil, err
			}
			lhs = a.NewExpr(0, id0, 0, lhs.AsNode(), mhs.AsNode(), rhs.AsNode(), nil)

		case t.IDDot:
			p.src = p.src[1:]
			selector := p.peek1()
			if first && selector.IsDQStrLiteral(p.tm) {
				p.src = p.src[1:]
			} else {
				selector, err = p.parseIdent()
				if err != nil {
					return nil, err
				}
			}
			lhs = a.NewExpr(0, a.ExprOperatorSelector, selector, lhs.AsNode(), nil, nil, nil)
		}
	}
}

func (p *parser) parseEffect() a.Effect {
	switch p.peek1() {
	case t.IDExclam:
		p.src = p.src[1:]
		return a.EffectImpure
	case t.IDQuestion:
		p.src = p.src[1:]
		return a.EffectImpureCoroutine
	}
	return 0
}
