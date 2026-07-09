const std = @import("std");

const c = @cImport({
    @cDefine("HAVE_GHOSTTY_VT", "1");
    @cInclude("tmux.h");
    @cInclude("ghostty/vt.h");
});

extern fn log_debug(fmt: [*c]const u8, ...) void;

const allocator = std.heap.c_allocator;
const max_size = std.math.maxInt(u16);
const max_osc_len = 1024 * 1024;
const max_hyperlink_uri = 4096;
const kitty_storage_limit: u64 = 64 * 1024 * 1024;
const alt_pending_max = 64;
const version = "tmux next-3.7-zig";

const ClearScanState = enum {
    ground,
    esc,
    csi,
};

const GhosttyVT = struct {
    terminal: c.GhosttyTerminal,
    render_state: c.GhosttyRenderState,
    row_iter: c.GhosttyRenderStateRowIterator,
    wp: *c.window_pane,
    sx: c_uint,
    sy: c_uint,
    last_scrollback: usize,
    active_screen: c.GhosttyTerminalScreen,
    osc_buf: ?[*]u8,
    osc_len: usize,
    osc_cap: usize,
    osc_active: bool,
    osc_esc: bool,
    osc_pending_esc: bool,
    alt_pending: [alt_pending_max]u8,
    alt_pending_len: usize,
    kitty_iter: c.GhosttyKittyGraphicsPlacementIterator,
    kitty_enabled: bool,
    last_kitty_sig: ?u64,
    screen_cleared: bool,
    saw_esc: bool,
    hist_anchor: c.GhosttyTrackedGridRef,
    clear_state: ClearScanState,
    clear_param: u16,
    clear_param_active: bool,
    clear_param_done: bool,
};

fn sizeValid(sx: c_uint, sy: c_uint) bool {
    return sx > 0 and sy > 0 and sx <= max_size and sy <= max_size;
}

fn pixelSize(wp: *c.window_pane) struct { width: u32, height: u32 } {
    if (wp.*.window) |window| {
        return .{
            .width = @intCast(window.*.xpixel),
            .height = @intCast(window.*.ypixel),
        };
    }
    return .{ .width = 0, .height = 0 };
}

fn asPane(userdata: ?*anyopaque) ?*c.window_pane {
    return @ptrCast(@alignCast(userdata orelse return null));
}

fn alternateMode(mode: c_uint) bool {
    return mode == 47 or mode == 1047 or mode == 1049;
}

const AltFiltered = struct {
    data: []u8,
    cap: usize,
};

const AltParse = union(enum) {
    not_candidate,
    partial,
    complete: usize,
};

// Byte at position `idx` of the virtual stream `pending ++ buf`.
fn altStreamAt(gvt: *GhosttyVT, buf: []const u8, idx: usize) u8 {
    if (idx < gvt.alt_pending_len)
        return gvt.alt_pending[idx];
    return buf[idx - gvt.alt_pending_len];
}

// Parse a private set/reset sequence (ESC [ ? params h/l) starting at
// `off`. Params must be non-empty digit runs separated by ';'.
fn altParse(gvt: *GhosttyVT, buf: []const u8, off: usize, n: usize) AltParse {
    const prefix = "\x1b[?";
    var i = off;
    for (prefix) |ch| {
        if (i == n)
            return .partial;
        if (altStreamAt(gvt, buf, i) != ch)
            return .not_candidate;
        i += 1;
    }
    while (true) {
        if (i == n)
            return .partial;
        if (!std.ascii.isDigit(altStreamAt(gvt, buf, i)))
            return .not_candidate;
        while (i < n and std.ascii.isDigit(altStreamAt(gvt, buf, i)))
            i += 1;
        if (i == n)
            return .partial;
        switch (altStreamAt(gvt, buf, i)) {
            ';' => i += 1,
            'h', 'l' => return .{ .complete = i + 1 },
            else => return .not_candidate,
        }
    }
}

// Re-emit the sequence in stream[off..end] without the alternate-screen
// modes. Emits nothing when no other modes remain.
fn altRewrite(gvt: *GhosttyVT, buf: []const u8, off: usize, end: usize, out: []u8, out_off: usize) usize {
    var o = out_off;
    const body_start = off + 3;
    const final = altStreamAt(gvt, buf, end - 1);
    var kept = false;

    var i = body_start;
    while (i < end - 1) {
        var mode: c_uint = 0;
        while (i < end - 1 and std.ascii.isDigit(altStreamAt(gvt, buf, i))) : (i += 1) {
            if (mode < 100000)
                mode = mode * 10 + @as(c_uint, altStreamAt(gvt, buf, i) - '0');
        }
        if (i < end - 1 and altStreamAt(gvt, buf, i) == ';')
            i += 1;
        if (alternateMode(mode))
            continue;
        if (!kept) {
            @memcpy(out[o..][0..3], "\x1b[?");
            o += 3;
        } else {
            out[o] = ';';
            o += 1;
        }
        var digits: [10]u8 = undefined;
        const str = std.fmt.bufPrint(&digits, "{d}", .{mode}) catch unreachable;
        @memcpy(out[o..][0..str.len], str);
        o += str.len;
        kept = true;
    }
    if (kept) {
        out[o] = final;
        o += 1;
    }
    return o;
}

// Strip alternate-screen switch sequences from the input, carrying an
// unfinished candidate sequence across writes in gvt.alt_pending. The
// returned buffer is owned by the caller; free data.ptr[0..cap].
fn filterAlternateScreen(gvt: *GhosttyVT, buf: []const u8) ?AltFiltered {
    if (gvt.alt_pending_len == 0 and
        std.mem.indexOfScalar(u8, buf, 0x1b) == null)
        return null;

    const cap = gvt.alt_pending_len + buf.len;
    const out = allocator.alloc(u8, cap) catch return null;
    const n = cap;

    var i: usize = 0;
    var o: usize = 0;
    while (i < n) {
        const ch = altStreamAt(gvt, buf, i);
        if (ch != 0x1b) {
            out[o] = ch;
            o += 1;
            i += 1;
            continue;
        }
        switch (altParse(gvt, buf, i, n)) {
            .not_candidate => {
                out[o] = ch;
                o += 1;
                i += 1;
            },
            .complete => |end| {
                o = altRewrite(gvt, buf, i, end, out, o);
                i = end;
            },
            .partial => {
                const rem = n - i;
                if (rem <= alt_pending_max) {
                    var tmp: [alt_pending_max]u8 = undefined;
                    for (0..rem) |k|
                        tmp[k] = altStreamAt(gvt, buf, i + k);
                    @memcpy(gvt.alt_pending[0..rem], tmp[0..rem]);
                    gvt.alt_pending_len = rem;
                    // Held bytes re-enter the stream on the next write.
                    return finishAltFilter(gvt, out, cap, o, true);
                }
                // Too long to hold; give up on this candidate.
                while (i < n) : (i += 1) {
                    out[o] = altStreamAt(gvt, buf, i);
                    o += 1;
                }
            },
        }
    }
    return finishAltFilter(gvt, out, cap, o, false);
}

fn finishAltFilter(gvt: *GhosttyVT, out: []u8, cap: usize, o: usize, held: bool) AltFiltered {
    if (!held)
        gvt.alt_pending_len = 0;
    return .{ .data = out[0..o], .cap = cap };
}

fn setProgressBar(wp: ?*c.window_pane, state: c.enum_progress_bar_state, progress: c_int) void {
    const pane = wp orelse return;
    c.screen_set_progress_bar(&pane.*.base, state, progress);
    if (pane.*.window) |window| {
        c.server_redraw_window_borders(window);
        c.server_status_window(window);
    }
}

fn maybeProgressBar(wp: ?*c.window_pane, buf: []const u8) void {
    if (buf.len < 5 or buf[0] != '9' or buf[1] != ';' or buf[2] != '4' or buf[3] != ';')
        return;

    var i: usize = 4;
    if (buf[i] < '0' or buf[i] > '4')
        return;
    const state: c.enum_progress_bar_state = @intCast(buf[i] - '0');
    i += 1;

    if (i == buf.len or (buf[i] == ';' and i + 1 == buf.len)) {
        setProgressBar(wp, state, -1);
        return;
    }
    if (buf[i] != ';')
        return;
    i += 1;

    var progress: c_int = 0;
    while (i < buf.len and std.ascii.isDigit(buf[i])) : (i += 1) {
        if (progress > 100)
            return;
        progress = progress * 10 + @as(c_int, buf[i] - '0');
    }
    if (i != buf.len or progress > 100)
        return;
    setProgressBar(wp, state, progress);
}

fn maybeClipboard(wp: ?*c.window_pane, buf: []const u8, input_end: c_int) void {
    const pane = wp orelse return;
    if (buf.len < 5 or buf[0] != '5' or buf[1] != '2' or buf[2] != ';')
        return;
    if (c.options_get_number(c.global_options, "set-clipboard") != 2)
        return;

    const allow = "cpqs01234567";
    var clip = std.mem.zeroes([allow.len + 1]u8);
    var clip_len: usize = 0;
    var i: usize = 3;
    while (i < buf.len and buf[i] != ';') : (i += 1) {
        if (std.mem.indexOfScalar(u8, allow, buf[i]) != null and
            std.mem.indexOfScalar(u8, clip[0..clip_len], buf[i]) == null)
        {
            clip[clip_len] = buf[i];
            clip_len += 1;
        }
    }
    if (i == buf.len or i + 1 == buf.len)
        return;

    const data = buf[i + 1 ..];
    if (data.len == 1 and data[0] == '?') {
        switch (c.options_get_number(c.global_options, "get-clipboard")) {
            0 => return,
            1 => {
                const pb = c.paste_get_top(null) orelse return;
                var pblen: usize = 0;
                const pbdata = c.paste_buffer_data(pb, &pblen);
                const end = if (input_end == 1) "\x07" else "\x1b\\";
                c.input_reply_clipboard(pane.*.event, pbdata, pblen, end, clip[0]);
                return;
            },
            else => {
                _ = c.input_request_clipboard(pane, input_end);
                return;
            },
        }
    }

    const copy = allocator.dupeZ(u8, data) catch return;
    defer allocator.free(copy);

    const out_len = ((data.len + 3) / 4) * 3;
    if (out_len == 0)
        return;
    const out = allocator.alloc(u8, out_len) catch return;
    const decoded = c.b64_pton(copy.ptr, out.ptr, out.len);
    if (decoded == -1) {
        allocator.free(out);
        return;
    }

    var ctx: c.screen_write_ctx = undefined;
    c.screen_write_start_pane(&ctx, pane, null);
    c.screen_write_setselection(&ctx, clip[0..clip_len :0].ptr, out.ptr, @intCast(decoded));
    c.screen_write_stop(&ctx);
    c.notify_pane("pane-set-clipboard", pane);
    c.paste_add(null, @ptrCast(out.ptr), @intCast(decoded));
}

