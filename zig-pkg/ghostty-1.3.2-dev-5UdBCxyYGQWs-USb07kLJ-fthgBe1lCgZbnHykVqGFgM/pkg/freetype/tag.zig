/// FT_Tag
pub const Tag = packed struct(u32) {
    d: u8,
    c: u8,
    b: u8,
    a: u8,

    pub fn init(v: *const [4]u8) Tag {
        return .{ .a = v[0], .b = v[1], .c = v[2], .d = v[3] };
    }

    /// Converts the ID to a string. The return value is only valid
    /// for the lifetime of the self pointer.
    pub fn str(self: Tag) [4]u8 {
        return .{ self.a, self.b, self.c, self.d };
    }
};
