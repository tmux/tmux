// Copyright 2019 The Wuffs Authors.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

package rac

import (
	"io"
)

const (
	numRBuffersPerWorker = 2
	rBufferSize          = 65536
)

type rBuffer [rBufferSize]byte

// rWork is a unit of work for concurrent reading. The Manager sends dRanges
// for Workers to read. Workers send filled buffers to the concReader.
type rWork struct {
	err error

	// dRange is set by the Manager goroutine, for a Worker's incoming work.
	// That Worker may slice that dRange into smaller pieces of outgoing work.
	// Each outgoing piece has a dRange.Size() of at most rBufferSize.
	dRange Range

	// buffer[i:j] holds bytes decompressed from the underlying RAC file but
	// not yet served onwards to concReader.Read's caller.
	//
	// j should equal dRange.Size().
	//
	// When the buffer is done with (e.g. if i == j, or if we've canceled a
	// read-in-progress), the buffer is returned to its owning Worker
	// goroutine via recyclec.
	//
	// These fields are not used by the Manager goroutine.
	buffer   *rBuffer
	i, j     uint32
	recyclec chan<- *rBuffer
}

func (r *rWork) recycle() {
	if (r.recyclec != nil) && (r.buffer != nil) {
		r.recyclec <- r.buffer
		r.recyclec = nil
		r.buffer = nil
		r.i = 0
		r.j = 0
	}
}

// stopWork is a cancel (non-permanent, keepWorking = true) or close
// (permanent, keepWorking = false) notice to the Manager and Workers. The
// recipient needs to acknowledge the request by receiving from ackc (if
// non-nil).
type stopWork struct {
	ackc        <-chan struct{}
	keepWorking bool
}

// concReader co-ordinates multiple goroutines (1 Manager, multiple Workers)
// serving a Reader.
type concReader struct {
	// Channels between the concReader, the Manager and multiple Workers.
	//
	// The concReader sends roic and                recvs resc.
	// The Manager    recvs roic and sends reqc.
	// Each Worker                   recvs reqc and sends resc.
	roic chan Range // Region of Interest channel.
	reqc chan rWork // Work-Request       channel.
	resc chan rWork // Work-Response      channel.

	// stopc and ackc are used to synchronize the concReader, Manager and
	// Workers, either canceling work-in-progress or closing everything down.
	//
	// Importantly, these are unbuffered channels. Sending and receiving will
	// wait for the other end to synchronize.
	stopc chan stopWork
	ackc  chan struct{}

	// currWork holds the unit of work currently being processed by Read. It
	// will be recycled after Read is done with it, or if a Seek causes us to
	// cancel the work-in-progress.
	currWork rWork

	// completedWorks hold completed units of work that are not the next unit
	// to be sent out via Read. Works may arrive out of order.
	//
	// The map is keyed by an rWork's dRange[0].
	completedWorks map[int64]rWork

	// numWorkers is the number of concurrent Workers.
	numWorkers int

	// seekResolved means that Read does not have to seek to pos.
	//
	// Each Seek call is relatively cheap, only changing the pos field. The
	// bulk of the work happens in the first Read call following a Seek call.
	seekResolved bool

	// seenRead means that we've already seen at least one Read call.
	seenRead bool

	// pos is the current position, in DSpace. It is the base value when Seek
	// is called with io.SeekCurrent.
	pos int64

	// posLimit is an upper limit on pos. pos can go higher than it (e.g.
	// seeking past the end of the file in DSpace), but after doing so, Read
	// will always return (0, io.EOF).
	posLimit int64

	// decompressedSize is the size of the RAC file in DSpace.
	decompressedSize int64
}

func (c *concReader) initialize(racReader *Reader) {
	if racReader.Concurrency <= 1 {
		return
	}
	c.numWorkers = racReader.Concurrency
	if c.numWorkers > 65536 {
		c.numWorkers = 65536
	}

	// Set up other state.
	c.completedWorks = map[int64]rWork{}
	c.posLimit = racReader.chunkReader.decompressedSize
	c.decompressedSize = racReader.chunkReader.decompressedSize

	// Set up the Manager and the Workers.
	c.roic = make(chan Range)
	c.reqc = make(chan rWork, c.numWorkers)
	c.resc = make(chan rWork, c.numWorkers*numRBuffersPerWorker)

	// Set up the channels used in stopAnyWorkInProgress. It is important that
	// these are unbuffered, so that communication is also synchronization.
	c.stopc = make(chan stopWork)
	c.ackc = make(chan struct{})

	for i := 0; i < c.numWorkers; i++ {
		rr := racReader.clone()
		rr.Concurrency = 0
		go runRWorker(c.stopc, c.resc, c.reqc, rr)
	}
	go runRManager(c.stopc, c.roic, c.reqc, &racReader.chunkReader)
}

