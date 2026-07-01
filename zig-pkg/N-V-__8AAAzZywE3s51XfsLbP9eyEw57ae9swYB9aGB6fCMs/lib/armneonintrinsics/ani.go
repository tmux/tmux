// Copyright 2021 The Wuffs Authors.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

// ----------------

//go:generate go run gen.go

// Package armneonintrinsics is a database of ARM Neon intrinsics - a list of C
// function signatures. It knows each function's name, return type and their
// arguments' names and types.
//
// It does not call or run those intrinsic functions (or the underlying CPU
// ops). It just lists their signatures (in a structured way, not as strings)
// so that other tools can more easily generate or bind to those C functions.
//
// See the example for more detail.
package armneonintrinsics

type element struct {
	stringTableOffset      uint16
	stringTableLengthFinal uint8 // The 0x80 bit indicates the final element.
	typ                    Type
}

func (e element) isFinal() bool {
	return (e.stringTableLengthFinal & 0x80) != 0
}

func (e element) name() string {
	i := int(e.stringTableOffset)
	j := int(e.stringTableOffset) + int(e.stringTableLength())
	return stringTable[i:j]
}

func (e element) stringTableLength() uint8 {
	return e.stringTableLengthFinal & 0x7F
}

// Argument is an intrinsic function's argument.
type Argument struct {
	index uint32
}

func (a Argument) IsValid() bool { return a.index != 0 }
func (a Argument) Name() string  { return elementTable[a.index].name() }
func (a Argument) Type() Type    { return elementTable[a.index].typ }

// NextArgument iterates through a function's arguments in call order. It
// returns an invalid Argument if called on the final argument.
func (a Argument) NextArgument() Argument {
	if (a.index == 0) || elementTable[a.index].isFinal() {
		return Argument{}
	}
	return Argument{a.index + 1}
}

// Argument is an intrinsic function.
type Function struct {
	index uint32
}

// FindFunction looks up a function by name, like "vsetq_lane_u8". It returns
// an invalid Function if no function has that name.
func FindFunction(cFuncName string) Function {
	hash := jenkins(cFuncName) & uint32(len(hashTable)-1)
	for {
		h := uint32(hashTable[hash])
		if h == 0 {
			break
		}
		if cFuncName == elementTable[h].name() {
			return Function{h}
		}
		hash = (hash + 1) & uint32(len(hashTable)-1)
	}
	return Function{}
}

// FirstFunction returns the first function (in alphabetical order). Calling
// NextFunction can then iterate through the remaining functions.
func FirstFunction() Function {
	return Function{1}
}

func (f Function) IsValid() bool    { return f.index != 0 }
func (f Function) Name() string     { return elementTable[f.index].name() }
func (f Function) ReturnType() Type { return elementTable[f.index].typ }

// NumArguments returns f's argument count.
func (f Function) NumArguments() int {
	n := 0
	for a := f.FirstArgument(); a.IsValid(); a = a.NextArgument() {
		n++
	}
	return n
}

// FirstArgument returns f's first argument. Calling NextArgument can then
// iterate through the remaining arguments. It returns an invalid Argument if f
// has no arguments.
func (f Function) FirstArgument() Argument {
	if (f.index == 0) || elementTable[f.index].isFinal() {
		return Argument{}
	}
	return Argument{f.index + 1}
}

// NextFunction iterates through the known Functions in alphabetical order. It
// returns an invalid Function if called on the final Function.
func (f Function) NextFunction() Function {
	if f.index == 0 {
		return Function{}
	}
	i := f.index
	for {
		i++
		if !elementTable[i-1].isFinal() {
			continue
		} else if int(i) < len(elementTable) {
			return Function{i}
		}
		break
	}
	return Function{}
}

// String returns the C function signature like "int8x8_t vaba_s8(int8x8_t __a,
// int8x8_t __b, int8x8_t __c)".
func (f Function) String() string {
	if f.index == 0 {
		return "no_such_function"
	}
	i := f.index
	elem := elementTable[i]
	b := make([]byte, 0, 256)
	b = elem.typ.appendString(b)
	b = append(b, ' ')
	b = append(b, elem.name()...)
	b = append(b, '(')
	for comma := false; !elem.isFinal(); comma = true {
		if comma {
			b = append(b, ", "...)
		}
		i++
		elem = elementTable[i]
		b = elem.typ.appendString(b)
		b = append(b, ' ')
		b = append(b, elem.name()...)
	}
	b = append(b, ')')
	return string(b)
}

// jenkins implements https://en.wikipedia.org/wiki/Jenkins_hash_function
func jenkins(s string) (hash uint32) {
	for i := 0; i < len(s); i++ {
		hash += uint32(s[i])
		hash += hash << 10
		hash ^= hash >> 6
	}
	hash += hash << 3
	hash ^= hash >> 11
	hash += hash << 15
	return hash
}
