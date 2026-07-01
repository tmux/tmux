#include <simdutf.h>

extern "C" {

size_t ghostty_simd_base64_max_length(const char* input, size_t length) {
  return simdutf::maximal_binary_length_from_base64(input, length);
}

size_t ghostty_simd_base64_decode(const char* input,
                                  size_t length,
                                  char* output) {
  simdutf::result r = simdutf::base64_to_binary(input, length, output);
  if (r.error) {
    return -1;
  }

  return r.count;
}

}  // extern "C"