// Write an OSC 10/11/12 or OSC 4 colour reply straight back to the
// pane's pty in the same rgb:RRRR/GGGG/BBBB form input_osc_colour_reply
// uses. Sends nothing for an unknown colour, matching stock tmux.
fn colorReplyOsc(pane: *c.window_pane, n: u32, idx: ?u32, col: c_int, input_end: c_int) void {
    if (pane.*.event == null or col == -1)
        return;
    const rgb = c.colour_force_rgb(col);
    if (rgb == -1)
        return;
    var r: u8 = 0;
    var g: u8 = 0;
    var b: u8 = 0;
    c.colour_split_rgb(rgb, &r, &g, &b);
    const end = if (input_end == 1) "\x07" else "\x1b\\";
    var buf: [64]u8 = undefined;
    const reply = if (idx) |ix|
        std.fmt.bufPrint(&buf, "\x1b]{d};{d};rgb:{x:0>2}{x:0>2}/{x:0>2}{x:0>2}/{x:0>2}{x:0>2}{s}", .{ n, ix, r, r, g, g, b, b, end }) catch return
    else
        std.fmt.bufPrint(&buf, "\x1b]{d};rgb:{x:0>2}{x:0>2}/{x:0>2}{x:0>2}/{x:0>2}{x:0>2}{s}", .{ n, r, r, g, g, b, b, end }) catch return;
    _ = c.bufferevent_write(pane.*.event, reply.ptr, reply.len);
}

fn isColorQuery(rest: []const u8) bool {
    return rest.len >= 1 and rest[0] == '?';
}

// Answer OSC 10/11/12 and OSC 4;n colour QUERIES with tmux's colour for
// this pane. ghostty ignores colour queries and input.c is bypassed for
// ghostty panes, so otherwise an app probing the fg/bg (e.g. neovim's
// `background` autodetect, fzf, delta) gets no reply at all.
fn maybeColorQuery(wp: ?*c.window_pane, buf: []const u8, input_end: c_int) void {
    const pane = wp orelse return;

    var i: usize = 0;
    var code: u32 = 0;
    if (i >= buf.len or !std.ascii.isDigit(buf[i]))
        return;
    while (i < buf.len and std.ascii.isDigit(buf[i])) : (i += 1)
        code = code * 10 + @as(u32, buf[i] - '0');
    if (i >= buf.len or buf[i] != ';')
        return;
    i += 1;

    switch (code) {
        10 => if (isColorQuery(buf[i..]))
            colorReplyOsc(pane, 10, null, c.window_pane_get_fg(pane), input_end),
        11 => if (isColorQuery(buf[i..]))
            colorReplyOsc(pane, 11, null, c.window_pane_get_bg(pane), input_end),
        12 => if (isColorQuery(buf[i..]))
            colorReplyOsc(pane, 12, null, pane.*.base.default_ccolour, input_end),
        4 => {
            var idx: u32 = 0;
            if (i >= buf.len or !std.ascii.isDigit(buf[i]))
                return;
            while (i < buf.len and std.ascii.isDigit(buf[i])) : (i += 1)
                idx = idx * 10 + @as(u32, buf[i] - '0');
            if (idx > 255 or i >= buf.len or buf[i] != ';')
                return;
            i += 1;
            if (isColorQuery(buf[i..]))
                colorReplyOsc(pane, 4, idx, c.colour_palette_get(&pane.*.palette, @intCast(idx)), input_end);
        },
        else => {},
    }
}

fn resetOsc(gvt: *GhosttyVT) void {
    gvt.osc_len = 0;
    gvt.osc_active = false;
    gvt.osc_esc = false;
}

fn freeOsc(gvt: *GhosttyVT) void {
    if (gvt.osc_buf) |buf|
        allocator.free(buf[0..gvt.osc_cap]);
    gvt.osc_buf = null;
    gvt.osc_cap = 0;
    resetOsc(gvt);
    gvt.osc_pending_esc = false;
}

fn appendOsc(gvt: *GhosttyVT, ch: u8) bool {
    if (gvt.osc_len == max_osc_len)
        return false;

    if (gvt.osc_len == gvt.osc_cap) {
        const new_cap = @min(max_osc_len, @max(@as(usize, 64), gvt.osc_cap * 2));
        if (gvt.osc_buf) |buf| {
            const next = allocator.realloc(buf[0..gvt.osc_cap], new_cap) catch return false;
            gvt.osc_buf = next.ptr;
        } else {
            const next = allocator.alloc(u8, new_cap) catch return false;
            gvt.osc_buf = next.ptr;
        }
        gvt.osc_cap = new_cap;
    }

    gvt.osc_buf.?[gvt.osc_len] = ch;
    gvt.osc_len += 1;
    return true;
}

fn finishOsc(gvt: *GhosttyVT, input_end: c_int) void {
    if (gvt.osc_buf) |buf| {
        const osc = buf[0..gvt.osc_len];
        maybeProgressBar(gvt.wp, osc);
        maybeClipboard(gvt.wp, osc, input_end);
        maybeColorQuery(gvt.wp, osc, input_end);
    }
    resetOsc(gvt);
}

fn resetClearScan(gvt: *GhosttyVT, state: ClearScanState) void {
    gvt.clear_state = state;
    gvt.clear_param = 0;
    gvt.clear_param_active = false;
    gvt.clear_param_done = false;
}

fn scanClearScreen(gvt: *GhosttyVT, buf: []const u8) void {
    for (buf) |ch| {
        switch (gvt.clear_state) {
            .ground => {
                if (ch == 0x1b)
                    resetClearScan(gvt, .esc);
            },
            .esc => {
                if (ch == '[')
                    resetClearScan(gvt, .csi)
                else if (ch != 0x1b)
                    resetClearScan(gvt, .ground);
            },
            .csi => {
                if (std.ascii.isDigit(ch)) {
                    if (!gvt.clear_param_done) {
                        gvt.clear_param_active = true;
                        if (gvt.clear_param < 1000)
                            gvt.clear_param = gvt.clear_param * 10 + ch - '0';
                    }
                } else if (ch == ';') {
                    gvt.clear_param_done = true;
                } else if (ch == 'J') {
                    if (gvt.clear_param_active and (gvt.clear_param == 2 or gvt.clear_param == 3))
                        gvt.screen_cleared = true;
                    resetClearScan(gvt, .ground);
                } else if (ch == 0x1b) {
                    resetClearScan(gvt, .esc);
                } else if (ch >= 0x40 and ch <= 0x7e) {
                    resetClearScan(gvt, .ground);
                }
            },
        }
    }
}

fn scanOscSideEffects(gvt: *GhosttyVT, buf: []const u8) void {
    for (buf) |ch| {
        if (!gvt.osc_active) {
            if (gvt.osc_pending_esc) {
                gvt.osc_pending_esc = false;
                if (ch == ']') {
                    resetOsc(gvt);
                    gvt.osc_active = true;
                    continue;
                }
            }
            if (ch == 0x1b)
                gvt.osc_pending_esc = true;
            continue;
        }

        if (gvt.osc_esc) {
            gvt.osc_esc = false;
            if (ch == '\\') {
                finishOsc(gvt, 0);
                continue;
            }
            resetOsc(gvt);
            if (ch == ']') {
                gvt.osc_active = true;
                continue;
            }
            continue;
        }

        if (ch == 0x18 or ch == 0x1a) {
            resetOsc(gvt);
            continue;
        }
        if (ch == 0x07) {
            finishOsc(gvt, 1);
            continue;
        }
        if (ch == 0x1b) {
            gvt.osc_esc = true;
            continue;
        }
        if (!appendOsc(gvt, ch))
            resetOsc(gvt);
    }
}

fn writePtyCb(_: c.GhosttyTerminal, userdata: ?*anyopaque, data: [*c]const u8, len: usize) callconv(.c) void {
    const wp = asPane(userdata) orelse return;
    if (wp.*.event == null or len == 0)
        return;
    _ = c.bufferevent_write(wp.*.event, data, len);
}

fn titleChangedCb(terminal: c.GhosttyTerminal, userdata: ?*anyopaque) callconv(.c) void {
    const wp = asPane(userdata) orelse return;
    var title: c.GhosttyString = undefined;
    if (c.ghostty_terminal_get(terminal, c.GHOSTTY_TERMINAL_DATA_TITLE, &title) != c.GHOSTTY_SUCCESS or title.len == 0)
        return;

    const copy = allocator.dupeZ(u8, title.ptr[0..title.len]) catch return;
    defer allocator.free(copy);
    if (c.screen_set_title(&wp.*.base, copy.ptr, 1) != 0)
        wp.*.flags |= c.PANE_NEWSTATUS;
}

fn pwdChangedCb(terminal: c.GhosttyTerminal, userdata: ?*anyopaque) callconv(.c) void {
    const wp = asPane(userdata) orelse return;
    var pwd: c.GhosttyString = undefined;
    if (c.ghostty_terminal_get(terminal, c.GHOSTTY_TERMINAL_DATA_PWD, &pwd) != c.GHOSTTY_SUCCESS or pwd.len == 0)
        return;

    const copy = allocator.dupeZ(u8, pwd.ptr[0..pwd.len]) catch return;
    defer allocator.free(copy);
    if (c.screen_set_path(&wp.*.base, copy.ptr, 1) != 0) {
        if (wp.*.window) |window| {
            c.server_redraw_window_borders(window);
            c.server_status_window(window);
        }
    }
}

fn bellCb(_: c.GhosttyTerminal, userdata: ?*anyopaque) callconv(.c) void {
    const wp = asPane(userdata) orelse return;
    if (wp.*.window) |window|
        c.alerts_queue(window, c.WINDOW_BELL);
}

fn enquiryCb(_: c.GhosttyTerminal, _: ?*anyopaque) callconv(.c) c.GhosttyString {
    return .{ .ptr = null, .len = 0 };
}

fn xtversionCb(_: c.GhosttyTerminal, _: ?*anyopaque) callconv(.c) c.GhosttyString {
    return .{ .ptr = version.ptr, .len = version.len };
}

fn sizeCb(_: c.GhosttyTerminal, userdata: ?*anyopaque, out_size: ?*c.GhosttySizeReportSize) callconv(.c) bool {
    const wp = asPane(userdata) orelse return false;
    const size = out_size orelse return false;

    size.*.rows = @intCast(wp.*.sy);
    size.*.columns = @intCast(wp.*.sx);
    if (wp.*.window) |window| {
        size.*.cell_width = window.*.xpixel;
        size.*.cell_height = window.*.ypixel;
    } else {
        size.*.cell_width = 0;
        size.*.cell_height = 0;
    }
    return true;
}

