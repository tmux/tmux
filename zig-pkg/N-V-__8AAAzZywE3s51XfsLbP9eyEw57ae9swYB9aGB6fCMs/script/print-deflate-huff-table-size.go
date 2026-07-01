// Copyright 2019 The Wuffs Authors.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

//go:build ignore
// +build ignore

package main

// print-deflate-huff-table-size.go prints the worst-case number of elements in
// the std/deflate decoder's huffs tables.
//
// Usage: go run print-deflate-huff-table-size.go
//
// This program might take tens of seconds to run.
//
// It is algorithmically similar to examples/enough.c in the zlib C library.
// Much of the initial commentary in that file is also relevant for this
// program. Not all of it applies, though. For example, the Wuffs decoder
// implementation's primary (root) table length is fixed at compile time. It
// does not grow that at run-time, even if the shortest Huffman code is longer
// than that primary length.
//
// This program should print:
//
// ----------------
// --------  Lit/Len (up to 286 symbols, 149142 combos)  @1  @2  @3  @4  @5  @6  @7  @8  @9 @10 @11 @12 @13 @14 @15
// primLen  3:   4376  entries =     8 prim + 4368 seco   1   0   0;  1   1   1  17   1   1 257   1   1   1   1   2
// primLen  4:   2338  entries =    16 prim + 2322 seco   0   0   0   0;  3   1   1 177  97   1   1   1   1   1   2
// primLen  5:   1330  entries =    32 prim + 1298 seco   1   0   0   0   0;  3   1   1 177  97   1   1   1   1   2
// primLen  6:    852  entries =    64 prim +  788 seco   0   0   0   0   0   0;  1 229  49   1   1   1   1   1   2
// primLen  7:    660  entries =   128 prim +  532 seco   1   0   0   0   0   0   0;  1 229  49   1   1   1   1   2
// primLen  8:    660  entries =   256 prim +  404 seco   1   1   0   0   0   0   0   0;  1 229  49   1   1   1   2
// primLen  9:    852  entries =   512 prim +  340 seco   1   1   1   0   0   0   0   0   0;  1 229  49   1   1   2
// primLen 10:   1332  entries =  1024 prim +  308 seco   1   1   1   1   0   0   0   0   0   0;  1 229  49   1   2
// primLen 11:   2340  entries =  2048 prim +  292 seco   1   1   1   1   1   0   0   0   0   0   0;  1 229  49   2
// primLen 12:   4380  entries =  4096 prim +  284 seco   1   1   1   1   1   1   0   0   0   0   0   0;  1 229  50
// primLen 13:   8472  entries =  8192 prim +  280 seco   1   1   1   1   1   1   0   1   0   0   0   0   0;105 174
// primLen 14:  16658  entries = 16384 prim +  274 seco   1   1   1   1   1   1   0   1   1   1   0   1   1   1;274
// primLen 15:  32768  entries = 32768 prim +    0 seco   2   0   0   0   0   0   0   0   0   0   0   0   0   0   0
//
// -------- Distance (up to  30 symbols,   1666 combos)  @1  @2  @3  @4  @5  @6  @7  @8  @9 @10 @11 @12 @13 @14 @15
// primLen  3:   4120  entries =     8 prim + 4112 seco   1   0   0;  1   9   9   1   1   1   1   1   1   1   1   2
// primLen  4:   2080  entries =    16 prim + 2064 seco   1   1   0   0;  1   9   9   1   1   1   1   1   1   1   2
// primLen  5:   1072  entries =    32 prim + 1040 seco   1   1   1   0   0;  1   9   9   1   1   1   1   1   1   2
// primLen  6:    592  entries =    64 prim +  528 seco   1   1   1   1   0   0;  1   9   9   1   1   1   1   1   2
// primLen  7:    400  entries =   128 prim +  272 seco   1   1   1   1   1   0   0;  1   9   9   1   1   1   1   2
// primLen  8:    400  entries =   256 prim +  144 seco   1   1   1   1   1   1   0   0;  1   9   9   1   1   1   2
// primLen  9:    592  entries =   512 prim +   80 seco   1   1   1   1   1   1   1   0   0;  1   9   9   1   1   2
// primLen 10:   1072  entries =  1024 prim +   48 seco   1   1   1   1   1   1   1   1   0   0;  1   9   9   1   2
// primLen 11:   2080  entries =  2048 prim +   32 seco   1   1   1   1   1   1   1   1   1   0   0;  1   9   9   2
// primLen 12:   4120  entries =  4096 prim +   24 seco   1   1   1   1   1   1   1   1   1   1   0   0;  1   9  10
// primLen 13:   8212  entries =  8192 prim +   20 seco   1   1   1   1   1   1   1   1   1   1   0   1   0;  5  14
// primLen 14:  16400  entries = 16384 prim +   16 seco   1   1   1   1   1   1   1   1   1   1   1   0   0   0; 16
// primLen 15:  32768  entries = 32768 prim +    0 seco   2   0   0   0   0   0   0   0   0   0   0   0   0   0   0
// ----------------
//
// Deflate's canonical Huffman codes are up to 15 bits long, but the decoder
// can use fewer than (1 << 15) entries in its look-up table, by splitting it
// into one primary table (of an at-compile-time fixed length <= 15) and zero
// or more secondary tables (of variable lengths, determined at-run-time).
//
// Symbols whose Huffman codes' length are less than or equal to the primary
// table length do not require any secondary tables. Longer codes are grouped
// by their primLen length prefix. Each group occupies one entry in the primary
// table, which redirects to a secondary table of size (1 << (N - primLen)),
// where N is the length of the longest code in that group.
//
// This program calculates that, for a primary table length of 9 bits, the
// worst case Huffman table size is 852 for the Literal/Length table (852 being
// a 512 entry primary table and the secondary tables totalling 340 further
// entries) and 592 (512 + 80) for the Distance table.
//
// Copy/pasting one row of that output (and the column headers):
//
// --------  Lit/Len (up to 286 symbols, 149142 combos)  @1  @2  @3  @4  @5  @6  @7  @8  @9 @10 @11 @12 @13 @14 @15
// primLen  9:    852  entries =   512 prim +  340 seco   1   1   1   0   0   0   0   0   0;  1 229  49   1   1   2
//
// The "@1, @2, @3, ..." columns mean that one combination that hits that 852
// worst case is having 1 1-length code, 1 2-length code, 1 3-length code, 0
// 4-length codes, ..., 1 10-length code, 229 11-length codes, etc. There are a
// total of 286 (1 + 1 + 1 + 0 + ... + 49 + 1 + 1 + 2) codes, for 286 symbols.
//
// See test/data/artificial/deflate-huffman-primlen-9.deflate* for an actual
// example of valid Deflate-formatted data that exercises these worst-case code
// lengths (for a primary table length of 9 bits).
//
// Hypothetically, suppose that we used a primLen of 13 for the Distance table.
// Here is a code length assignment that produces 20 secondary table entries:
//
// -------- Distance (up to  30 symbols,   1666 combos)  @1  @2  @3  @4  @5  @6  @7  @8  @9 @10 @11 @12 @13 @14 @15
// primLen 13:   8212  entries =  8192 prim +   20 seco   1   1   1   1   1   1   1   1   1   1   0   1   0;  5  14
//
// In detail: call the 14 length codes "e0", "e1", ..., "e4" and the 15 length
// codes "f00", "f01", "f02", ..., "f13".
//
// Recall that secondary tables' lengths are (1 << (N - primLen)), where N is
// the longest code for a given primLen length prefix. In this case, (N -
// primLen) is (15 - 13) if the secondary table contains a "f??" code,
// otherwise it N is (14 - 13). When the secondary table contains a mixture of
// lengths, some of the shorter codes' entries are duplicated.
//
// Also, for canonical Huffman codes, the codes (bitstrings) for 14-length
// codes are all assigned (sequentially) before any 15-length codes. There are
// therefore 6 secondary tables:
//
//  - length 2: e0,  e1.
//  - length 2: e2,  e3.
//  - length 4: e4,  e4,  f00, f01.
//  - length 4: f02, f03, f04, f05.
//  - length 4: f06, f07, f08, f09.
//  - length 4: f10, f11, f12, f13.
//
// The total size of the secondary tables is (2 + 2 + 4 + 4 + 4 + 4) = 20.

