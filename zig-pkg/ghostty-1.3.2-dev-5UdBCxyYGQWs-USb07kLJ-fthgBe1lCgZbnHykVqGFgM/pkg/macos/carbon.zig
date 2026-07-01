pub const c = @import("carbon/c.zig").c;

test {
    @import("std").testing.refAllDecls(@This());
}