func (c *concReader) ready() bool {
	return c.stopc != nil
}

func (c *concReader) Close() error {
	if c.stopc != nil {
		c.stopAnyWorkInProgress(false)
		c.stopc = nil
	}
	return nil
}

func (c *concReader) CloseWithoutWaiting() error {
	if c.stopc != nil {
		// Just close the c.stopc channel, which should eventually shut down
		// the Manager and Worker goroutines. Everything else can be garbage
		// collected.
		close(c.stopc)
		c.stopc = nil
	}
	return nil
}

func (c *concReader) seek(offset int64, whence int, limit int64) (int64, error) {
	pos := c.pos
	switch whence {
	case io.SeekStart:
		pos = offset
	case io.SeekCurrent:
		pos += offset
	case io.SeekEnd:
		pos = c.decompressedSize + offset
	default:
		return 0, errSeekToInvalidWhence
	}

	if c.pos != pos {
		if pos < 0 {
			return 0, errSeekToNegativePosition
		}
		c.pos = pos
		c.seekResolved = false
	}

	if limit > c.decompressedSize {
		limit = c.decompressedSize
	}
	c.posLimit = limit

	return pos, nil
}

func (c *concReader) Read(p []byte) (int, error) {
	if c.pos >= c.posLimit {
		return 0, io.EOF
	}

	if !c.seekResolved {
		c.seekResolved = true
		if c.seenRead {
			c.stopAnyWorkInProgress(true)
		}
		c.seenRead = true
		c.roic <- Range{c.pos, c.posLimit}
	}

	for numRead := 0; ; {
		if c.pos >= c.posLimit {
			return numRead, io.EOF
		}
		if len(p) == 0 {
			return numRead, nil
		}
		if c.currWork.i >= c.currWork.j {
			err := c.currWork.err
			c.currWork.recycle()
			if err != nil {
				return numRead, err
			}
			c.currWork = c.nextWork()
		}

		// Fill p from c.currWork.
		n := copy(p, c.currWork.buffer[c.currWork.i:c.currWork.j])
		p = p[n:]
		numRead += n
		c.pos += int64(n)
		c.currWork.i += uint32(n)
	}
}

func (c *concReader) nextWork() rWork {
	for {
		if work, ok := c.completedWorks[c.pos]; ok {
			delete(c.completedWorks, c.pos)
			return work
		}
		work := <-c.resc
		c.completedWorks[work.dRange[0]] = work
	}
}

// stopAnyWorkInProgress winds up any Manager and Worker work-in-progress.
// keepWorking is whether those goroutines should stick around to do future
// work. It should be false for closes and true otherwise.
func (c *concReader) stopAnyWorkInProgress(keepWorking bool) {
	// Synchronize the Manager and Workers on stopc (an unbuffered channel).
	for i, n := 0, 1+c.numWorkers; i < n; i++ {
		c.stopc <- stopWork{c.ackc, keepWorking}
	}

	if keepWorking {
		c.recycleBuffers()
	}

	// Synchronize the Manager and Workers on ackc (an unbuffered channel).
	for i, n := 0, 1+c.numWorkers; i < n; i++ {
		c.ackc <- struct{}{}
	}
}

func (c *concReader) recycleBuffers() {
	c.currWork.recycle()

	for k, work := range c.completedWorks {
		work.recycle()
		delete(c.completedWorks, k)
	}

	// Drain c's buffered channels.
	drainWorkChan(c.reqc)
	drainWorkChan(c.resc)
}

func drainWorkChan(c chan rWork) {
	for {
		select {
		case work := <-c:
			work.recycle()
		default:
			return
		}
	}
}