import (
	"fmt"
	"os"
	"sort"
)

const (
	maxMaxNSyms = 286 // RFC 1951 lists 286 literal/length codes and 30 distance codes.
	maxCodeLen  = 15  // Each code's encoding is at most 15 bits.
)

func main() {
	if err := main1(); err != nil {
		os.Stderr.WriteString(err.Error() + "\n")
		os.Exit(1)
	}
}

func main1() error {
	do("Lit/Len", 286)
	do("Distance", 30)
	return nil
}

func do(name string, maxNSyms uint32) {
	ftab := &feasibleTable{}
	ftab.build(maxNSyms)

	// We can do the per-primLen computations concurrently, speeding up the
	// overall wall time taken.
	ch := make(chan string)
	numPending := 0
	for primLen := uint32(3); primLen <= maxCodeLen; primLen++ {
		go doPrimLen(ch, maxNSyms, ftab, primLen)
		numPending++
	}

	// Gather, sort and print the results for each primLen.
	results := make([]string, 0, numPending)
	for ; numPending > 0; numPending-- {
		results = append(results, <-ch)
	}
	sort.Strings(results)
	fmt.Printf("\n-------- %8s (up to %3d symbols, %6d combos)"+
		"  @1  @2  @3  @4  @5  @6  @7  @8  @9 @10 @11 @12 @13 @14 @15\n",
		name, maxNSyms, ftab.count())
	for _, s := range results {
		fmt.Println(s)
	}
}

