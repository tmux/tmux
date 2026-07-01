//! A zig builder step that runs "lipo" on two binaries to create
//! a universal binary.
const LipoStep = @This();

const std = @import("std");
const Step = std.Build.Step;
const RunStep = std.Build.Step.Run;
const LazyPath = std.Build.LazyPath;

pub const Options = struct {
    /// The name of the xcframework to create.
    name: []const u8,

    /// The filename (not the path) of the file to create.
    out_name: []const u8,

    /// Library file (dylib, a) to package.
    input_a: LazyPath,
    input_b: LazyPath,
};

step: *Step,

/// Resulting binary
output: LazyPath,

pub fn create(b: *std.Build, opts: Options) *LipoStep {
    const self = b.allocator.create(LipoStep) catch @panic("OOM");

    const run_step = RunStep.create(b, b.fmt("lipo {s}", .{opts.name}));
    run_step.addArgs(&.{ "lipo", "-create", "-output" });
    const output = run_step.addOutputFileArg(opts.out_name);
    run_step.addFileArg(opts.input_a);
    run_step.addFileArg(opts.input_b);

    self.* = .{
        .step = &run_step.step,
        .output = output,
    };

    return self;
}
