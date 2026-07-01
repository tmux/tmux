// Copyright 2017 The Wuffs Authors.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

package ast

import (
	t "github.com/google/wuffs/lang/token"
)

func TopologicalSortStructs(ns []*Struct) (sorted []*Struct, ok bool) {
	// Algorithm is a depth-first search as per
	// https://en.wikipedia.org/wiki/Topological_sorting#Depth-first_search
	sorted = make([]*Struct, 0, len(ns))
	byQID := map[t.QID]*Struct{}
	for _, n := range ns {
		byQID[n.QID()] = n
	}
	marks := map[*Struct]uint8{}
	for _, n := range ns {
		if _, ok := marks[n]; !ok {
			sorted, ok = tssVisit(sorted, n, byQID, marks)
			if !ok {
				return nil, false
			}
		}
	}
	return sorted, true
}

func tssVisit(dst []*Struct, n *Struct, byQID map[t.QID]*Struct, marks map[*Struct]uint8) ([]*Struct, bool) {
	const (
		unmarked  = 0
		temporary = 1
		permanent = 2
	)
	switch marks[n] {
	case temporary:
		return nil, false
	case permanent:
		return dst, true
	}
	marks[n] = temporary

	for _, f := range n.Fields() {
		x := f.AsField().XType().Innermost()
		if o := byQID[x.QID()]; o != nil {
			var ok bool
			dst, ok = tssVisit(dst, o, byQID, marks)
			if !ok {
				return nil, false
			}
		}
	}

	marks[n] = permanent
	return append(dst, n), true
}
