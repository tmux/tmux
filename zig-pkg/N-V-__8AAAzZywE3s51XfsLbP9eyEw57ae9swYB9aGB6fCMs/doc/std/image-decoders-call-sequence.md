# Image Decoders Call Sequence

As discussed in the [Image Decoders](./image-decoders.md) document, here's the
canonical sequence of API calls to decode an animated image (with N frames):

1. DIC (`decode_image_config`)
1. DFC (`decode_frame_config`) #0
1. DF (`decode_frame`) #0
1. DFC (`decode_frame_config`) #1
1. DF (`decode_frame`) #1
1. ...
1. DFC (`decode_frame_config`) #N-1
1. DF (`decode_frame`) #N-1
1. DFC (`decode_frame_config`) #N returning an `"@end of data"` status value.

The final DFC call returns `"@end of data"` even if the animation loops.

Decoding a still (non-animated) image is equivalent to N being 1.

These three methods (DIC, DFC, DF) are all
[coroutines](/doc/note/coroutines.md) and may return I/O related suspension
status values. Those coroutines should be resumed by calling the same method
again, before moving to the next one in the sequence.

Encountering an error (such as the input not being valid in that image file
format) is sticky (any further calls will return that same error).


## Implicit Calls

These three methods (DIC, DFC, DF) are all also optional in some sense.
Referring to the ordered list at the top of this page, any `decode_etc` call
jumps forward to the next matching method in that canonical sequence,
implicitly calling any jumped over method with the same `io_reader` but null
values for other arguments.

For example, starting with a freshly initialized image decoder, if it's a first
party image and you already know its width, height and pixel format (so you can
prepare a perfectly sized pixel buffer without having to call DIC or DFC), it
is valid to call DF, going straight to the DF #0 stage. This implicitly calls
DIC and DFC #0.

Even if you didn't know the width, height and pixel format, it is still valid
to call DF with a smaller-dimensioned pixel buffer (which just means the
decoded image will be clipped) or with a different pixel format (the decoder
will convert).

After that, it is valid to call DFC, going to the DFC #1 stage (or get "@end of
data" for still images), which would not make any implicit calls.

After that, it is valid to call DFC, going to the DFC #2 stage, implicitly
calling DF #1.

And so on.

For example, starting at the DF #N-1 stage, it is valid to call DF again, which
will return "@end of data" because it first implicitly calls DF #N, which
returns "@end of data".

Implicitly calling DF (skipping over the pixel data) is typically much faster
than explicitly calling DF (decompressing the pixel data). For example,
starting with a freshly initialized image decoder, if you just want to *count*
the number of animation frames, without needing each frame's decoded pixels,
just call DFC (and not DF) in a loop until "#end of data".


## State Machine

It's not part of Wuffs' public API, but Wuffs' standard library's image decoder
implementations all have a private `call_sequence` field to track this state
machine. Broadly speaking, an image decoder starts in an initial state (labeled
`0x00`). The DIC call moves to state `0x20`. Subsequent DFC and DF calls bounce
between states `0x40` and `0x20` until the last DFC moves the final state
`0x60`. These are the bold-labeled states in the left hand column of the
diagram below:

![Image Decoders Call Sequence
Diagram](./image-decoders-call-sequence-diagram.png)

The original Google Docs document for that diagram [is
here](https://docs.google.com/document/d/1bbH9cpgxsMP8M5U4NsTotr-wSlyH85tiDKSJSF1HVXc/edit?usp=sharing).

The italic-labeled states (whose numeric labels all have the `0x10` bit set) in
the right hand column are side-track states when [metadata](./metadata.md) is
encountered, and are discussed below. If the caller never opts in to seeing
metadata (opting in by calling `set_report_metadata`) then the decoder never
enters these `(base_value | 0x10)` states.

The `0x08` bit is used when calling `restart_frame`, discussed below. The low
three bits are reserved but currently unused.

The primary purposes of the `call_sequence` field is track when we've reached
"@end of data" and to reject method calls that occur out of sequence. Such
method calls result in the `"#bad call sequence"` error status value. For
example, a DIC call is illegal after a DIC, DFC or DF call. Similarly, a TMM
(`tell_me_more`) call is illegal unless the decoder is in a right hand column
state.


## Restarts

Image decoding typically runs sequentially: the first frame (#0) is decoded,
then the second (#1), then the third (#2), etc. However, decoding can sometimes
happen in random access instead of sequential order. Playing a looping
animation may require rewinding back to the first frame. Replaying an animation
may be able to re-use previousy cached frames at times, so a decoder may be
asked to jump ahead. While Wuffs is designed for sequential decoding (as it
does not assume that its input byte stream is rewindable or seekable), using
Wuffs to implement other graphics APIs can involve [random access to animation
frames](https://github.com/google/skia/blob/ee6f41262b2ef86b5f06b33473379ac887401451/include/codec/SkCodec.h#L314-L319).

The RF `restart_frame` method supports these usage patterns, setting up the
decoder so that it's just about to decode the I'th frame configuration (when
the `io_reader` is also placed at `io_position` previously returned by the I'th
DFC).

The RF method can only be called when the image configuration has already been
decoded. Equivalently, only when the `call_sequence` is at least `0x20`.