fn colorSchemeCb(_: c.GhosttyTerminal, userdata: ?*anyopaque, out_scheme: ?*c.GhosttyColorScheme) callconv(.c) bool {
    const scheme = out_scheme orelse return false;
    const wp = asPane(userdata);
    scheme.* = if (wp != null and c.window_pane_get_theme(wp.?) == c.THEME_LIGHT)
        c.GHOSTTY_COLOR_SCHEME_LIGHT
    else
        c.GHOSTTY_COLOR_SCHEME_DARK;
    return true;
}

fn deviceAttributesCb(_: c.GhosttyTerminal, _: ?*anyopaque, out_attrs: ?*c.GhosttyDeviceAttributes) callconv(.c) bool {
    const attrs = out_attrs orelse return false;
    attrs.* = std.mem.zeroes(c.GhosttyDeviceAttributes);
    attrs.*.primary.conformance_level = c.GHOSTTY_DA_CONFORMANCE_VT100;
    attrs.*.primary.features[attrs.*.primary.num_features] = 2;
    attrs.*.primary.num_features += 1;
    attrs.*.secondary.device_type = 84;
    return true;
}

fn mapAttr(style: *const c.GhosttyStyle) c_ushort {
    var attr: c_ushort = 0;
    if (style.bold) attr |= c.GRID_ATTR_BRIGHT;
    if (style.faint) attr |= c.GRID_ATTR_DIM;
    if (style.italic) attr |= c.GRID_ATTR_ITALICS;
    if (style.blink) attr |= c.GRID_ATTR_BLINK;
    if (style.inverse) attr |= c.GRID_ATTR_REVERSE;
    if (style.invisible) attr |= c.GRID_ATTR_HIDDEN;
    if (style.strikethrough) attr |= c.GRID_ATTR_STRIKETHROUGH;
    if (style.overline) attr |= c.GRID_ATTR_OVERLINE;

    switch (style.underline) {
        c.GHOSTTY_SGR_UNDERLINE_SINGLE => attr |= c.GRID_ATTR_UNDERSCORE,
        c.GHOSTTY_SGR_UNDERLINE_DOUBLE => attr |= c.GRID_ATTR_UNDERSCORE_2,
        c.GHOSTTY_SGR_UNDERLINE_CURLY => attr |= c.GRID_ATTR_UNDERSCORE_3,
        c.GHOSTTY_SGR_UNDERLINE_DOTTED => attr |= c.GRID_ATTR_UNDERSCORE_4,
        c.GHOSTTY_SGR_UNDERLINE_DASHED => attr |= c.GRID_ATTR_UNDERSCORE_5,
        else => {},
    }
    return attr;
}

fn mapColor(sc: *const c.GhosttyStyleColor, default_col: c_int) c_int {
    return switch (sc.tag) {
        c.GHOSTTY_STYLE_COLOR_RGB => c.colour_join_rgb(sc.value.rgb.r, sc.value.rgb.g, sc.value.rgb.b),
        c.GHOSTTY_STYLE_COLOR_PALETTE => @as(c_int, sc.value.palette) | c.COLOUR_FLAG_256,
        else => default_col,
    };
}

// Truncate to the last complete codepoint that fits in `max` bytes.
fn utf8Truncate(buf: []const u8, max: usize) []const u8 {
    if (buf.len <= max)
        return buf;
    var end: usize = 0;
    while (end < buf.len) {
        const l = std.unicode.utf8ByteSequenceLength(buf[end]) catch break;
        if (end + l > max or end + l > buf.len)
            break;
        end += l;
    }
    return buf[0..end];
}

// Cell width comes from ghostty's wide-cell data (finishCellWide
// widens to 2 or marks padding), never from wcwidth: ghostty already
// applied its width tables during parsing, and its cursor arithmetic
// is the one the application observed.
fn setCellText(gc: *c.grid_cell, grapheme: []const u8) void {
    const utf8_buf = utf8Truncate(grapheme, c.UTF8_SIZE);
    if (utf8_buf.len == 0) {
        c.utf8_set(&gc.*.data, ' ');
        return;
    }
    @memcpy(gc.*.data.data[0..utf8_buf.len], utf8_buf);
    gc.*.data.size = @intCast(utf8_buf.len);
    gc.*.data.have = @intCast(utf8_buf.len);
    gc.*.data.width = 1;
}

fn setCellCodepoint(gc: *c.grid_cell, cp: u32) void {
    if (cp == 0 or cp > std.math.maxInt(u21)) {
        c.utf8_set(&gc.*.data, ' ');
        return;
    }
    if (cp < 0x80) {
        c.utf8_set(&gc.*.data, @intCast(cp));
        return;
    }
    const len = std.unicode.utf8Encode(@intCast(cp), gc.*.data.data[0..4]) catch {
        c.utf8_set(&gc.*.data, ' ');
        return;
    };
    gc.*.data.size = len;
    gc.*.data.have = len;
    gc.*.data.width = 1;
}

// Bit layout of a ghostty cell (terminal/page.zig `Cell` at the pinned
// library rev). The C API treats the u64 as opaque, so every decoded
// cell is validated against ghostty_cell_get_multi for the first few
// thousand reads and the decoder is disabled on the first mismatch.
const CellBits = packed struct(u64) {
    content_tag: u2,
    content: u24,
    style_id: u16,
    wide: u2,
    protected: u1,
    hyperlink: u1,
    semantic_content: u2,
    _padding: u16,
};

const CellDecode = enum { validating, bits, ffi };
var cell_decode: CellDecode = .validating;
var cell_decode_validated: u32 = 0;
const cell_decode_validate_max = 4096;

// The interesting bits of a raw cell, extracted once so the expensive
// style/grapheme/hyperlink lookups run only for cells that carry them.
const RawCell = struct {
    raw: c.GhosttyCell,
    tag: c.GhosttyCellContentTag,
    cp: u32,
    wide: c.GhosttyCellWide,
    style_id: u16,
    has_styling: bool,
    has_hyperlink: bool,

    const read_keys = [_]c.GhosttyCellData{
        c.GHOSTTY_CELL_DATA_CONTENT_TAG,
        c.GHOSTTY_CELL_DATA_CODEPOINT,
        c.GHOSTTY_CELL_DATA_WIDE,
        c.GHOSTTY_CELL_DATA_STYLE_ID,
        c.GHOSTTY_CELL_DATA_HAS_STYLING,
        c.GHOSTTY_CELL_DATA_HAS_HYPERLINK,
    };

    fn readFfi(raw: c.GhosttyCell) RawCell {
        var rc = RawCell{
            .raw = raw,
            .tag = c.GHOSTTY_CELL_CONTENT_CODEPOINT,
            .cp = 0,
            .wide = c.GHOSTTY_CELL_WIDE_NARROW,
            .style_id = 0,
            .has_styling = false,
            .has_hyperlink = false,
        };
        var values = [read_keys.len]?*anyopaque{
            &rc.tag, &rc.cp, &rc.wide, &rc.style_id, &rc.has_styling, &rc.has_hyperlink,
        };
        _ = c.ghostty_cell_get_multi(raw, read_keys.len, &read_keys, &values, null);
        return rc;
    }

    fn readBits(raw: c.GhosttyCell) RawCell {
        const bits: CellBits = @bitCast(raw);
        const is_text = bits.content_tag == 0 or bits.content_tag == 1;
        return .{
            .raw = raw,
            .tag = bits.content_tag,
            .cp = if (is_text) bits.content & 0x1fffff else 0,
            .wide = bits.wide,
            .style_id = bits.style_id,
            .has_styling = bits.style_id != 0,
            .has_hyperlink = bits.hyperlink != 0,
        };
    }

    fn read(raw: c.GhosttyCell) RawCell {
        return switch (cell_decode) {
            .bits => readBits(raw),
            .ffi => readFfi(raw),
            .validating => readValidating(raw),
        };
    }

    fn readValidating(raw: c.GhosttyCell) RawCell {
        @branchHint(.cold);
        const rc = readBits(raw);
        const ffi = readFfi(raw);
        if (rc.tag != ffi.tag or rc.cp != ffi.cp or rc.wide != ffi.wide or
            rc.style_id != ffi.style_id or
            rc.has_styling != ffi.has_styling or rc.has_hyperlink != ffi.has_hyperlink)
        {
            cell_decode = .ffi;
            log_debug("%s: cell bit-decode mismatch, using FFI reads", "ghostty-vt");
            return ffi;
        }
        cell_decode_validated += 1;
        if (cell_decode_validated >= cell_decode_validate_max)
            cell_decode = .bits;
        return rc;
    }

    fn isDefault(rc: RawCell) bool {
        return rc.tag == c.GHOSTTY_CELL_CONTENT_CODEPOINT and rc.cp == 0 and
            !rc.has_styling and !rc.has_hyperlink and
            rc.wide == c.GHOSTTY_CELL_WIDE_NARROW;
    }
};

fn applyCellStyle(gc: *c.grid_cell, style: *const c.GhosttyStyle) void {
    gc.*.attr = mapAttr(style);
    gc.*.fg = mapColor(&style.fg_color, 8);
    gc.*.bg = mapColor(&style.bg_color, 8);
    gc.*.us = mapColor(&style.underline_color, 8);
}

// Direct-mapped cache of mapped styles, keyed by (page node, style id).
// Style ids are only stable while the terminal is not mutating, so
// entries are stamped with a generation that sync() bumps per pass.
const StyleCacheEntry = struct {
    node: ?*anyopaque = null,
    style_id: u16 = 0,
    gen: u32 = 0,
    attr: c_ushort = 0,
    fg: c_int = 0,
    bg: c_int = 0,
    us: c_int = 0,
};
var style_cache: [512]StyleCacheEntry = @splat(.{});
var style_cache_gen: u32 = 0;

fn bumpStyleCacheGen() void {
    style_cache_gen +%= 1;
    if (style_cache_gen == 0)
        style_cache = @splat(.{});
}

fn applyCachedStyle(gc: *c.grid_cell, ref: *const c.GhosttyGridRef, style_id: u16) void {
    const idx = (style_id ^ (@intFromPtr(ref.node) >> 6)) & (style_cache.len - 1);
    const e = &style_cache[idx];
    if (e.gen == style_cache_gen and e.node == ref.node and e.style_id == style_id) {
        gc.*.attr = e.attr;
        gc.*.fg = e.fg;
        gc.*.bg = e.bg;
        gc.*.us = e.us;
        return;
    }
    var style = std.mem.zeroes(c.GhosttyStyle);
    style.size = @sizeOf(c.GhosttyStyle);
    if (c.ghostty_grid_ref_style(ref, &style) == c.GHOSTTY_SUCCESS)
        applyCellStyle(gc, &style);
    e.* = .{
        .node = ref.node,
        .style_id = style_id,
        .gen = style_cache_gen,
        .attr = gc.*.attr,
        .fg = gc.*.fg,
        .bg = gc.*.bg,
        .us = gc.*.us,
    };
}