type state struct {
	ftab    *feasibleTable
	primLen uint32

	currCase struct {
		nAssignedCodes nAssignedCodes
	}

	worstCase struct {
		nAssignedCodes nAssignedCodes
		nSecondary     uint32
	}

	visited [maxCodeLen + 1][maxMaxNSyms + 1][maxMaxNSyms + 1]bitVector
}

func doPrimLen(ch chan<- string, maxNSyms uint32, ftab *feasibleTable, primLen uint32) {
	z := &state{
		ftab:    ftab,
		primLen: primLen,
	}

	if z.primLen > maxCodeLen {
		panic("unreachable")

	} else if z.primLen == maxCodeLen {
		// There's only the primary table and no secondary tables. For an
		// example that fills that primary table, create a trivial encoding of
		// two symbols: one symbol's code is "0", the other's is "1".
		z.worstCase.nAssignedCodes[1] = 2
		for n := uint32(2); n <= maxCodeLen; n++ {
			z.worstCase.nAssignedCodes[n] = 0
		}

	} else {
		// Brute force search the tree of possible code length assignments.
		//
		// nCodes is always even: the number of unassigned Huffman codes at
		// length L is twice the number at length (L-1). At the start, there
		// are two unassigned codes of length 1: "0" and "1".
		//
		// nSyms starts at nCodes, since (nCodes > nSyms) is infeasible: there
		// will be unassigned codes.
		for nCodes := uint32(2); nCodes <= maxNSyms; nCodes += 2 {
			for nSyms := nCodes; nSyms <= maxNSyms; nSyms++ {
				if z.ftab[z.primLen+1][nCodes][nSyms] {
					z.visit(z.primLen+1, nCodes, nSyms, 0, 0)
				}
			}
		}

		z.worstCase.nAssignedCodes.fillPrimaryValues(z.primLen)
	}

	// Collect the details: the number of codes assigned to each length in the
	// worst case.
	details := ""
	for n := uint32(1); n <= maxCodeLen; n++ {
		if n == z.primLen+1 {
			details += ";"
		} else {
			details += " "
		}
		details += fmt.Sprintf("%3d", z.worstCase.nAssignedCodes[n])
	}

	nPrimary := uint32(1) << z.primLen
	nSecondary := z.worstCase.nSecondary
	nEntries := nPrimary + nSecondary

	ch <- fmt.Sprintf("primLen %2d:  %5d  entries = %5d prim + %4d seco%s",
		z.primLen, nEntries, nPrimary, nSecondary, details)
}

