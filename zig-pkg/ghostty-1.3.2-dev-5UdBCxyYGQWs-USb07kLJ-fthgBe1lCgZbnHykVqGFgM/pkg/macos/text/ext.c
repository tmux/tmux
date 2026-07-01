#include <CoreText/CoreText.h>

// A wrapper to fix a Zig C ABI issue.
void zig_cabi_CTLineGetBoundsWithOptions(
    CTLineRef line,
    CTLineBoundsOptions options,
    CGRect *result
) {
    *result = CTLineGetBoundsWithOptions(line, options);
}
