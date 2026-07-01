/// Matrix type
pub const Mat = [4]F32x4;
pub const F32x4 = @Vector(4, f32);

/// 2D orthographic projection matrix
pub fn ortho2d(left: f32, right: f32, bottom: f32, top: f32) Mat {
    const w = right - left;
    const h = top - bottom;
    return .{
        .{ 2 / w, 0, 0, 0 },
        .{ 0, 2 / h, 0, 0 },
        .{ 0.0, 0.0, -1.0, 0.0 },
        .{ -(right + left) / w, -(top + bottom) / h, 0.0, 1.0 },
    };
}