// visit updates z.worstCase, starting with nCodes unassigned Huffman codes of
// length codeLen for nSyms symbols. nSecondary is the total size (number of
// entries) so far of all of the secondary tables.
//
// nRemain > 0 iff we have an incomplete secondary table, whose size was
// partially accounted for in nSecondary, by (1 << (codeLen - z.primLen)). If
// positive, nRemain is the number of unassigned entries in that incomplete
// secondary table.
//
// visit can recursively call itself with longer codeLen values. As it does so,
// it temporarily sets z.currCase.nAssignedCodes[codeLen], restoring that to
// zero before it returns.
func (z *state) visit(codeLen uint32, nCodes uint32, nSyms uint32, nSecondary uint32, nRemain uint32) {
	if z.alreadyVisited(codeLen, nCodes, nSyms, nSecondary, nRemain) {
		// No-op.

	} else if nCodes > nSyms {
		// Infeasible: there will be unassigned codes.

	} else if nCodes == nSyms {
		// Fill in the incomplete secondary table (if present) and any new
		// secondary tables (if necessary) to assign the nCodes codes to the
		// nSyms symbols. At the end, we should have no remaining entries.
		n := nCodes
		for n > nRemain {
			n -= nRemain
			nRemain = 1 << (codeLen - z.primLen)
			nSecondary += nRemain
		}
		if n != nRemain {
			panic("inconsistent secondary table size")
		}

		// Update z.worstCase if we have a new record for nSecondary.
		if z.worstCase.nSecondary < nSecondary {
			z.worstCase.nSecondary = nSecondary
			z.worstCase.nAssignedCodes = z.currCase.nAssignedCodes
			z.worstCase.nAssignedCodes[codeLen] = nCodes
		}

	} else { // nCodes < nSyms.
		codeLen1 := codeLen + 1
		// Brute force assigning n codes of this codeLen. The other (nCodes -
		// n) codes will have longer codes.
		for n := uint32(0); n < nCodes; n++ {
			nCodes1 := (nCodes - n) * 2
			nSyms1 := nSyms - n

			if nCodes1 > nSyms1 {
				// Infeasible: there will be unassigned codes.

			} else if z.ftab[codeLen1][nCodes1][nSyms1] {
				nSecondary1 := nSecondary
				nRemain1 := nRemain * 2

				if nRemain1 > 0 {
					// We have an incomplete secondary table, which has been
					// partially accounted for: nSecondary has previously been
					// incremented by X, where X equals (1 << (codeLen -
					// z.primLen)).
					//
					// As we recursively call visit, with codeLen increased by
					// 1, then we need to double the accounted size of that
					// secondary table to 2*X. Since X out of that 2*X has
					// already been added, we need to increment nSecondary1 by
					// just the difference, which is also X.
					nSecondary1 += 1 << (codeLen - z.primLen)
				}

				z.currCase.nAssignedCodes[codeLen] = n
				z.visit(codeLen1, nCodes1, nSyms1, nSecondary1, nRemain1)
				z.currCase.nAssignedCodes[codeLen] = 0
			}

			// Open a new secondary table if necessary: one whose size is (1 <<
			// (codeLen - z.primLen)).
			if nRemain == 0 {
				nRemain = 1 << (codeLen - z.primLen)
				nSecondary += nRemain
			}

			// Assign one of the incomplete secondary table's entries.
			nRemain--
		}
	}
}

// alreadyVisited returns whether z has already visited the (nSecondary,
// nRemain) pair. As a side effect, it also marks that pair as visited.
func (z *state) alreadyVisited(codeLen uint32, nCodes uint32, nSyms uint32, nSecondary uint32, nRemain uint32) bool {
	// Use the zig re-numbering to minimize the memory requirements for the
	// visited slice. There's an additional 8-fold memory reduction by using 1
	// bit per element, not 1 bool (1 byte) per element.
	i := zig(nSecondary/8, nRemain)
	mask := uint8(1) << (nSecondary & 7)

	visited := z.visited[codeLen][nCodes][nSyms]
	if uint64(i) < uint64(len(visited)) {
		x := visited[i]
		visited[i] = x | mask
		return x&mask != 0
	}

	oldDone := visited
	visited = make(bitVector, i+1)
	copy(visited, oldDone)
	visited[i] = mask

	z.visited[codeLen][nCodes][nSyms] = visited
	return false
}

