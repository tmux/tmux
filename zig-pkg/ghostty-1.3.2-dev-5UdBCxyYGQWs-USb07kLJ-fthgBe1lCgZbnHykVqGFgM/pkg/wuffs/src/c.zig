pub const c = @cImport({
    for (defines) |d| @cDefine(d, "1");
    @cInclude("wuffs-v0.4.c");
});

/// All the C macros defined so that the header matches the build.
pub const defines: []const []const u8 = &[_][]const u8{
    "WUFFS_CONFIG__MODULES",
    "WUFFS_CONFIG__MODULE__AUX__BASE",
    "WUFFS_CONFIG__MODULE__AUX__IMAGE",
    "WUFFS_CONFIG__MODULE__BASE",
    "WUFFS_CONFIG__MODULE__ADLER32",
    "WUFFS_CONFIG__MODULE__CRC32",
    "WUFFS_CONFIG__MODULE__DEFLATE",
    "WUFFS_CONFIG__MODULE__JPEG",
    "WUFFS_CONFIG__MODULE__PNG",
    "WUFFS_CONFIG__MODULE__ZLIB",
};
