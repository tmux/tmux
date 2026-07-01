// Copyright 2021 The Wuffs Authors.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

// ---------------- Auxiliary - Image

namespace wuffs_aux {

struct DecodeImageResult {
  DecodeImageResult(MemOwner&& pixbuf_mem_owner0,
                    wuffs_base__pixel_buffer pixbuf0,
                    std::string&& error_message0);
  DecodeImageResult(std::string&& error_message0);

  MemOwner pixbuf_mem_owner;
  wuffs_base__pixel_buffer pixbuf;
  std::string error_message;
};

// DecodeImageCallbacks are the callbacks given to DecodeImage. They are always
// called in this order:
//  1. SelectDecoder
//  2. HandleMetadata
//  3. SelectPixfmt
//  4. AllocPixbuf
//  5. AllocWorkbuf
//  6. Done
//
// It may return early - the third callback might not be invoked if the second
// one fails - but the final callback (Done) is always invoked.
class DecodeImageCallbacks {
 public:
  // AllocPixbufResult holds a memory allocation (the result of malloc or new,
  // a statically allocated pointer, etc), or an error message. The memory is
  // de-allocated when mem_owner goes out of scope and is destroyed.
  struct AllocPixbufResult {
    AllocPixbufResult(MemOwner&& mem_owner0, wuffs_base__pixel_buffer pixbuf0);
    AllocPixbufResult(std::string&& error_message0);

    MemOwner mem_owner;
    wuffs_base__pixel_buffer pixbuf;
    std::string error_message;
  };

  // AllocWorkbufResult holds a memory allocation (the result of malloc or new,
  // a statically allocated pointer, etc), or an error message. The memory is
  // de-allocated when mem_owner goes out of scope and is destroyed.
  struct AllocWorkbufResult {
    AllocWorkbufResult(MemOwner&& mem_owner0, wuffs_base__slice_u8 workbuf0);
    AllocWorkbufResult(std::string&& error_message0);

    MemOwner mem_owner;
    wuffs_base__slice_u8 workbuf;
    std::string error_message;
  };

  virtual ~DecodeImageCallbacks();

  // SelectDecoder returns the image decoder for the input data's file format.
  // Returning a nullptr means failure (DecodeImage_UnsupportedImageFormat).
  //
  // Common formats will have a FourCC value in the range [1 ..= 0x7FFF_FFFF],
  // such as WUFFS_BASE__FOURCC__JPEG. A zero FourCC value means that Wuffs'
  // standard library did not recognize the image format but if SelectDecoder
  // was overridden, it may examine the input data's starting bytes and still
  // provide its own image decoder, e.g. for an exotic image file format that's
  // not in Wuffs' standard library. The prefix_etc fields have the same
  // meaning as wuffs_base__magic_number_guess_fourcc arguments. SelectDecoder
  // implementations should not modify prefix_data's contents.
  //
  // SelectDecoder might be called more than once, since some image file
  // formats can wrap others. For example, a nominal BMP file can actually
  // contain a JPEG or a PNG.
  //
  // The default SelectDecoder accepts the FOURCC codes listed below. For
  // modular builds (i.e. when #define'ing WUFFS_CONFIG__MODULES), acceptance
  // of the FOO file format is optional (for each value of FOO) and depends on
  // the corresponding module to be enabled at compile time (i.e. #define'ing
  // WUFFS_CONFIG__MODULE__FOO).
  //
  //  - WUFFS_BASE__FOURCC__BMP
  //  - WUFFS_BASE__FOURCC__ETC2
  //  - WUFFS_BASE__FOURCC__GIF
  //  - WUFFS_BASE__FOURCC__JPEG
  //  - WUFFS_BASE__FOURCC__NIE
  //  - WUFFS_BASE__FOURCC__NPBM
  //  - WUFFS_BASE__FOURCC__PNG
  //  - WUFFS_BASE__FOURCC__QOI
  //  - WUFFS_BASE__FOURCC__TARGA
  //  - WUFFS_BASE__FOURCC__TH
  //  - WUFFS_BASE__FOURCC__WBMP
  //  - WUFFS_BASE__FOURCC__WEBP
  //
  // The FOOBAR in WUFFS_BASE__FOURCC__FOBA is limited to four characters, but
  // the FOOBAR in the corresponding WUFFS_CONFIG__MODULE__FOOBAR macro might
  // be fuller and longer. For example, NPBM / NETPBM or TH / THUMBHASH.
  virtual wuffs_base__image_decoder::unique_ptr  //
  SelectDecoder(uint32_t fourcc,
                wuffs_base__slice_u8 prefix_data,
                bool prefix_closed);