// Content and wide/hyperlink-independent finishing shared by the
// viewport and history builders.
fn finishCellWide(gc: *c.grid_cell, rc: RawCell) void {
    if (rc.wide == c.GHOSTTY_CELL_WIDE_WIDE) {
        gc.*.data.width = 2;
    } else if (rc.wide == c.GHOSTTY_CELL_WIDE_SPACER_TAIL) {
        gc.*.flags |= c.GRID_FLAG_PADDING;
        c.utf8_set(&gc.*.data, 0);
    }
}

fn applyCellBgContent(gc: *c.grid_cell, rc: RawCell) void {
    c.utf8_set(&gc.*.data, ' ');
    switch (rc.tag) {
        c.GHOSTTY_CELL_CONTENT_BG_COLOR_PALETTE => {
            var idx: c.GhosttyColorPaletteIndex = 0;
            if (c.ghostty_cell_get(rc.raw, c.GHOSTTY_CELL_DATA_COLOR_PALETTE, &idx) == c.GHOSTTY_SUCCESS)
                gc.*.bg = @as(c_int, idx) | c.COLOUR_FLAG_256;
        },
        c.GHOSTTY_CELL_CONTENT_BG_COLOR_RGB => {
            var rgb = std.mem.zeroes(c.GhosttyColorRgb);
            if (c.ghostty_cell_get(rc.raw, c.GHOSTTY_CELL_DATA_COLOR_RGB, &rgb) == c.GHOSTTY_SUCCESS)
                gc.*.bg = c.colour_join_rgb(rgb.r, rgb.g, rgb.b);
        },
        else => {},
    }
}

fn utf8FromGridRef(ref: *const c.GhosttyGridRef, out: []u8) []const u8 {
    var codepoints: [c.UTF8_SIZE]u32 = undefined;
    var codepoints_len: usize = 0;
    if (c.ghostty_grid_ref_graphemes(ref, &codepoints, codepoints.len, &codepoints_len) != c.GHOSTTY_SUCCESS)
        return out[0..0];

    var out_len: usize = 0;
    for (codepoints[0..@min(codepoints_len, codepoints.len)]) |codepoint| {
        if (codepoint > std.math.maxInt(u21) or out_len + 4 > out.len)
            break;
        out_len += std.unicode.utf8Encode(@intCast(codepoint), out[out_len..]) catch break;
    }
    return out[0..out_len];
}

fn applyHyperlink(s: *c.screen, gc: *c.grid_cell, ref: *const c.GhosttyGridRef) void {
    var uri_buf: [max_hyperlink_uri]u8 = undefined;
    var uri_len: usize = 0;
    if (c.ghostty_grid_ref_hyperlink_uri(ref, &uri_buf, uri_buf.len, &uri_len) != c.GHOSTTY_SUCCESS or uri_len == 0)
        return;

    if (s.*.hyperlinks == null)
        c.screen_reset_hyperlinks(s);
    const uri = allocator.dupeZ(u8, uri_buf[0..uri_len]) catch return;
    defer allocator.free(uri);
    gc.*.link = c.hyperlinks_put(s.*.hyperlinks, uri.ptr, uri.ptr);
}

fn pointGridRef(gvt: *GhosttyVT, tag: c.GhosttyPointTag, x: c_uint, y: usize, ref: *c.GhosttyGridRef) bool {
    if (y > std.math.maxInt(u32))
        return false;

    var point = std.mem.zeroes(c.GhosttyPoint);
    point.tag = tag;
    point.value.coordinate.x = @intCast(x);
    point.value.coordinate.y = @intCast(y);

    ref.* = std.mem.zeroes(c.GhosttyGridRef);
    ref.*.size = @sizeOf(c.GhosttyGridRef);
    return c.ghostty_terminal_grid_ref(gvt.terminal, point, ref) == c.GHOSTTY_SUCCESS;
}

// Build a tmux cell from an already-fetched raw cell, consulting the
// grid ref only for data the raw cell cannot provide (style, grapheme
// cluster, hyperlink). Returns true when the result is the default
// blank cell, which callers may skip storing.
fn buildCellFromRaw(s: *c.screen, gc: *c.grid_cell, ref: *const c.GhosttyGridRef, rc: RawCell) bool {
    if (rc.isDefault()) {
        gc.* = c.grid_default_cell;
        return true;
    }

    gc.* = std.mem.zeroes(c.grid_cell);
    gc.*.fg = 8;
    gc.*.bg = 8;
    gc.*.us = 8;

    if (rc.has_styling)
        applyCachedStyle(gc, ref, rc.style_id);

    if (rc.tag == c.GHOSTTY_CELL_CONTENT_CODEPOINT_GRAPHEME) {
        var utf8_buf: [c.UTF8_SIZE * 4]u8 = undefined;
        setCellText(gc, utf8FromGridRef(ref, &utf8_buf));
    } else if (rc.tag == c.GHOSTTY_CELL_CONTENT_CODEPOINT) {
        setCellCodepoint(gc, rc.cp);
    } else {
        applyCellBgContent(gc, rc);
    }

    if (rc.has_hyperlink)
        applyHyperlink(s, gc, ref);

    finishCellWide(gc, rc);
    return false;
}

fn syncMode(gvt: *GhosttyVT, s: *c.screen, ghostty_mode: c.GhosttyMode, tmux_mode: c_int) void {
    var val = false;
    if (c.ghostty_terminal_mode_get(gvt.terminal, ghostty_mode, &val) == c.GHOSTTY_SUCCESS and val)
        s.*.mode |= tmux_mode;
}

fn ghosttyMode(value: u16, ansi: bool) c.GhosttyMode {
    return c.ghostty_mode_new(value, ansi);
}

fn syncModes(gvt: *GhosttyVT, s: *c.screen) void {
    // Clear only the modes ghostty tracks, then re-derive them. Bits
    // tmux manages itself must survive: zeroing s->mode wiped
    // MODE_KEYS_EXTENDED (from the extended-keys option),
    // MODE_CURSOR_BLINKING_SET / MODE_CURSOR_VERY_VISIBLE (from DECSCUSR),
    // MODE_THEME_UPDATES, and MODE_SYNC's timer-backed bit.
    const owned: c_int = c.MODE_CURSOR | c.MODE_INSERT | c.MODE_WRAP |
        c.MODE_ORIGIN | c.MODE_CURSOR_BLINKING | c.MODE_BRACKETPASTE |
        c.MODE_FOCUSON | c.MODE_CRLF | c.MODE_KKEYPAD | c.MODE_KCURSOR |
        c.MODE_MOUSE_STANDARD | c.MODE_MOUSE_BUTTON | c.MODE_MOUSE_ALL |
        c.MODE_MOUSE_UTF8 | c.MODE_MOUSE_SGR | c.MODE_KEYS_EXTENDED_2;
    s.*.mode &= ~owned;

    syncMode(gvt, s, ghosttyMode(25, false), c.MODE_CURSOR);
    syncMode(gvt, s, ghosttyMode(4, true), c.MODE_INSERT);
    syncMode(gvt, s, ghosttyMode(7, false), c.MODE_WRAP);
    syncMode(gvt, s, ghosttyMode(6, false), c.MODE_ORIGIN);
    syncMode(gvt, s, ghosttyMode(12, false), c.MODE_CURSOR_BLINKING);
    syncMode(gvt, s, ghosttyMode(2004, false), c.MODE_BRACKETPASTE);
    syncMode(gvt, s, ghosttyMode(1004, false), c.MODE_FOCUSON);
    syncMode(gvt, s, ghosttyMode(20, true), c.MODE_CRLF);
    syncMode(gvt, s, ghosttyMode(66, false), c.MODE_KKEYPAD);
    syncMode(gvt, s, ghosttyMode(1, false), c.MODE_KCURSOR);
    syncMode(gvt, s, ghosttyMode(1000, false), c.MODE_MOUSE_STANDARD);
    syncMode(gvt, s, ghosttyMode(1002, false), c.MODE_MOUSE_BUTTON);
    syncMode(gvt, s, ghosttyMode(1003, false), c.MODE_MOUSE_ALL);
    syncMode(gvt, s, ghosttyMode(1005, false), c.MODE_MOUSE_UTF8);
    syncMode(gvt, s, ghosttyMode(1006, false), c.MODE_MOUSE_SGR);

    // MODE_SYNC (mode 2026) is deliberately not mirrored: tmux's own sync
    // bit is backed by a 1s escape-hatch timer (screen_write_start_sync);
    // setting the raw bit here with no timer can wedge cursor/redraw.

    // Reflect the kitty keyboard protocol negotiated inside ghostty. It
    // asks for every key in the enhanced form, so map it to tmux's
    // "report everything extended" mode (MODE_KEYS_EXTENDED_2, see
    // input-keys.c). modifyOtherKeys - what vim uses - is not exposed by
    // the ghostty C API and so cannot be mirrored yet.
    var kkflags: u8 = 0;
    if (c.ghostty_terminal_get(gvt.terminal, c.GHOSTTY_TERMINAL_DATA_KITTY_KEYBOARD_FLAGS, &kkflags) == c.GHOSTTY_SUCCESS and kkflags != 0)
        s.*.mode |= c.MODE_KEYS_EXTENDED_2;
}

fn syncHistoryRow(gvt: *GhosttyVT, s: *c.screen, history_y: usize, target_y: c_uint) void {
    const grid = s.*.grid orelse return;
    var ref: c.GhosttyGridRef = undefined;

    resetRowFlags(grid, target_y);
    // One page lookup per row: a grid ref is a transparent (node, x, y)
    // snapshot, so stepping x along the row reuses the resolved node
    // instead of paying the full lookup for every cell.
    if (!pointGridRef(gvt, c.GHOSTTY_POINT_TAG_HISTORY, 0, history_y, &ref))
        return;
    var row: c.GhosttyRow = 0;
    if (c.ghostty_grid_ref_row(&ref, &row) == c.GHOSTTY_SUCCESS)
        syncRowFlagsLine(grid, target_y, row);

    syncRowCellsFromRef(s, grid, &ref, target_y);
}