type bitVector []uint8

// zig packs a pair of non-negative integers (i, j) into a single unique
// non-negative integer, in a zig-zagging pattern:
//
// .    i=0 i=1 i=2 i=3 i=4 etc
// j=0:   0   1   3   6  10 ...
// j=1:   2   4   7  11  16 ...
// j=2:   5   8  12  17  23 ...
// j=3:   9  13  18  24  31 ...
// j=4:  14  19  25  32  40 ...
// etc: ... ... ... ... ... ...
//
// The closed from expression relies on the fact that the sum (0 + 1 + ... + n)
// equals ((n * (n + 1)) / 2).
//
// This lets us minimize the memory requirements of a triangular array: one for
// which a[i][j] is only accessed when ((i+j) < someBound).
func zig(i uint32, j uint32) uint32 {
	if (i > 0x4000) || (j > 0x4000) {
		panic("overflow")
	}
	n := i + j
	return ((n * (n + 1)) / 2) + j
}

// nAssignedCodes[i] holds the number of codes of length i.
type nAssignedCodes [maxCodeLen + 1]uint32

// fillPrimaryValues fills in feasible a[n] values for n <= primLen. It assumes
// that the caller has already filled in a[n] for n > primLen.
func (a *nAssignedCodes) fillPrimaryValues(primLen uint32) {
	// Looping backwards from the maxCodeLen, figure out how many unassigned
	// primLen length codes there were.
	//
	// For example, if there were 10 assigned 15 length codes, then there must
	// have been (10 / 2 = 5) unassigned 14 length codes. If there were also 7
	// 14 length codes, then there must have been ((5 + 7) / 2 = 6) unassigned
	// 13 length codes. Etcetera.
	nUnassignedPrimLen := uint32(0)
	for n := uint32(maxCodeLen); n > primLen; n-- {
		nUnassignedPrimLen += a[n]
		if nUnassignedPrimLen%2 != 0 {
			panic("cannot undo Huffman code split")
		}
		nUnassignedPrimLen /= 2
	}
	if nUnassignedPrimLen < 1 {
		panic("not enough unassigned primLen length codes")
	}

	// Pick a combination of assigning shorter length codes (i.e. setting a[n]
	// values) to reach that nUnassignedPrimLen target. There are many possible
	// ways to do so, but only one unique way with the additional constraint
	// that each value is either 0 or 1: unique because every positive number
	// has a unique binary representation.
	//
	// For example, suppose we need to have 12 unassigned primLen length codes.
	// With that additional constraint, the way to do this is:
	//
	//  - 12 unassigned code of length primLen-0.
	//  -  6 unassigned code of length primLen-1.
	//  -  3 unassigned code of length primLen-2.
	//  -  2 unassigned code of length primLen-3.
	//  -  1 unassigned code of length primLen-4.
	//  -  1 unassigned code of length primLen-5.
	//  -  1 unassigned code of length primLen-6.
	//  -  etc
	//
	// Each left hand column value is either ((2*b)-0) or ((2*b)-1), where b is
	// the the number below. Subtracting 0 or 1 means that we assign 0 or 1
	// codes of that row's length:
	//
	//  - 0 assigned code of length primLen-0.
	//  - 0 assigned code of length primLen-1.
	//  - 1 assigned code of length primLen-2.
	//  - 0 assigned code of length primLen-3.
	//  - 1 assigned code of length primLen-4.
	//  - 1 assigned code of length primLen-5.
	//  - 1 assigned code of length primLen-6.
	//  - etc
	//
	// Reading upwards, this is the binary string "...1110100", which is the
	// two's complement representation of the decimal number -12: the negative
	// of nUnassignedPrimLen.
	bits := -int32(nUnassignedPrimLen)
	for n := primLen; n >= 1; n-- {
		a[n] = uint32(bits & 1)
		bits >>= 1
	}
	if bits != -1 {
		panic("did not finish with one unassigned zero-length code")
	}

	// As a coherence check of the "negative two's complement" theory, loop
	// forwards to calculate the number of unassigned primLen length codes,
	// which should match nUnassignedPrimLen.
	nUnassigned := uint32(1)
	for n := uint32(1); n <= primLen; n++ {
		nUnassigned *= 2
		if nUnassigned <= a[n] {
			panic("not enough unassigned codes")
		}
		nUnassigned -= a[n]
	}
	if nUnassigned != nUnassignedPrimLen {
		panic("inconsistent unassigned codes")
	}
}

