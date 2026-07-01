const std = @import("std");

const c = @cImport({
    @cDefine("HAVE_GHOSTTY_VT", "1");
    @cInclude("tmux.h");
    @cInclude("ghostty/vt.h");
});

const allocator = std.heap.c_allocator;
const max_size = std.math.maxInt(u16);
const max_osc_len = 1024 * 1024;
const max_hyperlink_uri = 4096;
const version = "tmux next-3.7-zig";

const GhosttyVT = struct {
    terminal: c.GhosttyTerminal,
    render_state: c.GhosttyRenderState,
    row_iter: c.GhosttyRenderStateRowIterator,
    row_cells: c.GhosttyRenderStateRowCells,
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

fn alternateCsiEnd(buf: []const u8, off: usize) ?usize {
    if (off + 4 >= buf.len or buf[off] != 0x1b or buf[off + 1] != '[' or buf[off + 2] != '?')
        return null;

    var i = off + 3;
    while (true) {
        if (i >= buf.len or !std.ascii.isDigit(buf[i]))
            return null;

        var mode: c_uint = 0;
        while (i < buf.len and std.ascii.isDigit(buf[i])) : (i += 1)
            mode = mode * 10 + @as(c_uint, buf[i] - '0');
        if (!alternateMode(mode))
            return null;

        if (i >= buf.len)
            return null;
        switch (buf[i]) {
            ';' => i += 1,
            'h', 'l' => return i + 1,
            else => return null,
        }
    }
}

fn filterAlternateScreen(buf: []const u8) ?[]u8 {
    var first: ?usize = null;
    for (buf, 0..) |_, i| {
        if (alternateCsiEnd(buf, i) != null) {
            first = i;
            break;
        }
    }
    const start = first orelse return null;

    var out = allocator.alloc(u8, buf.len) catch return null;
    var in: usize = 0;
    var out_off: usize = 0;

    while (in < buf.len) {
        if (in >= start) {
            if (alternateCsiEnd(buf, in)) |end| {
                in = end;
                continue;
            }
        }
        out[out_off] = buf[in];
        out_off += 1;
        in += 1;
    }
    return out[0..out_off];
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
    }
    resetOsc(gvt);
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

fn buildCell(gc: *c.grid_cell, style: *const c.GhosttyStyle, utf8_buf: []const u8) void {
    gc.* = std.mem.zeroes(c.grid_cell);
    gc.*.fg = 8;
    gc.*.bg = 8;
    gc.*.us = 8;

    if (utf8_buf.len == 0) {
        c.utf8_set(&gc.*.data, ' ');
    } else if (utf8_buf.len <= c.UTF8_SIZE) {
        @memcpy(gc.*.data.data[0..utf8_buf.len], utf8_buf);
        gc.*.data.size = @intCast(utf8_buf.len);
        gc.*.data.have = @intCast(utf8_buf.len);
        if (utf8_buf.len == 1) {
            gc.*.data.width = 1;
        } else {
            var wc: c.wchar_t = 0;
            var ud: c.utf8_data = undefined;
            c.utf8_set(&ud, 0);
            @memcpy(ud.data[0..utf8_buf.len], utf8_buf);
            ud.size = @intCast(utf8_buf.len);
            ud.have = @intCast(utf8_buf.len);
            if (c.utf8_towc(&ud, &wc) == c.UTF8_DONE) {
                const width = c.wcwidth(wc);
                gc.*.data.width = if (width < 0) 1 else @intCast(width);
            } else {
                gc.*.data.width = 1;
            }
        }
    } else {
        c.utf8_set(&gc.*.data, ' ');
    }

    gc.*.attr = mapAttr(style);
    gc.*.fg = mapColor(&style.fg_color, 8);
    gc.*.bg = mapColor(&style.bg_color, 8);
    gc.*.us = mapColor(&style.underline_color, 8);
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

fn buildCellFromGridRef(s: *c.screen, gc: *c.grid_cell, ref: *const c.GhosttyGridRef) void {
    var style = std.mem.zeroes(c.GhosttyStyle);
    style.size = @sizeOf(c.GhosttyStyle);
    if (c.ghostty_grid_ref_style(ref, &style) != c.GHOSTTY_SUCCESS)
        style = std.mem.zeroes(c.GhosttyStyle);

    var utf8_buf: [c.UTF8_SIZE * 4]u8 = undefined;
    buildCell(gc, &style, utf8FromGridRef(ref, &utf8_buf));

    var raw_cell: c.GhosttyCell = 0;
    _ = c.ghostty_grid_ref_cell(ref, &raw_cell);
    var wide = c.GHOSTTY_CELL_WIDE_NARROW;
    _ = c.ghostty_cell_get(raw_cell, c.GHOSTTY_CELL_DATA_WIDE, &wide);
    var has_hyperlink = false;
    _ = c.ghostty_cell_get(raw_cell, c.GHOSTTY_CELL_DATA_HAS_HYPERLINK, &has_hyperlink);
    if (has_hyperlink)
        applyHyperlink(s, gc, ref);

    if (wide == c.GHOSTTY_CELL_WIDE_WIDE) {
        gc.*.data.width = 2;
    } else if (wide == c.GHOSTTY_CELL_WIDE_SPACER_TAIL) {
        gc.*.flags |= c.GRID_FLAG_PADDING;
        c.utf8_set(&gc.*.data, 0);
    }
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
    s.*.mode = 0;
    syncMode(gvt, s, ghosttyMode(25, false), c.MODE_CURSOR);
    syncMode(gvt, s, ghosttyMode(4, true), c.MODE_INSERT);
    syncMode(gvt, s, ghosttyMode(7, false), c.MODE_WRAP);
    syncMode(gvt, s, ghosttyMode(6, false), c.MODE_ORIGIN);
    syncMode(gvt, s, ghosttyMode(12, false), c.MODE_CURSOR_BLINKING);
    syncMode(gvt, s, ghosttyMode(2004, false), c.MODE_BRACKETPASTE);
    syncMode(gvt, s, ghosttyMode(1004, false), c.MODE_FOCUSON);
    syncMode(gvt, s, ghosttyMode(2026, false), c.MODE_SYNC);
    syncMode(gvt, s, ghosttyMode(20, true), c.MODE_CRLF);
    syncMode(gvt, s, ghosttyMode(66, false), c.MODE_KKEYPAD);
    syncMode(gvt, s, ghosttyMode(1, false), c.MODE_KCURSOR);
    syncMode(gvt, s, ghosttyMode(1000, false), c.MODE_MOUSE_STANDARD);
    syncMode(gvt, s, ghosttyMode(1002, false), c.MODE_MOUSE_BUTTON);
    syncMode(gvt, s, ghosttyMode(1003, false), c.MODE_MOUSE_ALL);
    syncMode(gvt, s, ghosttyMode(1005, false), c.MODE_MOUSE_UTF8);
    syncMode(gvt, s, ghosttyMode(1006, false), c.MODE_MOUSE_SGR);
}

fn syncHistoryRow(gvt: *GhosttyVT, s: *c.screen, history_y: usize, target_y: c_uint) void {
    const grid = s.*.grid orelse return;
    var ref: c.GhosttyGridRef = undefined;

    resetRowFlags(grid, target_y);
    if (pointGridRef(gvt, c.GHOSTTY_POINT_TAG_HISTORY, 0, history_y, &ref)) {
        var row: c.GhosttyRow = 0;
        if (c.ghostty_grid_ref_row(&ref, &row) == c.GHOSTTY_SUCCESS)
            syncRowFlagsLine(grid, target_y, row);
    }

    var px: c_uint = 0;
    while (px < grid.*.sx) : (px += 1) {
        if (!pointGridRef(gvt, c.GHOSTTY_POINT_TAG_HISTORY, px, history_y, &ref))
            continue;
        var gc: c.grid_cell = undefined;
        buildCellFromGridRef(s, &gc, &ref);
        c.grid_set_cell(grid, px, target_y, &gc);
    }
}

fn historyRowChanged(gvt: *GhosttyVT, s: *c.screen, history_y: usize, target_y: c_uint) bool {
    const grid = s.*.grid orelse return false;
    var ref: c.GhosttyGridRef = undefined;

    if (pointGridRef(gvt, c.GHOSTTY_POINT_TAG_HISTORY, 0, history_y, &ref)) {
        var row: c.GhosttyRow = 0;
        var wrapped = false;
        if (c.ghostty_grid_ref_row(&ref, &row) == c.GHOSTTY_SUCCESS and
            c.ghostty_row_get(row, c.GHOSTTY_ROW_DATA_WRAP, &wrapped) == c.GHOSTTY_SUCCESS)
        {
            const line = c.grid_get_line(grid, target_y);
            if (((line.*.flags & c.GRID_LINE_WRAPPED) != 0) != wrapped)
                return true;
        }
    }

    var px: c_uint = 0;
    while (px < grid.*.sx) : (px += 1) {
        if (!pointGridRef(gvt, c.GHOSTTY_POINT_TAG_HISTORY, px, history_y, &ref))
            continue;

        var old_gc: c.grid_cell = undefined;
        var new_gc: c.grid_cell = undefined;
        c.grid_get_cell(grid, px, target_y, &old_gc);
        buildCellFromGridRef(s, &new_gc, &ref);
        if (c.grid_cells_equal(&old_gc, &new_gc) == 0)
            return true;
    }
    return false;
}

fn gridHistoryFull(grid: *c.grid) bool {
    return grid.*.hlimit != 0 and grid.*.hsize >= grid.*.hlimit;
}

fn syncScrollback(gvt: *GhosttyVT, s: *c.screen) void {
    var scrollback_rows: usize = 0;
    if (c.ghostty_terminal_get(gvt.terminal, c.GHOSTTY_TERMINAL_DATA_SCROLLBACK_ROWS, &scrollback_rows) != c.GHOSTTY_SUCCESS)
        return;
    const grid = s.*.grid orelse return;

    if (scrollback_rows == gvt.last_scrollback) {
        if (scrollback_rows != 0 and grid.*.hsize != 0 and gridHistoryFull(grid)) {
            const history_y = scrollback_rows - 1;
            if (historyRowChanged(gvt, s, history_y, grid.*.hsize - 1)) {
                c.grid_collect_history(grid, 0);
                const next_y = grid.*.hsize;
                c.grid_scroll_history(grid, 8);
                syncHistoryRow(gvt, s, history_y, next_y);
            }
        }
        return;
    }

    if (scrollback_rows < gvt.last_scrollback) {
        c.grid_clear_history(grid);
        gvt.last_scrollback = 0;
    }
    if (scrollback_rows > gvt.last_scrollback) {
        var history_y = gvt.last_scrollback;
        while (history_y < scrollback_rows) : (history_y += 1) {
            if (grid.*.hsize >= grid.*.hlimit)
                c.grid_collect_history(grid, 0);
            const target_y = grid.*.hsize;
            c.grid_scroll_history(grid, 8);
            syncHistoryRow(gvt, s, history_y, target_y);
        }
    }
    gvt.last_scrollback = scrollback_rows;
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

fn syncColors(gvt: *GhosttyVT, s: *c.screen) void {
    var colors = std.mem.zeroes(c.GhosttyRenderStateColors);
    colors.size = @sizeOf(c.GhosttyRenderStateColors);
    if (c.ghostty_render_state_colors_get(gvt.render_state, &colors) != c.GHOSTTY_SUCCESS)
        return;

    var changed = false;
    const cursor = if (colors.cursor_has_value)
        c.colour_join_rgb(colors.cursor.r, colors.cursor.g, colors.cursor.b)
    else
        -1;
    if (s.*.default_ccolour != cursor) {
        s.*.default_ccolour = cursor;
        changed = true;
    }

    const wp = gvt.wp;
    var color = c.colour_join_rgb(colors.foreground.r, colors.foreground.g, colors.foreground.b);
    if (wp.*.palette.fg != color) {
        wp.*.palette.fg = color;
        changed = true;
    }
    color = c.colour_join_rgb(colors.background.r, colors.background.g, colors.background.b);
    if (wp.*.palette.bg != color) {
        wp.*.palette.bg = color;
        changed = true;
    }
    for (0..256) |i| {
        color = c.colour_join_rgb(colors.palette[i].r, colors.palette[i].g, colors.palette[i].b);
        if (c.colour_palette_get(&wp.*.palette, @as(c_int, @intCast(i)) | c.COLOUR_FLAG_256) != color) {
            _ = c.colour_palette_set(&wp.*.palette, @intCast(i), color);
            changed = true;
        }
    }

    if (changed)
        wp.*.flags |= c.PANE_STYLECHANGED | c.PANE_THEMECHANGED;
}

fn sync(gvt: *GhosttyVT, s: *c.screen) void {
    const grid = s.*.grid orelse return;
    const screen_changed = syncActiveScreen(gvt, s);
    if (gvt.active_screen == c.GHOSTTY_TERMINAL_SCREEN_PRIMARY)
        syncScrollback(gvt, s);

    if (c.ghostty_render_state_update(gvt.render_state, gvt.terminal) != c.GHOSTTY_SUCCESS)
        return;

    var dirty: c.GhosttyRenderStateDirty = undefined;
    if (c.ghostty_render_state_get(gvt.render_state, c.GHOSTTY_RENDER_STATE_DATA_DIRTY, &dirty) != c.GHOSTTY_SUCCESS)
        return;
    if (screen_changed)
        dirty = c.GHOSTTY_RENDER_STATE_DIRTY_FULL
    else if (dirty == c.GHOSTTY_RENDER_STATE_DIRTY_FALSE)
        return;

    var cols: u16 = 0;
    var rows: u16 = 0;
    if (c.ghostty_render_state_get(gvt.render_state, c.GHOSTTY_RENDER_STATE_DATA_COLS, &cols) != c.GHOSTTY_SUCCESS)
        return;
    if (c.ghostty_render_state_get(gvt.render_state, c.GHOSTTY_RENDER_STATE_DATA_ROWS, &rows) != c.GHOSTTY_SUCCESS)
        return;
    if (cols != grid.*.sx or rows != grid.*.sy)
        return;

    if (c.ghostty_render_state_get(gvt.render_state, c.GHOSTTY_RENDER_STATE_DATA_ROW_ITERATOR, @ptrCast(&gvt.row_iter)) != c.GHOSTTY_SUCCESS)
        return;

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

        if (c.ghostty_render_state_row_get(gvt.row_iter, c.GHOSTTY_RENDER_STATE_ROW_DATA_CELLS, @ptrCast(&gvt.row_cells)) != c.GHOSTTY_SUCCESS)
            continue;

        var raw_row: c.GhosttyRow = 0;
        if (c.ghostty_render_state_row_get(gvt.row_iter, c.GHOSTTY_RENDER_STATE_ROW_DATA_RAW, &raw_row) == c.GHOSTTY_SUCCESS)
            syncRowFlags(grid, py, raw_row);

        var px: c_uint = 0;
        while (px < cols) : (px += 1) {
            if (!c.ghostty_render_state_row_cells_next(gvt.row_cells))
                break;

            var style = std.mem.zeroes(c.GhosttyStyle);
            style.size = @sizeOf(c.GhosttyStyle);
            if (c.ghostty_render_state_row_cells_get(gvt.row_cells, c.GHOSTTY_RENDER_STATE_ROW_CELLS_DATA_STYLE, &style) != c.GHOSTTY_SUCCESS)
                style = std.mem.zeroes(c.GhosttyStyle);

            var utf8_buf: [c.UTF8_SIZE * 4]u8 = undefined;
            var gbuf = c.GhosttyBuffer{ .ptr = &utf8_buf, .cap = utf8_buf.len, .len = 0 };
            if (c.ghostty_render_state_row_cells_get(gvt.row_cells, c.GHOSTTY_RENDER_STATE_ROW_CELLS_DATA_GRAPHEMES_UTF8, &gbuf) != c.GHOSTTY_SUCCESS)
                gbuf.len = 0;

            var gc: c.grid_cell = undefined;
            buildCell(&gc, &style, utf8_buf[0..gbuf.len]);

            var raw_cell: c.GhosttyCell = 0;
            _ = c.ghostty_render_state_row_cells_get(gvt.row_cells, c.GHOSTTY_RENDER_STATE_ROW_CELLS_DATA_RAW, &raw_cell);
            var wide = c.GHOSTTY_CELL_WIDE_NARROW;
            _ = c.ghostty_cell_get(raw_cell, c.GHOSTTY_CELL_DATA_WIDE, &wide);
            var has_hyperlink = false;
            _ = c.ghostty_cell_get(raw_cell, c.GHOSTTY_CELL_DATA_HAS_HYPERLINK, &has_hyperlink);
            if (has_hyperlink) {
                var ref: c.GhosttyGridRef = undefined;
                if (pointGridRef(gvt, c.GHOSTTY_POINT_TAG_VIEWPORT, px, py, &ref))
                    applyHyperlink(s, &gc, &ref);
            }
            if (wide == c.GHOSTTY_CELL_WIDE_WIDE) {
                gc.data.width = 2;
            } else if (wide == c.GHOSTTY_CELL_WIDE_SPACER_TAIL) {
                gc.flags |= c.GRID_FLAG_PADDING;
                c.utf8_set(&gc.data, 0);
            }
            c.grid_view_set_cell(grid, px, py, &gc);
        }

        if (dirty == c.GHOSTTY_RENDER_STATE_DIRTY_PARTIAL) {
            var clean = false;
            _ = c.ghostty_render_state_row_set(gvt.row_iter, c.GHOSTTY_RENDER_STATE_ROW_OPTION_DIRTY, &clean);
        }
    }

    syncModes(gvt, s);
    syncCursor(gvt, s);
    syncColors(gvt, s);

    if (dirty == c.GHOSTTY_RENDER_STATE_DIRTY_FULL) {
        var clean = c.GHOSTTY_RENDER_STATE_DIRTY_FALSE;
        _ = c.ghostty_render_state_set(gvt.render_state, c.GHOSTTY_RENDER_STATE_OPTION_DIRTY, &clean);
    }
}

export fn tmux_ghostty_vt_new(wp: ?*c.window_pane) ?*GhosttyVT {
    const pane = wp orelse return null;
    if (!sizeValid(pane.*.sx, pane.*.sy))
        return null;

    const gvt = allocator.create(GhosttyVT) catch return null;
    gvt.* = .{
        .terminal = null,
        .render_state = null,
        .row_iter = null,
        .row_cells = null,
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
    };

    const options = c.GhosttyTerminalOptions{
        .cols = @intCast(pane.*.sx),
        .rows = @intCast(pane.*.sy),
        .max_scrollback = pane.*.base.grid.?.*.hlimit,
    };
    if (c.ghostty_terminal_new(null, &gvt.terminal, options) != c.GHOSTTY_SUCCESS) {
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
    if (c.ghostty_render_state_row_cells_new(null, &gvt.row_cells) != c.GHOSTTY_SUCCESS) {
        c.ghostty_render_state_row_iterator_free(gvt.row_iter);
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

    return gvt;
}

export fn tmux_ghostty_vt_free(gvt_: ?*GhosttyVT) void {
    const gvt = gvt_ orelse return;
    c.ghostty_render_state_row_cells_free(gvt.row_cells);
    c.ghostty_render_state_row_iterator_free(gvt.row_iter);
    c.ghostty_render_state_free(gvt.render_state);
    c.ghostty_terminal_free(gvt.terminal);
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
    gvt.sx = sx;
    gvt.sy = sy;
    if (gvt.wp.*.base.grid != null)
        c.grid_clear_history(gvt.wp.*.base.grid);
    gvt.last_scrollback = 0;
}

export fn tmux_ghostty_vt_write(gvt_: ?*GhosttyVT, data: [*c]const u8, len: usize) void {
    const gvt = gvt_ orelse return;
    if (len == 0)
        return;

    var buf = data[0..len];
    scanOscSideEffects(gvt, buf);

    var filtered: ?[]u8 = null;
    if (gvt.wp.*.options != null and c.options_get_number(gvt.wp.*.options, "alternate-screen") == 0) {
        filtered = filterAlternateScreen(buf);
        if (filtered) |f|
            buf = f;
    }
    defer if (filtered) |f| allocator.free(f.ptr[0..len]);

    if (buf.len != 0)
        c.ghostty_terminal_vt_write(gvt.terminal, buf.ptr, buf.len);

    sync(gvt, &gvt.wp.*.base);
    gvt.wp.*.flags |= c.PANE_CHANGED | c.PANE_REDRAW;
}