// Mirror one ghostty row into the tmux grid line at absolute row
// `line_y`, stepping the resolved grid ref across the columns. Default
// cells are skipped when the tmux line does not store that column yet
// (grid_expand_line fills gaps with the default cell), which keeps
// mostly-blank lines short on the tmux side.
// A pending run of adjacent single-byte cells sharing one style,
// flushed to the tmux grid with a single grid_set_cells call.
const Run = struct {
    start: c_uint = 0,
    len: usize = 0,
    style_id: u16 = 0,
    gc: c.grid_cell = undefined,
    chars: [512]u8 = undefined,

    fn flush(run: *Run, grid: *c.grid, line_y: c_uint) void {
        if (run.len == 0)
            return;
        c.grid_set_cells(grid, run.start, line_y, &run.gc, &run.chars, run.len);
        run.len = 0;
    }
};

fn syncRowCellsFromRef(s: *c.screen, grid: *c.grid, ref: *c.GhosttyGridRef, line_y: c_uint) void {
    const line = c.grid_get_line(grid, line_y);
    const run_cells = cell_decode == .bits;
    var run = Run{};
    var px: c_uint = 0;
    while (px < grid.*.sx) : (px += 1) {
        ref.x = @intCast(px);
        var raw: c.GhosttyCell = 0;
        if (c.ghostty_grid_ref_cell(ref, &raw) != c.GHOSTTY_SUCCESS)
            continue;

        // An all-zero cell is the default cell; when the tmux line does
        // not store that column either, there is nothing to do.
        if (raw == 0 and cell_decode != .ffi) {
            run.flush(grid, line_y);
            if (px < line.*.cellsize)
                c.grid_set_cell(grid, px, line_y, &c.grid_default_cell);
            continue;
        }

        if (run_cells) {
            const bits: CellBits = @bitCast(raw);
            const cp = bits.content & 0x1fffff;
            if (bits.content_tag == 0 and bits.wide == 0 and
                bits.hyperlink == 0 and cp >= 0x20 and cp < 0x7f)
            {
                if (run.len != 0 and
                    (bits.style_id != run.style_id or run.len == run.chars.len))
                    run.flush(grid, line_y);
                if (run.len == 0) {
                    run.start = px;
                    run.style_id = bits.style_id;
                    run.gc = std.mem.zeroes(c.grid_cell);
                    run.gc.fg = 8;
                    run.gc.bg = 8;
                    run.gc.us = 8;
                    c.utf8_set(&run.gc.data, ' ');
                    if (bits.style_id != 0)
                        applyCachedStyle(&run.gc, ref, bits.style_id);
                }
                run.chars[run.len] = @intCast(cp);
                run.len += 1;
                continue;
            }
        }

        run.flush(grid, line_y);
        var gc: c.grid_cell = undefined;
        if (buildCellFromRaw(s, &gc, ref, RawCell.read(raw)) and
            px >= line.*.cellsize)
            continue;
        c.grid_set_cell(grid, px, line_y, &gc);
    }
    run.flush(grid, line_y);
}

fn historyRowChanged(gvt: *GhosttyVT, s: *c.screen, history_y: usize, target_y: c_uint) bool {
    const grid = s.*.grid orelse return false;
    var ref: c.GhosttyGridRef = undefined;

    if (!pointGridRef(gvt, c.GHOSTTY_POINT_TAG_HISTORY, 0, history_y, &ref))
        return false;

    var row: c.GhosttyRow = 0;
    var wrapped = false;
    if (c.ghostty_grid_ref_row(&ref, &row) == c.GHOSTTY_SUCCESS and
        c.ghostty_row_get(row, c.GHOSTTY_ROW_DATA_WRAP, &wrapped) == c.GHOSTTY_SUCCESS)
    {
        const line = c.grid_get_line(grid, target_y);
        if (((line.*.flags & c.GRID_LINE_WRAPPED) != 0) != wrapped)
            return true;
    }

    var px: c_uint = 0;
    while (px < grid.*.sx) : (px += 1) {
        ref.x = @intCast(px);
        var raw: c.GhosttyCell = 0;
        if (c.ghostty_grid_ref_cell(&ref, &raw) != c.GHOSTTY_SUCCESS)
            continue;

        var old_gc: c.grid_cell = undefined;
        var new_gc: c.grid_cell = undefined;
        c.grid_get_cell(grid, px, target_y, &old_gc);
        _ = buildCellFromRaw(s, &new_gc, &ref, RawCell.read(raw));
        if (c.grid_cells_equal(&old_gc, &new_gc) == 0)
            return true;
    }
    return false;
}

fn importHistoryRows(gvt: *GhosttyVT, s: *c.screen, grid: *c.grid, from: usize, to: usize) void {
    var history_y = from;
    while (history_y < to) : (history_y += 1) {
        if (grid.*.hlimit != 0 and grid.*.hsize >= grid.*.hlimit)
            c.grid_collect_history(grid, 0);
        const target_y = grid.*.hsize;
        c.grid_scroll_history(grid, 8);
        syncHistoryRow(gvt, s, history_y, target_y);
    }
    setHistAnchor(gvt, to);
}

// Anchor a tracked grid ref to the newest imported history row. The
// library moves it as the scrollback ring shifts, so the next sync can
// read the shift count from the anchor's position instead of comparing
// row content.
fn setHistAnchor(gvt: *GhosttyVT, scrollback_rows: usize) void {
    if (scrollback_rows == 0 or scrollback_rows - 1 > std.math.maxInt(u32))
        return;
    var point = std.mem.zeroes(c.GhosttyPoint);
    point.tag = c.GHOSTTY_POINT_TAG_HISTORY;
    point.value.coordinate.x = 0;
    point.value.coordinate.y = @intCast(scrollback_rows - 1);
    if (gvt.hist_anchor == null) {
        if (c.ghostty_terminal_grid_ref_track(gvt.terminal, point, &gvt.hist_anchor) != c.GHOSTTY_SUCCESS)
            gvt.hist_anchor = null;
    } else if (c.ghostty_tracked_grid_ref_set(gvt.hist_anchor, gvt.terminal, point) != c.GHOSTTY_SUCCESS) {
        c.ghostty_tracked_grid_ref_free(gvt.hist_anchor);
        gvt.hist_anchor = null;
    }
}

// Ring-full shift count via the anchor: it sat at the newest history
// row after the last import, so its current position gives the number
// of rows pushed past it since. Returns scrollback_rows when the
// anchored row was pruned entirely (full rebuild) and null when the
// anchor is unavailable (fall back to content comparison).
fn histAnchorShift(gvt: *GhosttyVT, scrollback_rows: usize) ?usize {
    const anchor = gvt.hist_anchor orelse return null;
    if (!c.ghostty_tracked_grid_ref_has_value(anchor))
        return scrollback_rows;
    var pt = std.mem.zeroes(c.GhosttyPointCoordinate);
    if (c.ghostty_tracked_grid_ref_point(anchor, c.GHOSTTY_POINT_TAG_HISTORY, &pt) != c.GHOSTTY_SUCCESS)
        return null;
    const y: usize = pt.y;
    if (y >= scrollback_rows)
        return null;
    return scrollback_rows - 1 - y;
}

fn syncScrollback(gvt: *GhosttyVT, s: *c.screen) bool {
    var scrollback_rows: usize = 0;
    if (c.ghostty_terminal_get(gvt.terminal, c.GHOSTTY_TERMINAL_DATA_SCROLLBACK_ROWS, &scrollback_rows) != c.GHOSTTY_SUCCESS)
        return false;
    const grid = s.*.grid orelse return false;

    if (scrollback_rows == gvt.last_scrollback) {
        if (scrollback_rows == 0)
            return false;
        if (grid.*.hsize == 0) {
            importHistoryRows(gvt, s, grid, 0, scrollback_rows);
            return true;
        }

        // Once ghostty's scrollback ring is full the row count stays
        // constant while the content shifts, possibly by several rows
        // per write. The anchor gives the shift directly; without it,
        // scan backwards for the row matching our newest history row.
        // If the ring shifted further than it holds, rebuild.
        var k: usize = 0;
        if (histAnchorShift(gvt, scrollback_rows)) |shift| {
            k = shift;
        } else {
            while (k < scrollback_rows) : (k += 1) {
                if (!historyRowChanged(gvt, s, scrollback_rows - 1 - k, grid.*.hsize - 1))
                    break;
            }
        }
        if (k == 0)
            return false;
        if (k < scrollback_rows) {
            importHistoryRows(gvt, s, grid, scrollback_rows - k, scrollback_rows);
            return true;
        }
        c.grid_clear_history(grid);
        importHistoryRows(gvt, s, grid, 0, scrollback_rows);
        return true;
    }

    var changed = false;
    if (scrollback_rows < gvt.last_scrollback) {
        c.grid_clear_history(grid);
        gvt.last_scrollback = 0;
        changed = true;
    }
    if (scrollback_rows > gvt.last_scrollback) {
        importHistoryRows(gvt, s, grid, gvt.last_scrollback, scrollback_rows);
        changed = true;
    }
    gvt.last_scrollback = scrollback_rows;
    return changed;
}

fn syncActiveScreen(gvt: *GhosttyVT, s: *c.screen) bool {
    var active: c.GhosttyTerminalScreen = undefined;
    if (c.ghostty_terminal_get(gvt.terminal, c.GHOSTTY_TERMINAL_DATA_ACTIVE_SCREEN, &active) != c.GHOSTTY_SUCCESS)
        return false;

    var changed = false;
    var gc = c.grid_default_cell;
    if (active == c.GHOSTTY_TERMINAL_SCREEN_ALTERNATE) {
        if (s.*.saved_grid == null and c.screen_alternate_on(s, &gc, 1) != 0) {
            changed = true;
            c.window_pane_clear_resizes(gvt.wp, null);
            if (gvt.wp.*.window) |window| {
                c.layout_fix_panes(window, null);
                c.server_redraw_window_borders(window);
            }
        }
    } else if (s.*.saved_grid != null and c.screen_alternate_off(s, &gc, 1) != 0) {
        changed = true;
        if (gvt.wp.*.window) |window| {
            c.layout_fix_panes(window, null);
            c.server_redraw_window_borders(window);
        }
    }

    gvt.active_screen = active;
    return changed;
}

fn syncRowFlagsLine(grid: *c.grid, line_y: c_uint, row: c.GhosttyRow) void {
    var wrapped = false;
    if (c.ghostty_row_get(row, c.GHOSTTY_ROW_DATA_WRAP, &wrapped) != c.GHOSTTY_SUCCESS)
        return;

    const line = c.grid_get_line(grid, line_y);
    if (wrapped)
        line.*.flags |= c.GRID_LINE_WRAPPED
    else
        line.*.flags &= ~@as(c_int, c.GRID_LINE_WRAPPED);
}

fn resetRowFlags(grid: *c.grid, line_y: c_uint) void {
    c.grid_get_line(grid, line_y).*.flags = 0;
}