  // HandleMetadata acknowledges image metadata. minfo.flavor will be one of:
  //  - WUFFS_BASE__MORE_INFORMATION__FLAVOR__METADATA_RAW_PASSTHROUGH
  //  - WUFFS_BASE__MORE_INFORMATION__FLAVOR__METADATA_PARSED
  // If it is METADATA_RAW_PASSTHROUGH then raw contains the metadata bytes.
  // Those bytes should not be retained beyond the the HandleMetadata call.
  //
  // minfo.metadata__fourcc() will typically match one of the
  // DecodeImageArgFlags bits. For example, if (REPORT_METADATA_CHRM |
  // REPORT_METADATA_GAMA) was passed to DecodeImage then the metadata FourCC
  // will be either WUFFS_BASE__FOURCC__CHRM or WUFFS_BASE__FOURCC__GAMA.
  //
  // It returns an error message, or an empty string on success.
  virtual std::string  //
  HandleMetadata(const wuffs_base__more_information& minfo,
                 wuffs_base__slice_u8 raw);

  // SelectPixfmt returns the destination pixel format for AllocPixbuf. It
  // should return wuffs_base__make_pixel_format(etc) called with one of:
  //  - WUFFS_BASE__PIXEL_FORMAT__Y
  //  - WUFFS_BASE__PIXEL_FORMAT__BGR_565
  //  - WUFFS_BASE__PIXEL_FORMAT__BGR
  //  - WUFFS_BASE__PIXEL_FORMAT__BGRA_NONPREMUL
  //  - WUFFS_BASE__PIXEL_FORMAT__BGRA_NONPREMUL_4X16LE
  //  - WUFFS_BASE__PIXEL_FORMAT__BGRA_PREMUL
  //  - WUFFS_BASE__PIXEL_FORMAT__RGB
  //  - WUFFS_BASE__PIXEL_FORMAT__RGBA_NONPREMUL
  //  - WUFFS_BASE__PIXEL_FORMAT__RGBA_PREMUL
  // or return image_config.pixcfg.pixel_format(). The latter means to use the
  // image file's natural pixel format. For example, GIF images' natural pixel
  // format is an indexed one.
  //
  // Returning otherwise means failure (DecodeImage_UnsupportedPixelFormat).
  //
  // The default SelectPixfmt implementation returns
  // wuffs_base__make_pixel_format(WUFFS_BASE__PIXEL_FORMAT__BGRA_PREMUL) which
  // is 4 bytes per pixel (8 bits per channel × 4 channels).
  virtual wuffs_base__pixel_format  //
  SelectPixfmt(const wuffs_base__image_config& image_config);

  // AllocPixbuf allocates the pixel buffer.
  //
  // allow_uninitialized_memory will be true if a valid background_color was
  // passed to DecodeImage, since the pixel buffer's contents will be
  // overwritten with that color after AllocPixbuf returns.
  //
  // The default AllocPixbuf implementation allocates either uninitialized or
  // zeroed memory. Zeroed memory typically corresponds to filling with opaque
  // black or transparent black, depending on the pixel format.
  virtual AllocPixbufResult  //
  AllocPixbuf(const wuffs_base__image_config& image_config,
              bool allow_uninitialized_memory);

  // AllocWorkbuf allocates the work buffer. The allocated buffer's length
  // should be at least len_range.min_incl, but larger allocations (up to
  // len_range.max_incl) may have better performance (by using more memory).
  //
  // The default AllocWorkbuf implementation allocates len_range.max_incl bytes
  // of either uninitialized or zeroed memory.
  virtual AllocWorkbufResult  //
  AllocWorkbuf(wuffs_base__range_ii_u64 len_range,
               bool allow_uninitialized_memory);