func runRWorker(stopc <-chan stopWork, resc chan<- rWork, reqc <-chan rWork, racReader *Reader) {
	input, output := reqc, (chan<- rWork)(nil)
	outWork := rWork{}

	// dRange is what part of incoming work remains to be read from the
	// racReader.
	dRange := Range{}

	// Each worker owns up to numRBuffersPerWorker buffers, some of which may
	// be temporarily loaned to the concReader goroutine.
	buffers := [numRBuffersPerWorker]*rBuffer{}
	recyclec := make(chan *rBuffer, numRBuffersPerWorker)
	canAlloc := numRBuffersPerWorker

loop:
	for {
		select {
		case stop := <-stopc:
			if stop.ackc != nil {
				<-stop.ackc
			} else {
				// No need to ack. This is CloseWithoutWaiting.
			}
			if !stop.keepWorking {
				return
			}
			continue loop

		case inWork := <-input:
			input = nil
			if inWork.err == nil {
				dRange = inWork.dRange
				if dRange.Empty() {
					inWork.err = errInternalEmptyDRange
				} else {
					inWork.err = racReader.SeekRange(dRange[0], dRange[1])
				}
				if inWork.err == io.EOF {
					inWork.err = io.ErrUnexpectedEOF
				}
			}
			if inWork.err != nil {
				output, outWork = resc, inWork
				continue loop
			}

		case output <- outWork:
			output, outWork = nil, rWork{}

		case recycledBuffer := <-recyclec:
			for i := range buffers {
				if buffers[i] == nil {
					buffers[i], recycledBuffer = recycledBuffer, nil
					break
				}
			}
			if recycledBuffer != nil {
				panic("unreachable")
			}
		}

		// If there's existing outWork, sending it trumps making new outWork.
		if output != nil {
			continue loop
		}

		// If dRange was completely processsed, get new inWork.
		if dRange.Empty() {
			input = reqc
			continue loop
		}

		// Find a new or recycled buffer.
		buffer := (*rBuffer)(nil)
		{
			b := -1
			if buffers[0] != nil {
				b = 0
			} else if buffers[1] != nil {
				b = 1
			}

			if b >= 0 {
				buffer, buffers[b] = buffers[b], nil
			} else if canAlloc == 0 {
				// Wait until we receive a recycled buffer.
				continue loop
			} else {
				canAlloc--
				buffer = &rBuffer{}
			}
		}

		// Make a new outWork, shrinking dRange to be whatever's left over.
		{
			n, err := racReader.Read(buffer[:])
			if err == io.EOF {
				err = nil
			}
			oldDPos := dRange[0]
			newDPos := dRange[0] + int64(n)
			dRange[0] = newDPos
			output, outWork = resc, rWork{
				err:      err,
				dRange:   Range{oldDPos, newDPos},
				buffer:   buffer,
				i:        0,
				j:        uint32(n),
				recyclec: recyclec,
			}
		}
	}
}

func runRManager(stopc <-chan stopWork, roic <-chan Range, reqc chan<- rWork, chunkReader *ChunkReader) {
	input, output := roic, (chan<- rWork)(nil)
	roi := Range{}
	work := rWork{}

loop:
	for {
		select {
		case stop := <-stopc:
			if stop.ackc != nil {
				<-stop.ackc
			} else {
				// No need to ack. This is CloseWithoutWaiting.
			}
			if !stop.keepWorking {
				return
			}
			continue loop

		case roi = <-input:
			input, output = nil, reqc
			if err := chunkReader.SeekToChunkContaining(roi[0]); err != nil {
				if err == io.EOF {
					err = io.ErrUnexpectedEOF
				}
				work = rWork{err: err}
				continue loop
			}

		case output <- work:
			err := work.err
			work = rWork{}
			if err != nil {
				input, output = roic, nil
				continue loop
			}
		}

		for {
			chunk, err := chunkReader.NextChunk()
			if err == io.EOF {
				input, output = roic, nil
				continue loop
			} else if err != nil {
				work = rWork{err: err}
				continue loop
			}

			if chunk.DRange[0] >= roi[1] {
				input, output = roic, nil
				continue loop
			}
			if dr := chunk.DRange.Intersect(roi); !dr.Empty() {
				work = rWork{dRange: dr}
				continue loop
			}
		}
	}
}