fn syncRowFlags(grid: *c.grid, py: c_uint, row: c.GhosttyRow) void {
    syncRowFlagsLine(grid, grid.*.hsize + py, row);
}

extern fn stbi_load_from_memory(
    buffer: [*c]const u8,
    len: c_int,
    x: [*c]c_int,
    y: [*c]c_int,
    channels_in_file: [*c]c_int,
    desired_channels: c_int,
) [*c]u8;

extern fn stbi_image_free(retval_from_stbi_load: ?*anyopaque) void;

var png_decoder_installed = false;

fn ghosttyLogCb(
    _: ?*anyopaque,
    _: c.GhosttySysLogLevel,
    scope: [*c]const u8,
    scope_len: usize,
    message: [*c]const u8,
    message_len: usize,
) callconv(.c) void {
    log_debug("ghostty[%.*s]: %.*s", @as(c_int, @intCast(scope_len)), scope, @as(c_int, @intCast(message_len)), message);
}

fn installPngDecoder() void {
    if (png_decoder_installed)
        return;
    _ = c.ghostty_sys_set(c.GHOSTTY_SYS_OPT_DECODE_PNG, @ptrCast(&decodePngCb));
    _ = c.ghostty_sys_set(c.GHOSTTY_SYS_OPT_LOG, @ptrCast(&ghosttyLogCb));
    png_decoder_installed = true;
}

fn decodePngCb(
    _: ?*anyopaque,
    ghostty_allocator: ?*const c.GhosttyAllocator,
    data: [*c]const u8,
    data_len: usize,
    out: ?*c.GhosttySysImage,
) callconv(.c) bool {
    var width: c_int = 0;
    var height: c_int = 0;
    var channels: c_int = 0;
    const pixels = stbi_load_from_memory(data, @intCast(data_len), &width, &height, &channels, 4);
    if (pixels == null)
        return false;
    defer stbi_image_free(pixels);

    const result = out orelse return false;
    const pixels_len = @as(usize, @intCast(width)) * @as(usize, @intCast(height)) * 4;

    // The library frees this buffer with the provided allocator, so the
    // pixels must be allocated through it rather than returned directly.
    const copy = c.ghostty_alloc(ghostty_allocator, pixels_len) orelse return false;
    @memcpy(copy[0..pixels_len], pixels[0..pixels_len]);

    result.*.width = @intCast(width);
    result.*.height = @intCast(height);
    result.*.data = copy;
    result.*.data_len = pixels_len;
    return true;
}

const KittyPlacementInfo = struct {
    image_id: u32,
    placement_id: u32,
    z: i32,
    render_info: c.GhosttyKittyGraphicsPlacementRenderInfo,
    img_handle: c.GhosttyKittyGraphicsImage,
};

fn kittyPlacementLess(_: void, a: KittyPlacementInfo, b: KittyPlacementInfo) bool {
    return a.z < b.z;
}

fn kittyPlacementNext(gvt: *GhosttyVT, graphics: c.GhosttyKittyGraphics) ?KittyPlacementInfo {
    while (c.ghostty_kitty_graphics_placement_next(gvt.kitty_iter)) {
        var info: KittyPlacementInfo = undefined;
        info.image_id = 0;
        info.placement_id = 0;
        info.z = 0;
        var is_virtual = false;
        _ = c.ghostty_kitty_graphics_placement_get(gvt.kitty_iter, c.GHOSTTY_KITTY_GRAPHICS_PLACEMENT_DATA_IMAGE_ID, &info.image_id);
        _ = c.ghostty_kitty_graphics_placement_get(gvt.kitty_iter, c.GHOSTTY_KITTY_GRAPHICS_PLACEMENT_DATA_PLACEMENT_ID, &info.placement_id);
        _ = c.ghostty_kitty_graphics_placement_get(gvt.kitty_iter, c.GHOSTTY_KITTY_GRAPHICS_PLACEMENT_DATA_Z, &info.z);
        _ = c.ghostty_kitty_graphics_placement_get(gvt.kitty_iter, c.GHOSTTY_KITTY_GRAPHICS_PLACEMENT_DATA_IS_VIRTUAL, &is_virtual);
        if (is_virtual)
            continue;

        info.render_info = std.mem.zeroes(c.GhosttyKittyGraphicsPlacementRenderInfo);
        info.render_info.size = @sizeOf(c.GhosttyKittyGraphicsPlacementRenderInfo);
        info.img_handle = c.ghostty_kitty_graphics_image(graphics, info.image_id);
        if (info.img_handle == null)
            continue;
        if (c.ghostty_kitty_graphics_placement_render_info(gvt.kitty_iter, info.img_handle, gvt.terminal, &info.render_info) != c.GHOSTTY_SUCCESS)
            continue;
        if (!info.render_info.viewport_visible)
            continue;
        return info;
    }
    return null;
}

fn kittyGraphics(gvt: *GhosttyVT) ?c.GhosttyKittyGraphics {
    var graphics: c.GhosttyKittyGraphics = null;
    if (c.ghostty_terminal_get(gvt.terminal, c.GHOSTTY_TERMINAL_DATA_KITTY_GRAPHICS, @ptrCast(&graphics)) != c.GHOSTTY_SUCCESS or graphics == null)
        return null;
    if (c.ghostty_kitty_graphics_get(graphics, c.GHOSTTY_KITTY_GRAPHICS_DATA_PLACEMENT_ITERATOR, @ptrCast(&gvt.kitty_iter)) != c.GHOSTTY_SUCCESS)
        return null;
    return graphics;
}

// Expand a grayscale image to RGBA in place of the copy; kitty has no
// grayscale wire format. Returns null on failure.
fn kittyExpandGray(data: []const u8, width: u32, height: u32, has_alpha: bool) ?[]u8 {
    const pixels = @as(usize, width) * @as(usize, height);
    const bpp: usize = if (has_alpha) 2 else 1;
    if (data.len < pixels * bpp)
        return null;
    const out = allocator.alloc(u8, pixels * 4) catch return null;
    for (0..pixels) |p| {
        const g = data[p * bpp];
        out[p * 4 + 0] = g;
        out[p * 4 + 1] = g;
        out[p * 4 + 2] = g;
        out[p * 4 + 3] = if (has_alpha) data[p * bpp + 1] else 0xff;
    }
    return out;
}

fn syncKittyImages(gvt: *GhosttyVT, s: *c.screen) bool {
    if (!gvt.kitty_enabled)
        return false;

    var changed = false;

    if (gvt.screen_cleared) {
        c.tty_clear_pane_kitty_images(gvt.wp);
        if (c.image_free_all(s) != 0)
            changed = true;
        gvt.last_kitty_sig = null;
        gvt.screen_cleared = false;
    }

    const graphics = kittyGraphics(gvt) orelse {
        if (gvt.last_kitty_sig != null) {
            c.tty_clear_pane_kitty_images(gvt.wp);
            if (c.image_free_all(s) != 0)
                changed = true;
            gvt.last_kitty_sig = null;
        }
        return changed;
    };

    // First pass: hash the visible placement set so an unchanged set of
    // placements is not freed and re-copied on every sync.
    var hasher = std.hash.Wyhash.init(0);
    var count: usize = 0;
    while (kittyPlacementNext(gvt, graphics)) |info| {
        hasher.update(std.mem.asBytes(&info.image_id));
        hasher.update(std.mem.asBytes(&info.placement_id));
        hasher.update(std.mem.asBytes(&info.render_info.viewport_col));
        hasher.update(std.mem.asBytes(&info.render_info.viewport_row));
        hasher.update(std.mem.asBytes(&info.render_info.grid_cols));
        hasher.update(std.mem.asBytes(&info.render_info.grid_rows));
        hasher.update(std.mem.asBytes(&info.render_info.source_x));
        hasher.update(std.mem.asBytes(&info.render_info.source_y));
        hasher.update(std.mem.asBytes(&info.render_info.source_width));
        hasher.update(std.mem.asBytes(&info.render_info.source_height));
        count += 1;
    }
    hasher.update(std.mem.asBytes(&count));
    const sig = hasher.final();
    if (gvt.last_kitty_sig != null and gvt.last_kitty_sig.? == sig)
        return changed;
    gvt.last_kitty_sig = sig;

    c.tty_clear_pane_kitty_images(gvt.wp);
    if (c.image_free_all(s) != 0)
        changed = true;
    if (count == 0)
        return changed;

    // Second pass: collect visible placements sorted by z-index so
    // lower layers are stored first and drawn underneath higher ones.
    const graphics2 = kittyGraphics(gvt) orelse return changed;
    var placements = allocator.alloc(KittyPlacementInfo, count) catch return changed;
    defer allocator.free(placements);
    var idx: usize = 0;
    while (kittyPlacementNext(gvt, graphics2)) |info| {
        if (idx < count) {
            placements[idx] = info;
            idx += 1;
        }
    }
    std.mem.sort(KittyPlacementInfo, placements[0..idx], {}, kittyPlacementLess);

    for (placements[0..idx]) |info| {
        var width: u32 = 0;
        var height: u32 = 0;
        var format: c.GhosttyKittyImageFormat = c.GHOSTTY_KITTY_IMAGE_FORMAT_RGBA;
        var compression: c.GhosttyKittyImageCompression = c.GHOSTTY_KITTY_IMAGE_COMPRESSION_NONE;
        var data_ptr: [*c]const u8 = null;
        var data_len: usize = 0;
        _ = c.ghostty_kitty_graphics_image_get(info.img_handle, c.GHOSTTY_KITTY_IMAGE_DATA_WIDTH, &width);
        _ = c.ghostty_kitty_graphics_image_get(info.img_handle, c.GHOSTTY_KITTY_IMAGE_DATA_HEIGHT, &height);
        _ = c.ghostty_kitty_graphics_image_get(info.img_handle, c.GHOSTTY_KITTY_IMAGE_DATA_FORMAT, &format);
        _ = c.ghostty_kitty_graphics_image_get(info.img_handle, c.GHOSTTY_KITTY_IMAGE_DATA_COMPRESSION, &compression);
        _ = c.ghostty_kitty_graphics_image_get(info.img_handle, c.GHOSTTY_KITTY_IMAGE_DATA_DATA_PTR, @ptrCast(&data_ptr));
        _ = c.ghostty_kitty_graphics_image_get(info.img_handle, c.GHOSTTY_KITTY_IMAGE_DATA_DATA_LEN, &data_len);
        if (data_ptr == null or data_len == 0)
            continue;

        const gray = format == c.GHOSTTY_KITTY_IMAGE_FORMAT_GRAY or
            format == c.GHOSTTY_KITTY_IMAGE_FORMAT_GRAY_ALPHA;
        if (gray and compression != c.GHOSTTY_KITTY_IMAGE_COMPRESSION_NONE) {
            // Cannot expand without inflating first; skip.
            log_debug("%s: skipping compressed grayscale image=%u", "syncKittyImages", info.image_id);
            continue;
        }

        var pixel_copy: []u8 = undefined;
        var out_format = format;
        if (gray) {
            const has_alpha = format == c.GHOSTTY_KITTY_IMAGE_FORMAT_GRAY_ALPHA;
            pixel_copy = kittyExpandGray(data_ptr[0..data_len], width, height, has_alpha) orelse continue;
            out_format = c.GHOSTTY_KITTY_IMAGE_FORMAT_RGBA;
        } else {
            const copy = allocator.alloc(u8, data_len) catch continue;
            @memcpy(copy, data_ptr[0..data_len]);
            pixel_copy = copy;
        }

        // Clip placements that start above or left of the viewport so
        // the stored position is inside the pane.
        var col = info.render_info.viewport_col;
        var row = info.render_info.viewport_row;
        var cols = info.render_info.grid_cols;
        var rows = info.render_info.grid_rows;
        var src_x = info.render_info.source_x;
        var src_y = info.render_info.source_y;
        var src_w = if (info.render_info.source_width != 0) info.render_info.source_width else width;
        var src_h = if (info.render_info.source_height != 0) info.render_info.source_height else height;
        if (col < 0 and cols != 0) {
            const shift: u32 = @intCast(-col);
            if (shift >= cols) {
                allocator.free(pixel_copy);
                continue;
            }
            src_x += shift * src_w / cols;
            src_w -= shift * src_w / cols;
            cols -= shift;
            col = 0;
        }
        if (row < 0 and rows != 0) {
            const shift: u32 = @intCast(-row);
            if (shift >= rows) {
                allocator.free(pixel_copy);
                continue;
            }
            src_y += shift * src_h / rows;
            src_h -= shift * src_h / rows;
            rows -= shift;
            row = 0;
        }
        if (cols == 0 or rows == 0) {
            allocator.free(pixel_copy);
            continue;
        }

        const ki = allocator.create(c.kitty_image) catch {
            allocator.free(pixel_copy);
            continue;
        };
        ki.* = .{
            .data = pixel_copy.ptr,
            .data_len = pixel_copy.len,
            .width = width,
            .height = height,
            .format = @intCast(out_format),
            .compression = @intCast(compression),
            .source_x = src_x,
            .source_y = src_y,
            .source_width = src_w,
            .source_height = src_h,
            .image_id = info.image_id,
            .placement_id = info.placement_id,
        };

        _ = c.image_store_kitty(s, ki, @intCast(col), @intCast(row), cols, rows);
        changed = true;
        log_debug("%s: stored kitty image=%u placement=%u at %d,%d grid=%ux%u", "syncKittyImages", info.image_id, info.placement_id, col, row, cols, rows);
    }

    return changed;
}