  // Done is always the last Callback method called by DecodeImage, whether or
  // not parsing the input encountered an error. Even when successful, trailing
  // data may remain in input and buffer.
  //
  // The image_decoder is the one returned by SelectDecoder (if SelectDecoder
  // was successful), or a no-op unique_ptr otherwise. Like any unique_ptr,
  // ownership moves to the Done implementation.
  //
  // Do not keep a reference to buffer or buffer.data.ptr after Done returns,
  // as DecodeImage may then de-allocate the backing array.
  //
  // The default Done implementation is a no-op, other than running the
  // image_decoder unique_ptr destructor.
  virtual void  //
  Done(DecodeImageResult& result,
       sync_io::Input& input,
       IOBuffer& buffer,
       wuffs_base__image_decoder::unique_ptr image_decoder);
};

extern const char DecodeImage_BufferIsTooShort[];
extern const char DecodeImage_MaxInclDimensionExceeded[];
extern const char DecodeImage_MaxInclMetadataLengthExceeded[];
extern const char DecodeImage_OutOfMemory[];
extern const char DecodeImage_UnexpectedEndOfFile[];
extern const char DecodeImage_UnsupportedImageFormat[];
extern const char DecodeImage_UnsupportedMetadata[];
extern const char DecodeImage_UnsupportedPixelBlend[];
extern const char DecodeImage_UnsupportedPixelConfiguration[];
extern const char DecodeImage_UnsupportedPixelFormat[];

// The FooArgBar types add structure to Foo's optional arguments. They wrap
// inner representations for several reasons:
//  - It provides a home for the DefaultValue static method, for Foo callers
//    that want to override some but not all optional arguments.
//  - It provides the "Bar" name at Foo call sites, which can help self-
//    document Foo calls with many arguemnts.
//  - It provides some type safety against accidentally transposing or omitting
//    adjacent fundamentally-numeric-typed optional arguments.

// DecodeImageArgQuirks wraps an optional argument to DecodeImage.
struct DecodeImageArgQuirks {
  explicit DecodeImageArgQuirks(const QuirkKeyValuePair* ptr0,
                                const size_t len0);

  // DefaultValue returns an empty slice.
  static DecodeImageArgQuirks DefaultValue();

  const QuirkKeyValuePair* ptr;
  const size_t len;
};

// DecodeImageArgFlags wraps an optional argument to DecodeImage.
struct DecodeImageArgFlags {
  explicit DecodeImageArgFlags(uint64_t repr0);

  // DefaultValue returns 0.
  static DecodeImageArgFlags DefaultValue();

  // TODO: support all of the REPORT_METADATA_FOO flags, not just CHRM, EXIF,
  // GAMA, ICCP, KVP, SRGB and XMP.

  // Background Color.
  static constexpr uint64_t REPORT_METADATA_BGCL = 0x0001;
  // Primary Chromaticities and White Point.
  static constexpr uint64_t REPORT_METADATA_CHRM = 0x0002;
  // Exchangeable Image File Format.
  static constexpr uint64_t REPORT_METADATA_EXIF = 0x0004;
  // Gamma Correction.
  static constexpr uint64_t REPORT_METADATA_GAMA = 0x0008;
  // International Color Consortium Profile.
  static constexpr uint64_t REPORT_METADATA_ICCP = 0x0010;
  // Key-Value Pair.
  //
  // For PNG files, this includes iTXt, tEXt and zTXt chunks. In the
  // HandleMetadata callback, the raw argument contains UTF-8 strings.
  static constexpr uint64_t REPORT_METADATA_KVP = 0x0020;
  // Modification Time.
  static constexpr uint64_t REPORT_METADATA_MTIM = 0x0040;
  // Offset (2-Dimensional).
  static constexpr uint64_t REPORT_METADATA_OFS2 = 0x0080;
  // Physical Dimensions.
  static constexpr uint64_t REPORT_METADATA_PHYD = 0x0100;
  // Standard Red Green Blue (Rendering Intent).
  static constexpr uint64_t REPORT_METADATA_SRGB = 0x0200;
  // Extensible Metadata Platform.
  static constexpr uint64_t REPORT_METADATA_XMP = 0x0400;

  uint64_t repr;
};

// DecodeImageArgPixelBlend wraps an optional argument to DecodeImage.
struct DecodeImageArgPixelBlend {
  explicit DecodeImageArgPixelBlend(wuffs_base__pixel_blend repr0);

  // DefaultValue returns WUFFS_BASE__PIXEL_BLEND__SRC.
  static DecodeImageArgPixelBlend DefaultValue();

  wuffs_base__pixel_blend repr;
};

// DecodeImageArgBackgroundColor wraps an optional argument to DecodeImage.
struct DecodeImageArgBackgroundColor {
  explicit DecodeImageArgBackgroundColor(
      wuffs_base__color_u32_argb_premul repr0);

