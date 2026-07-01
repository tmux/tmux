# Metadata

Files can also contain metadata (e.g. for image files, EXIF information, color
profiles and time stamps). By default, Wuffs' decoders skip over metadata, but
calling `set_report_metadata` will opt in to having `decode_etc` methods return
early when encountering metadata in the file. Calling `set_report_metadata` can
be done multiple times, each with a different
[FourCC](/doc/note/base38-and-fourcc.md) code such as `0x49434350` "ICCP" or
`0x584D5020` "XMP ", to indicate what sorts of metadata the caller is
interested in. Conversely, when the parser encounters metadata (and returns a
"@metadata reported" [status](/doc/note/statuses.md)), call `tell_me_more` to
see what sort of metadata it is.

Some metadata is short and `tell_me_more` will also fill in its `minfo: nptr
more_information` out parameter with "parsed metadata". For example, Gamma
Correction metadata is a single numerical value and Primary Chromaticities
metadata is only eight numbers. From C++ code, call the `metadata_parsed__gama`
or `metadata_parsed__chrm` methods to retrieve these value.

Some metadata is long (or at least variable length) and needs processing by a
separate parser. For example, processing XMP metadata usually involves some
sort of XML parser, regardless of what particular image format that XMP
metadata was embedded in. Wuffs decoders will return "raw metadata" instead of
parsing it themselves. Raw metadata is further sub-divided into two categories:
"raw passthrough" and "raw transform". Either way, note that raw metadata might
also be arranged in multiple (non-contiguous) chunks of the source data.

"Raw passthrough" means that the bytes in the wire format can be sent "as is"
to the separate parser. Call `tell_me_more` in a loop (the `dst: io_writer`
argument will be ignored) and `metadata_raw_passthrough__range` to identify a
range (a chunk) of the input stream. Divert the bytes in that range to the
separate parser (which should advance the `io_buffer` to the end of that range)
and repeat the loop as long as `tell_me_more` returned a "$even more
information" status.

"Raw transform" means that the bytes in the wire format have to be further
processed (e.g. decompressed) before sending them on to the separate parser.
That processing is done by the decoder callee, not the caller. Pass a writable
`dst io_buffer` to `tell_me_more` (where writable means that it has a positive
`writer_length`) and implementations should fill in that buffer with
post-transformed (e.g. decompressed) data to send onwards. Like any
`io_transformer`-like coroutine, this should be in a loop, as it may suspend
with "$short read" and "$short write" statuses.

Either way, break the loop (after handling the `dst` and `minfo` out
parameters) when `tell_me_more` returns a NULL status, meaning ok, when the
metadata is complete. Afterwards, call the original action (e.g. `decode_etc`)
again to resume after the "@metadata reported" detour.

`tell_me_more` both returns a status and fills in the `dst` and `minfo` out
parameters. Subtly, the caller should process the out parameters before
considering the status. It is valid for the callee to fill in the out
parameters with partial results when returning an error status or with full
results when returning an ok status. 'No partial results' is represented by an
unchanged `dst` or `minfo`. For example, if `minfo` was reset to zero before
each `tell_me_more` call then `minfo.flavor` remaining zero means that the
`minfo` was unchanged, as valid flavor values are non-zero.


## Examples

- [script/print-image-metadata](/script/print-image-metadata.cc)