fn syncCursor(gvt: *GhosttyVT, s: *c.screen) void {
    var cursor_in_vp = false;
    _ = c.ghostty_render_state_get(gvt.render_state, c.GHOSTTY_RENDER_STATE_DATA_CURSOR_VIEWPORT_HAS_VALUE, &cursor_in_vp);
    if (cursor_in_vp) {
        var cx: u16 = 0;
        var cy: u16 = 0;
        _ = c.ghostty_render_state_get(gvt.render_state, c.GHOSTTY_RENDER_STATE_DATA_CURSOR_VIEWPORT_X, &cx);
        _ = c.ghostty_render_state_get(gvt.render_state, c.GHOSTTY_RENDER_STATE_DATA_CURSOR_VIEWPORT_Y, &cy);
        s.*.cx = cx;
        s.*.cy = cy;
    }

    var blinking = false;
    _ = c.ghostty_render_state_get(gvt.render_state, c.GHOSTTY_RENDER_STATE_DATA_CURSOR_BLINKING, &blinking);
    if (blinking)
        s.*.mode |= c.MODE_CURSOR_BLINKING
    else
        s.*.mode &= ~@as(c_int, c.MODE_CURSOR_BLINKING);

    var visual_style: c.GhosttyRenderStateCursorVisualStyle = undefined;
    if (c.ghostty_render_state_get(gvt.render_state, c.GHOSTTY_RENDER_STATE_DATA_CURSOR_VISUAL_STYLE, &visual_style) != c.GHOSTTY_SUCCESS)
        return;
    s.*.cstyle = switch (visual_style) {
        c.GHOSTTY_RENDER_STATE_CURSOR_VISUAL_STYLE_BAR => c.SCREEN_CURSOR_BAR,
        c.GHOSTTY_RENDER_STATE_CURSOR_VISUAL_STYLE_UNDERLINE => c.SCREEN_CURSOR_UNDERLINE,
        else => c.SCREEN_CURSOR_BLOCK,
    };
}

fn colorEq(a: c.GhosttyColorRgb, b: c.GhosttyColorRgb) bool {
    return a.r == b.r and a.g == b.g and a.b == b.b;
}

// Colors the application never overrode (via OSC 10/11/12) must stay
// unset on the tmux side, so the outer terminal's own defaults - and
// features like background transparency - show through. The getters
// return GHOSTTY_NO_VALUE when nothing was configured or overridden.
fn overriddenColor(gvt: *GhosttyVT, eff_kind: c_uint, def_kind: c_uint, unset: c_int) c_int {
    var eff = std.mem.zeroes(c.GhosttyColorRgb);
    var def = std.mem.zeroes(c.GhosttyColorRgb);
    if (c.ghostty_terminal_get(gvt.terminal, eff_kind, &eff) != c.GHOSTTY_SUCCESS)
        return unset;
    if (c.ghostty_terminal_get(gvt.terminal, def_kind, &def) == c.GHOSTTY_SUCCESS and
        colorEq(eff, def))
        return unset;
    return c.colour_join_rgb(eff.r, eff.g, eff.b);
}

fn syncColors(gvt: *GhosttyVT, s: *c.screen) bool {
    const wp = gvt.wp;
    var changed = false;

    const cursor = overriddenColor(gvt, c.GHOSTTY_TERMINAL_DATA_COLOR_CURSOR, c.GHOSTTY_TERMINAL_DATA_COLOR_CURSOR_DEFAULT, -1);
    if (s.*.default_ccolour != cursor) {
        s.*.default_ccolour = cursor;
        changed = true;
    }

    const fg = overriddenColor(gvt, c.GHOSTTY_TERMINAL_DATA_COLOR_FOREGROUND, c.GHOSTTY_TERMINAL_DATA_COLOR_FOREGROUND_DEFAULT, 8);
    if (wp.*.palette.fg != fg) {
        wp.*.palette.fg = fg;
        changed = true;
    }
    const bg = overriddenColor(gvt, c.GHOSTTY_TERMINAL_DATA_COLOR_BACKGROUND, c.GHOSTTY_TERMINAL_DATA_COLOR_BACKGROUND_DEFAULT, 8);
    if (wp.*.palette.bg != bg) {
        wp.*.palette.bg = bg;
        changed = true;
    }

    var pal: [256]c.GhosttyColorRgb = undefined;
    var pald: [256]c.GhosttyColorRgb = undefined;
    if (c.ghostty_terminal_get(gvt.terminal, c.GHOSTTY_TERMINAL_DATA_COLOR_PALETTE, @ptrCast(&pal)) == c.GHOSTTY_SUCCESS and
        c.ghostty_terminal_get(gvt.terminal, c.GHOSTTY_TERMINAL_DATA_COLOR_PALETTE_DEFAULT, @ptrCast(&pald)) == c.GHOSTTY_SUCCESS)
    {
        for (0..256) |i| {
            const desired: c_int = if (colorEq(pal[i], pald[i]))
                -1
            else
                c.colour_join_rgb(pal[i].r, pal[i].g, pal[i].b);
            // Compare against the override slot only: falling back to
            // colour_palette_get would fight tmux's own pane-colours
            // configuration in default_palette.
            const current: c_int = if (wp.*.palette.palette != null)
                wp.*.palette.palette[i]
            else
                -1;
            if (current != desired) {
                _ = c.colour_palette_set(&wp.*.palette, @intCast(i), desired);
                changed = true;
            }
        }
    }

    if (changed)
        wp.*.flags |= c.PANE_STYLECHANGED | c.PANE_THEMECHANGED;
    return changed;
}

fn sync(gvt: *GhosttyVT, s: *c.screen, force: bool) bool {
    const grid = s.*.grid orelse return false;
    bumpStyleCacheGen();
    const screen_changed = syncActiveScreen(gvt, s);
    var changed = screen_changed;
    if (gvt.active_screen == c.GHOSTTY_TERMINAL_SCREEN_PRIMARY) {
        if (syncScrollback(gvt, s))
            changed = true;
    }
    if (syncKittyImages(gvt, s))
        changed = true;

    if (c.ghostty_render_state_update(gvt.render_state, gvt.terminal) != c.GHOSTTY_SUCCESS)
        return changed;

    var dirty: c.GhosttyRenderStateDirty = undefined;
    if (c.ghostty_render_state_get(gvt.render_state, c.GHOSTTY_RENDER_STATE_DATA_DIRTY, &dirty) != c.GHOSTTY_SUCCESS)
        return changed;
    if (screen_changed or force)
        dirty = c.GHOSTTY_RENDER_STATE_DIRTY_FULL
    else if (dirty == c.GHOSTTY_RENDER_STATE_DIRTY_FALSE) {
        // Mode and colour changes (mouse-mode toggles, OSC 10/11 sets,
        // theme reports) arrive through escape sequences that ghostty
        // does not flag as a dirty cell change, so the row iteration
        // below never runs for them. Mirror modes/colours/cursor here
        // too instead of waiting for a later write that dirties a row.
        const old_mode = s.*.mode;
        const old_cx = s.*.cx;
        const old_cy = s.*.cy;
        if (gvt.saw_esc) {
            syncModes(gvt, s);
            if (syncColors(gvt, s))
                changed = true;
        }
        syncCursor(gvt, s);
        if (s.*.mode != old_mode or s.*.cx != old_cx or s.*.cy != old_cy)
            changed = true;
        return changed;
    }

    var cols: u16 = 0;
    var rows: u16 = 0;
    if (c.ghostty_render_state_get(gvt.render_state, c.GHOSTTY_RENDER_STATE_DATA_COLS, &cols) != c.GHOSTTY_SUCCESS)
        return changed;
    if (c.ghostty_render_state_get(gvt.render_state, c.GHOSTTY_RENDER_STATE_DATA_ROWS, &rows) != c.GHOSTTY_SUCCESS)
        return changed;
    if (cols != grid.*.sx or rows != grid.*.sy)
        return changed;

    if (c.ghostty_render_state_get(gvt.render_state, c.GHOSTTY_RENDER_STATE_DATA_ROW_ITERATOR, @ptrCast(&gvt.row_iter)) != c.GHOSTTY_SUCCESS)
        return changed;

    var py: c_uint = 0;
    while (py < rows) : (py += 1) {
        if (!c.ghostty_render_state_row_iterator_next(gvt.row_iter))
            break;

        if (dirty == c.GHOSTTY_RENDER_STATE_DIRTY_PARTIAL) {
            var row_dirty = false;
            if (c.ghostty_render_state_row_get(gvt.row_iter, c.GHOSTTY_RENDER_STATE_ROW_DATA_DIRTY, &row_dirty) == c.GHOSTTY_SUCCESS and !row_dirty)
                continue;
        }

        resetRowFlags(grid, grid.*.hsize + py);

        var raw_row: c.GhosttyRow = 0;
        if (c.ghostty_render_state_row_get(gvt.row_iter, c.GHOSTTY_RENDER_STATE_ROW_DATA_RAW, &raw_row) == c.GHOSTTY_SUCCESS)
            syncRowFlags(grid, py, raw_row);

        var ref: c.GhosttyGridRef = undefined;
        if (pointGridRef(gvt, c.GHOSTTY_POINT_TAG_VIEWPORT, 0, py, &ref))
            syncRowCellsFromRef(s, grid, &ref, grid.*.hsize + py);

        if (dirty == c.GHOSTTY_RENDER_STATE_DIRTY_PARTIAL) {
            var clean = false;
            _ = c.ghostty_render_state_row_set(gvt.row_iter, c.GHOSTTY_RENDER_STATE_ROW_OPTION_DIRTY, &clean);
        }
    }

    if (force or screen_changed or gvt.saw_esc) {
        syncModes(gvt, s);
        _ = syncColors(gvt, s);
    }
    syncCursor(gvt, s);

    if (dirty == c.GHOSTTY_RENDER_STATE_DIRTY_FULL) {
        var clean = c.GHOSTTY_RENDER_STATE_DIRTY_FALSE;
        _ = c.ghostty_render_state_set(gvt.render_state, c.GHOSTTY_RENDER_STATE_OPTION_DIRTY, &clean);
    }
    return true;
}