  // DefaultValue returns 1, an invalid wuffs_base__color_u32_argb_premul.
  static DecodeImageArgBackgroundColor DefaultValue();

  wuffs_base__color_u32_argb_premul repr;
};

// DecodeImageArgMaxInclDimension wraps an optional argument to DecodeImage.
struct DecodeImageArgMaxInclDimension {
  explicit DecodeImageArgMaxInclDimension(uint32_t repr0);

  // DefaultValue returns 1048575 = 0x000F_FFFF, more than 1 million pixels.
  static DecodeImageArgMaxInclDimension DefaultValue();

  uint32_t repr;
};

// DecodeImageArgMaxInclMetadataLength wraps an optional argument to
// DecodeImage.
struct DecodeImageArgMaxInclMetadataLength {
  explicit DecodeImageArgMaxInclMetadataLength(uint64_t repr0);

  // DefaultValue returns 16777215 = 0x00FF_FFFF, one less than 16 MiB.
  static DecodeImageArgMaxInclMetadataLength DefaultValue();

  uint64_t repr;
};

// DecodeImage decodes the image data in input. A variety of image file formats
// can be decoded, depending on what callbacks.SelectDecoder returns.
//
// For animated formats, only the first frame is returned, since the API is
// simpler for synchronous I/O and having DecodeImage only return when
// completely done, but rendering animation often involves handling other
// events in between animation frames. To decode multiple frames of animated
// images, or for asynchronous I/O (e.g. when decoding an image streamed over
// the network), use Wuffs' lower level C API instead of its higher level,
// simplified C++ API (the wuffs_aux API).
//
// The DecodeImageResult's fields depend on whether decoding succeeded:
//  - On total success, the error_message is empty and pixbuf.pixcfg.is_valid()
//    is true.
//  - On partial success (e.g. the input file was truncated but we are still
//    able to decode some of the pixels), error_message is non-empty but
//    pixbuf.pixcfg.is_valid() is still true. It is up to the caller whether to
//    accept or reject partial success.
//  - On failure, the error_message is non_empty and pixbuf.pixcfg.is_valid()
//    is false.
//
// The callbacks allocate the pixel buffer memory and work buffer memory. On
// success, pixel buffer memory ownership is passed to the DecodeImage caller
// as the returned pixbuf_mem_owner. Regardless of success or failure, the work
// buffer memory is deleted.
//
// The pixel_blend (one of the constants listed below) determines how to
// composite the decoded image over the pixel buffer's original pixels (as
// returned by callbacks.AllocPixbuf):
//  - WUFFS_BASE__PIXEL_BLEND__SRC
//  - WUFFS_BASE__PIXEL_BLEND__SRC_OVER
//
// The background_color is used to fill the pixel buffer after
// callbacks.AllocPixbuf returns, if it is valid in the
// wuffs_base__color_u32_argb_premul__is_valid sense. The default value,
// 0x0000_0001, is not valid since its Blue channel value (0x01) is greater
// than its Alpha channel value (0x00). A valid background_color will typically
// be overwritten when pixel_blend is WUFFS_BASE__PIXEL_BLEND__SRC, but might
// still be visible on partial (not total) success or when pixel_blend is
// WUFFS_BASE__PIXEL_BLEND__SRC_OVER and the decoded image is not fully opaque.
//
// Decoding fails (with DecodeImage_MaxInclDimensionExceeded) if the image's
// width or height is greater than max_incl_dimension or if any opted-in (via
// flags bits) metadata is longer than max_incl_metadata_length.
DecodeImageResult  //
DecodeImage(DecodeImageCallbacks& callbacks,
            sync_io::Input& input,
            DecodeImageArgQuirks quirks = DecodeImageArgQuirks::DefaultValue(),
            DecodeImageArgFlags flags = DecodeImageArgFlags::DefaultValue(),
            DecodeImageArgPixelBlend pixel_blend =
                DecodeImageArgPixelBlend::DefaultValue(),
            DecodeImageArgBackgroundColor background_color =
                DecodeImageArgBackgroundColor::DefaultValue(),
            DecodeImageArgMaxInclDimension max_incl_dimension =
                DecodeImageArgMaxInclDimension::DefaultValue(),
            DecodeImageArgMaxInclMetadataLength max_incl_metadata_length =
                DecodeImageArgMaxInclMetadataLength::DefaultValue());

}  // namespace wuffs_aux
