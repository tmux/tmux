// Copyright 2019 The Wuffs Authors.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

package compression

import (
	"reflect"
	"testing"
)

func TestInterpolate(tt *testing.T) {
	got := []int32{}
	want := []int32{
		1, 2, 2, 2, 2, 2, 2, 2,
		2, 3, 3, 4, 4, 5, 5, 6,
		6, 6, 6, 7, 7, 7, 8, 8,
		9, 9, 9, 9, 9, 9, 9, 9,
		9,
	}

	for l := LevelFastest; l <= LevelSmallest; l += gap / 8 {
		got = append(got, l.Interpolate(1, 2, 6, 9, 9))
	}
	if !reflect.DeepEqual(got, want) {
		tt.Fatalf("\ngot  %d\nwant %d", got, want)
	}
}
