const std = @import("std");

const c = @cImport({
    @cInclude("tmux.h");
});

// OSC 133 parsing and grid-line application, shared by the native input
// parser and the Ghostty VT backend. Moved to Zig as part of the
// incremental rewrite; input.c keeps the pane state, event, and policy
// side. Exported under the names tmux.h already declares so C callers
// need no changes.

// The D marker's exit status: the field after ';' up to the next ';' or
// end, rejecting empty fields, anything containing '=', and values
// outside 0..255. Mirrors strtonum(copy, 0, 255): any parse failure
// yields 255.
fn exitStatus(p: [*:0]const u8) u8 {
    const s = std.mem.span(p);
    if (s.len < 3 or s[1] != ';')
        return 0;
    const rest = s[2..];
    if (rest[0] == '=')
        return 0;
    const end = std.mem.indexOfScalar(u8, rest, ';') orelse rest.len;
    const field = rest[0..end];
    if (field.len == 0 or std.mem.indexOfScalar(u8, field, '=') != null)
        return 0;
    const value = std.fmt.parseInt(i64, std.mem.trim(u8, field, " \t\n\r"), 10) catch
        return 255;
    if (value < 0 or value > 255)
        return 255;
    return @intCast(value);
}

pub fn parse(p: [*:0]const u8, column: c_uint, marker: *c.input_osc133_marker) void {
    marker.* = std.mem.zeroes(c.input_osc133_marker);
    marker.column = @truncate(column);

    const s = std.mem.span(p);
    if (s.len == 0)
        return;
    switch (s[0]) {
        'A', 'N' => marker.type = c.INPUT_OSC133_PROMPT,
        'P' => {
            marker.type = c.INPUT_OSC133_PROMPT_MARK;
            if (std.mem.indexOf(u8, s, ";k=s")) |idx| {
                const after = idx + 4;
                if (after == s.len or s[after] == ';')
                    marker.type = c.INPUT_OSC133_SECOND_PROMPT;
            }
        },
        'B', 'I' => marker.type = c.INPUT_OSC133_COMMAND,
        'C' => marker.type = c.INPUT_OSC133_OUTPUT,
        'D' => {
            marker.type = c.INPUT_OSC133_END;
            marker.exit_status = exitStatus(p);
        },
        else => {},
    }
}

pub fn applyLine(gl: *c.grid_line, marker: *const c.input_osc133_marker) void {
    switch (marker.type) {
        c.INPUT_OSC133_PROMPT => {
            gl.osc133_data = std.mem.zeroes(c.osc133_data);
            gl.osc133_data.prompt_col = marker.column;
            gl.flags |= @as(c_ushort, c.GRID_LINE_START_PROMPT);
        },
        c.INPUT_OSC133_PROMPT_MARK => {
            gl.flags |= @as(c_ushort, c.GRID_LINE_START_PROMPT);
            gl.osc133_data.prompt_col = marker.column;
        },
        c.INPUT_OSC133_SECOND_PROMPT => {
            gl.flags |= @as(c_ushort, c.GRID_LINE_SECOND_PROMPT);
            gl.osc133_data.prompt_col = marker.column;
        },
        c.INPUT_OSC133_COMMAND => {
            gl.flags |= @as(c_ushort, c.GRID_LINE_START_COMMAND);
            gl.osc133_data.cmd_col = marker.column;
        },
        c.INPUT_OSC133_OUTPUT => {
            gl.flags |= @as(c_ushort, c.GRID_LINE_START_OUTPUT);
            gl.osc133_data.out_start_col = marker.column;
        },
        c.INPUT_OSC133_END => {
            gl.flags |= @as(c_ushort, c.GRID_LINE_END_OUTPUT);
            gl.osc133_data.out_end_col = marker.column;
            gl.osc133_data.exit_status = marker.exit_status;
        },
        else => {},
    }
}

export fn input_osc_133_parse(p: [*:0]const u8, column: c_uint, marker: ?*c.input_osc133_marker) void {
    parse(p, column, marker orelse return);
}

export fn input_osc_133_apply_line(gl: ?*c.grid_line, marker: ?*const c.input_osc133_marker) void {
    applyLine(gl orelse return, marker orelse return);
}