export fn tmux_ghostty_vt_new(wp: ?*c.window_pane) ?*GhosttyVT {
    const pane = wp orelse return null;
    if (!sizeValid(pane.*.sx, pane.*.sy)) {
        log_debug("%s: invalid size %ux%u", "tmux_ghostty_vt_new", pane.*.sx, pane.*.sy);
        return null;
    }

    const gvt = allocator.create(GhosttyVT) catch return null;
    gvt.* = .{
        .terminal = null,
        .render_state = null,
        .row_iter = null,
        .wp = pane,
        .sx = pane.*.sx,
        .sy = pane.*.sy,
        .last_scrollback = 0,
        .active_screen = c.GHOSTTY_TERMINAL_SCREEN_PRIMARY,
        .osc_buf = null,
        .osc_len = 0,
        .osc_cap = 0,
        .osc_active = false,
        .osc_esc = false,
        .osc_pending_esc = false,
        .alt_pending = std.mem.zeroes([alt_pending_max]u8),
        .alt_pending_len = 0,
        .kitty_iter = null,
        .kitty_enabled = false,
        .last_kitty_sig = null,
        .screen_cleared = false,
        .saw_esc = true,
        .hist_anchor = null,
        .clear_state = .ground,
        .clear_param = 0,
        .clear_param_active = false,
        .clear_param_done = false,
    };

    const options = c.GhosttyTerminalOptions{
        .cols = @intCast(pane.*.sx),
        .rows = @intCast(pane.*.sy),
        .max_scrollback = pane.*.base.grid.?.*.hlimit,
    };
    if (c.ghostty_terminal_new(null, &gvt.terminal, options) != c.GHOSTTY_SUCCESS) {
        log_debug("%s: ghostty_terminal_new failed", "tmux_ghostty_vt_new");
        allocator.destroy(gvt);
        return null;
    }
    const pixels = pixelSize(pane);
    _ = c.ghostty_terminal_resize(gvt.terminal, @intCast(pane.*.sx), @intCast(pane.*.sy), pixels.width, pixels.height);
    if (c.ghostty_render_state_new(null, &gvt.render_state) != c.GHOSTTY_SUCCESS) {
        c.ghostty_terminal_free(gvt.terminal);
        allocator.destroy(gvt);
        return null;
    }
    if (c.ghostty_render_state_row_iterator_new(null, &gvt.row_iter) != c.GHOSTTY_SUCCESS) {
        c.ghostty_render_state_free(gvt.render_state);
        c.ghostty_terminal_free(gvt.terminal);
        allocator.destroy(gvt);
        return null;
    }

    _ = c.ghostty_terminal_set(gvt.terminal, c.GHOSTTY_TERMINAL_OPT_USERDATA, pane);
    _ = c.ghostty_terminal_set(gvt.terminal, c.GHOSTTY_TERMINAL_OPT_WRITE_PTY, @ptrCast(&writePtyCb));
    _ = c.ghostty_terminal_set(gvt.terminal, c.GHOSTTY_TERMINAL_OPT_TITLE_CHANGED, @ptrCast(&titleChangedCb));
    _ = c.ghostty_terminal_set(gvt.terminal, c.GHOSTTY_TERMINAL_OPT_BELL, @ptrCast(&bellCb));
    _ = c.ghostty_terminal_set(gvt.terminal, c.GHOSTTY_TERMINAL_OPT_ENQUIRY, @ptrCast(&enquiryCb));
    _ = c.ghostty_terminal_set(gvt.terminal, c.GHOSTTY_TERMINAL_OPT_XTVERSION, @ptrCast(&xtversionCb));
    _ = c.ghostty_terminal_set(gvt.terminal, c.GHOSTTY_TERMINAL_OPT_SIZE, @ptrCast(&sizeCb));
    _ = c.ghostty_terminal_set(gvt.terminal, c.GHOSTTY_TERMINAL_OPT_COLOR_SCHEME, @ptrCast(&colorSchemeCb));
    _ = c.ghostty_terminal_set(gvt.terminal, c.GHOSTTY_TERMINAL_OPT_DEVICE_ATTRIBUTES, @ptrCast(&deviceAttributesCb));
    _ = c.ghostty_terminal_set(gvt.terminal, c.GHOSTTY_TERMINAL_OPT_PWD_CHANGED, @ptrCast(&pwdChangedCb));

    var kitty_built = false;
    _ = c.ghostty_build_info(c.GHOSTTY_BUILD_INFO_KITTY_GRAPHICS, &kitty_built);
    if (kitty_built) {
        installPngDecoder();
        var storage_limit = kitty_storage_limit;
        _ = c.ghostty_terminal_set(gvt.terminal, c.GHOSTTY_TERMINAL_OPT_KITTY_IMAGE_STORAGE_LIMIT, &storage_limit);
        if (c.ghostty_kitty_graphics_placement_iterator_new(null, &gvt.kitty_iter) == c.GHOSTTY_SUCCESS)
            gvt.kitty_enabled = true;
    }

    return gvt;
}

export fn tmux_ghostty_vt_free(gvt_: ?*GhosttyVT) void {
    const gvt = gvt_ orelse return;
    c.ghostty_render_state_row_iterator_free(gvt.row_iter);
    c.ghostty_render_state_free(gvt.render_state);
    c.ghostty_terminal_free(gvt.terminal);
    if (gvt.kitty_iter != null)
        c.ghostty_kitty_graphics_placement_iterator_free(gvt.kitty_iter);
    c.ghostty_tracked_grid_ref_free(gvt.hist_anchor);
    freeOsc(gvt);
    allocator.destroy(gvt);
}

export fn tmux_ghostty_vt_resize(gvt_: ?*GhosttyVT, sx: c_uint, sy: c_uint) void {
    const gvt = gvt_ orelse return;
    if (!sizeValid(sx, sy))
        return;
    const pixels = pixelSize(gvt.wp);
    if (c.ghostty_terminal_resize(gvt.terminal, @intCast(sx), @intCast(sy), pixels.width, pixels.height) != c.GHOSTTY_SUCCESS)
        return;

    // A pixel-only change (e.g. the client font size changed) does not
    // invalidate the grid contents or the history.
    if (sx == gvt.sx and sy == gvt.sy)
        return;
    gvt.sx = sx;
    gvt.sy = sy;

    // ghostty reflows its scrollback on resize, so the imported copy no
    // longer lines up; rebuild it from scratch. The caller has already
    // resized the tmux grid, so resync immediately rather than leaving
    // the history empty until the next write.
    if (gvt.wp.*.base.grid != null)
        c.grid_clear_history(gvt.wp.*.base.grid);
    gvt.last_scrollback = 0;
    gvt.last_kitty_sig = null;
    _ = c.image_free_all(&gvt.wp.*.base);
    if (sync(gvt, &gvt.wp.*.base, true))
        gvt.wp.*.flags |= c.PANE_CHANGED | c.PANE_REDRAW;
}

export fn tmux_ghostty_vt_write(gvt_: ?*GhosttyVT, data: [*c]const u8, len: usize) void {
    const gvt = gvt_ orelse return;
    if (len == 0)
        return;

    var buf = data[0..len];
    // Both scanners only ever act on ESC/BEL/CAN/SUB bytes when idle,
    // so a plain-text buffer can skip the byte-by-byte state machines.
    // The same test gates the mode/palette sync: modes and colors can
    // only change through escape sequences, and a sequence split across
    // writes leaves the scanner states non-idle in between.
    const scanners_idle = gvt.clear_state == .ground and
        !gvt.osc_active and !gvt.osc_pending_esc;
    gvt.saw_esc = !scanners_idle or
        std.mem.indexOfScalar(u8, buf, 0x1b) != null;
    if (gvt.saw_esc) {
        scanClearScreen(gvt, buf);
        scanOscSideEffects(gvt, buf);
    }

    var filtered: ?AltFiltered = null;
    if (gvt.wp.*.options != null and c.options_get_number(gvt.wp.*.options, "alternate-screen") == 0)
        filtered = filterAlternateScreen(gvt, buf);
    defer if (filtered) |f| allocator.free(f.data.ptr[0..f.cap]);
    if (filtered) |f|
        buf = f.data;

    if (buf.len != 0)
        c.ghostty_terminal_vt_write(gvt.terminal, buf.ptr, buf.len);

    gvt.wp.*.flags |= c.PANE_CHANGED;
    if (sync(gvt, &gvt.wp.*.base, false))
        gvt.wp.*.flags |= c.PANE_REDRAW;
}