// feasibleTable[codeLen][nCodes][nSyms] is whether it is feasible to assign
// Huffman codes (of length at least codeLen) to nSyms symbols, given nCodes
// unassigned codes of length codeLen. Each unassigned code of length L can be
// split into 2 codes of length L+1, 4 codes of length L+2, etc, up to a
// maximum code length of maxCodeLen. A feasible assignment ends with zero
// unassigned codes, no more and no less.
type feasibleTable [maxCodeLen + 1][maxMaxNSyms + 1][maxMaxNSyms + 1]bool

func (f *feasibleTable) count() (ret uint64) {
	for i := range *f {
		for j := range f[i] {
			for k := range f[i][j] {
				if f[i][j][k] {
					ret++
				}
			}
		}
	}
	return ret
}

func (f *feasibleTable) build(maxNSyms uint32) {
	for nSyms := uint32(2); nSyms <= maxNSyms; nSyms++ {
		if f.buildMore(1, 2, nSyms) != bmResultOK {
			panic("infeasible [codeLen][nCodes][nSyms] combination")
		}
	}
}

const (
	bmResultOK            = 0
	bmResultUseFewerCodes = 1
	bmResultUseMoreCodes  = 2
	bmResultUnsatisfiable = 3
)

func (f *feasibleTable) buildMore(codeLen uint32, nCodes uint32, nSyms uint32) uint32 {
	if nCodes == nSyms {
		if nCodes%2 == 1 {
			panic("odd nCodes declared feasible")
		}
		// This is trivial: assign every symbol a code of length codeLen.
		f[codeLen][nCodes][nSyms] = true
		return bmResultOK
	} else if nCodes > nSyms {
		// Infeasible: there will be unassigned codes.
		return bmResultUseMoreCodes
	}
	// From here onwards, we have (nCodes < nSyms).

	if (nCodes << (maxCodeLen - codeLen)) < nSyms {
		// Infeasible: there will be unassigned symbols, even if we extend the
		// codes out to maxCodeLen.
		return bmResultUseFewerCodes
	}

	// If we've already visited this [codeLen][nCodes][nSyms] combination,
	// there's no need to re-do the computation.
	if f[codeLen][nCodes][nSyms] {
		return bmResultOK
	}

	// Try assigning n out of the nCodes codes 1-to-1 to a symbol, remembering
	// that we have checked above that (nCodes < nSyms). The remaining (nCodes
	// - n) codes are lengthened by 1, doubling the number of them, and try to
	// assign those longer codes to the remaining (nSyms - n) symbols.
	ok := false
	codeLen1 := codeLen + 1
	for n := uint32(0); n < nCodes; n++ {
		nCodes1 := (nCodes - n) * 2
		nSyms1 := nSyms - n

		if x := f.buildMore(codeLen1, nCodes1, nSyms1); x == bmResultOK {
			ok = true
		} else if x == bmResultUseFewerCodes {
			// This value of n didn't succeed, but also any larger n also won't
			// succeed, so we can break the loop early.
			break
		}
	}

	if !ok {
		return bmResultUnsatisfiable
	}
	if nCodes%2 == 1 {
		panic("odd nCodes declared feasible")
	}
	f[codeLen][nCodes][nSyms] = true
	return bmResultOK
}
