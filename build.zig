const std = @import("std");

const tmux_sources = [_][]const u8{
    "alerts.c",
    "arguments.c",
    "attributes.c",
    "cfg.c",
    "client.c",
    "cmd-attach-session.c",
    "cmd-bind-key.c",
    "cmd-break-pane.c",
    "cmd-capture-pane.c",
    "cmd-choose-tree.c",
    "cmd-command-prompt.c",
    "cmd-confirm-before.c",
    "cmd-copy-mode.c",
    "cmd-detach-client.c",
    "cmd-display-menu.c",
    "cmd-display-message.c",
    "cmd-find-window.c",
    "cmd-find.c",
    "cmd-if-shell.c",
    "cmd-join-pane.c",
    "cmd-kill-pane.c",
    "cmd-kill-server.c",
    "cmd-kill-session.c",
    "cmd-kill-window.c",
    "cmd-list-buffers.c",
    "cmd-list-clients.c",
    "cmd-list-commands.c",
    "cmd-list-keys.c",
    "cmd-list-panes.c",
    "cmd-list-sessions.c",
    "cmd-list-windows.c",
    "cmd-load-buffer.c",
    "cmd-lock-server.c",
    "cmd-move-window.c",
    "cmd-new-session.c",
    "cmd-new-window.c",
    "cmd-paste-buffer.c",
    "cmd-pipe-pane.c",
    "cmd-queue.c",
    "cmd-refresh-client.c",
    "cmd-rename-session.c",
    "cmd-rename-window.c",
    "cmd-resize-pane.c",
    "cmd-resize-window.c",
    "cmd-respawn-pane.c",
    "cmd-respawn-window.c",
    "cmd-rotate-window.c",
    "cmd-run-shell.c",
    "cmd-save-buffer.c",
    "cmd-select-layout.c",
    "cmd-select-pane.c",
    "cmd-select-window.c",
    "cmd-send-keys.c",
    "cmd-server-access.c",
    "cmd-set-buffer.c",
    "cmd-set-environment.c",
    "cmd-set-option.c",
    "cmd-show-environment.c",
    "cmd-show-messages.c",
    "cmd-show-options.c",
    "cmd-show-prompt-history.c",
    "cmd-source-file.c",
    "cmd-split-window.c",
    "cmd-swap-pane.c",
    "cmd-swap-window.c",
    "cmd-switch-client.c",
    "cmd-unbind-key.c",
    "cmd-wait-for.c",
    "cmd.c",
    "colour.c",
    "control-notify.c",
    "control.c",
    "environ.c",
    "events-payload.c",
    "events.c",
    "file.c",
    "format.c",
    "format-draw.c",
    "fuzzy.c",
    "grid-reader.c",
    "grid-view.c",
    "grid.c",
    "hooks.c",
    "hyperlinks.c",
    "image.c",
    "input-keys.c",
    "input.c",
    "job.c",
    "key-bindings.c",
    "key-string.c",
    "layout-custom.c",
    "layout-set.c",
    "layout.c",
    "log.c",
    "menu.c",
    "mode-tree.c",
    "monitor.c",
    "names.c",
    "options-table.c",
    "options.c",
    "paste.c",
    "popup.c",
    "proc.c",
    "prompt.c",
    "prompt-history.c",
    "regsub.c",
    "resize.c",
    "screen-redraw.c",
    "screen-write.c",
    "screen.c",
    "server-acl.c",
    "server-client.c",
    "server-fn.c",
    "server.c",
    "session.c",
    "sort.c",
    "spawn.c",
    "status.c",
    "style.c",
    "tmux.c",
    "tty-acs.c",
    "tty-draw.c",
    "tty-features.c",
    "tty-keys.c",
    "tty-term.c",
    "tty.c",
    "utf8-combined.c",
    "utf8.c",
    "window-border.c",
    "window-buffer.c",
    "window-client.c",
    "window-clock.c",
    "window-copy.c",
    "window-customize.c",
    "window-panes.c",
    "window-switch.c",
    "window-tree.c",
    "window-visible.c",
    "window.c",
    "xmalloc.c",
};

const compat_common_sources = [_][]const u8{
    "compat/base64.c",
    "compat/closefrom.c",
    "compat/daemon.c",
    "compat/explicit_bzero.c",
    "compat/fdforkpty.c",
    "compat/freezero.c",
    "compat/getdtablecount.c",
    "compat/getopt_long.c",
    "compat/htonll.c",
    "compat/imsg-buffer.c",
    "compat/imsg.c",
    "compat/ntohll.c",
    "compat/reallocarray.c",
    "compat/recallocarray.c",
    "compat/setproctitle.c",
    "compat/unvis.c",
    "compat/vis.c",
};

const compat_linux_sources = [_][]const u8{
    "compat/getpeereid.c",
    "compat/getprogname.c",
    "compat/strtonum.c",
};

const wuffs_defines = [_][]const u8{
    "WUFFS_IMPLEMENTATION",
    "WUFFS_CONFIG__MODULES",
    "WUFFS_CONFIG__MODULE__BASE",
    "WUFFS_CONFIG__MODULE__PNG",
    "WUFFS_CONFIG__MODULE__DEFLATE",
    "WUFFS_CONFIG__MODULE__ZLIB",
    "WUFFS_CONFIG__MODULE__CRC32",
    "WUFFS_CONFIG__MODULE__ADLER32",
    "WUFFS_CONFIG__ENABLE_DROP_IN_REPLACEMENT__STB",
    "STBI_NO_STDIO",
    "WUFFS_CONFIG__DST_PIXEL_FORMAT__ENABLE_ALLOWLIST",
    "WUFFS_CONFIG__DST_PIXEL_FORMAT__ALLOW_RGBA_NONPREMUL",
};

const cflags = [_][]const u8{
    "-std=gnu99",
    "-D_GNU_SOURCE",
    "-Wall",
    "-Wextra",
    "-Wno-unused-parameter",
    "-Wno-missing-field-initializers",
    "-Wno-deprecated-declarations",
    "-Wno-unknown-warning-option",
    "-Wno-pointer-sign",
};

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});
    const enable_ghostty_vt = b.option(
        bool,
        "ghostty-vt",
        "Link libghostty-vt and compile the Ghostty VT backend",
    ) orelse false;
    const enable_utf8proc = b.option(
        bool,
        "utf8proc",
        "Link utf8proc for Unicode width support",
    ) orelse false;
    const exe = b.addExecutable(.{
        .name = "tmux",
        .root_module = b.createModule(.{
            .target = target,
            .optimize = optimize,
            .link_libc = true,
        }),
    });
    const mod = exe.root_module;

    mod.addIncludePath(b.path("."));
    mod.addIncludePath(b.path("compat"));

    addCommonDefines(mod);
    switch (target.result.os.tag) {
        .macos => addDarwin(mod, b),
        .linux => addLinux(mod, b),
        else => addUnknownUnix(mod, b, target.result.os.tag),
    }

    mod.addCSourceFiles(.{
        .root = b.path("."),
        .files = &tmux_sources,
        .flags = &cflags,
    });
    mod.addCSourceFiles(.{
        .root = b.path("."),
        .files = &compat_common_sources,
        .flags = &cflags,
    });

    const yacc = b.addSystemCommand(&.{ "bison", "-o" });
    const cmd_parse_c = yacc.addOutputFileArg("cmd-parse.c");
    yacc.addFileArg(b.path("cmd-parse.y"));
    mod.addCSourceFile(.{ .file = cmd_parse_c, .flags = &cflags });

    mod.linkSystemLibrary("libevent_core", .{});
    mod.linkSystemLibrary("ncurses", .{});
    mod.linkSystemLibrary("m", .{});
    if (enable_utf8proc) {
        mod.addCMacro("HAVE_UTF8PROC", "1");
        mod.addCSourceFile(.{ .file = b.path("compat/utf8proc.c"), .flags = &cflags });
        mod.linkSystemLibrary("utf8proc", .{ .needed = true });
    }

    if (enable_ghostty_vt) {
        mod.addCMacro("HAVE_GHOSTTY_VT", "1");
        const ghostty_mod = b.createModule(.{
            .root_source_file = b.path("src/ghostty-vt.zig"),
            .target = target,
            .optimize = optimize,
            .link_libc = true,
        });
        ghostty_mod.addIncludePath(b.path("."));
        ghostty_mod.addIncludePath(b.path("compat"));
        addPkgConfigIncludes(b, ghostty_mod, &.{ "libevent_core", "ncurses", "libghostty-vt" });
        addCommonDefines(ghostty_mod);
        addTargetDefines(ghostty_mod, target.result.os.tag);
        if (target.result.os.tag == .macos)
            addDarwinSDKIncludes(ghostty_mod, b);
        if (enable_utf8proc) {
            ghostty_mod.addCMacro("HAVE_UTF8PROC", "1");
            addPkgConfigIncludes(b, ghostty_mod, &.{ "libutf8proc" });
        }
        ghostty_mod.addCMacro("HAVE_GHOSTTY_VT", "1");
        if (b.lazyDependency("wuffs", .{})) |wuffs_dep| {
            var wuffs_flags: std.ArrayList([]const u8) = .empty;
            inline for (wuffs_defines) |define| {
                wuffs_flags.append(b.allocator, "-D" ++ define) catch @panic("OOM");
            }
            ghostty_mod.addCSourceFile(.{
                .file = wuffs_dep.path("release/c/wuffs-v0.4.c"),
                .flags = wuffs_flags.items,
            });
        }
        const ghostty_obj = b.addObject(.{
            .name = "ghostty-vt",
            .root_module = ghostty_mod,
        });
        mod.addObject(ghostty_obj);
        mod.linkSystemLibrary("libghostty-vt", .{ .needed = true });
    }

    b.installArtifact(exe);

    const run_cmd = b.addRunArtifact(exe);
    if (b.args) |args| run_cmd.addArgs(args);
    const run_step = b.step("run", "Run tmux");
    run_step.dependOn(&run_cmd.step);

    // Run tmux under Valgrind's memcheck tool. Mirrors ghostty's wiring: a
    // dedicated step that wraps the built binary in `valgrind` with leak
    // checking plus a project suppressions file. tmux forks a server
    // process, so trace children to actually check the long-lived server.
    // Args after `--` are forwarded to tmux, e.g.
    //   zig build valgrind -- -f /dev/null new-session -d
    const valgrind_cmd = b.addSystemCommand(&.{
        "valgrind",
        "--leak-check=full",
        "--track-origins=yes",
        "--trace-children=yes",
        "--num-callers=50",
        b.fmt("--suppressions={s}", .{b.pathFromRoot("valgrind.supp")}),
        "--gen-suppressions=all",
    });
    valgrind_cmd.addArtifactArg(exe);
    if (b.args) |args| valgrind_cmd.addArgs(args);
    const valgrind_step = b.step("valgrind", "Run tmux under Valgrind memcheck");
    valgrind_step.dependOn(&valgrind_cmd.step);
}

fn addPkgConfigIncludes(b: *std.Build, mod: *std.Build.Module, packages: []const []const u8) void {
    const argv = b.allocator.alloc([]const u8, packages.len + 2) catch @panic("OOM");
    argv[0] = "pkg-config";
    argv[1] = "--cflags-only-I";
    @memcpy(argv[2..], packages);

    const output = b.run(argv);
    var it = std.mem.tokenizeAny(u8, output, " \t\r\n");
    while (it.next()) |flag| {
        if (std.mem.startsWith(u8, flag, "-I") and flag.len > 2)
            mod.addSystemIncludePath(.{ .cwd_relative = flag[2..] });
    }
}

fn addCommonDefines(mod: *std.Build.Module) void {
    mod.addCMacro("TMUX_VERSION", "\"next-3.7-zig\"");
    mod.addCMacro("TMUX_CONF", "\"/etc/tmux.conf:~/.tmux.conf:$XDG_CONFIG_HOME/tmux/tmux.conf:~/.config/tmux/tmux.conf\"");
    mod.addCMacro("TMUX_LOCK_CMD", "\"lock -np\"");
    mod.addCMacro("TMUX_TERM", "\"screen\"");

    mod.addCMacro("HAVE_EVENT2_EVENT_H", "1");
    mod.addCMacro("HAVE_NCURSES_H", "1");
    mod.addCMacro("HAVE_STDINT_H", "1");
    mod.addCMacro("HAVE_INTTYPES_H", "1");
    mod.addCMacro("HAVE_FCNTL_H", "1");
    mod.addCMacro("HAVE_DIRENT_H", "1");
    mod.addCMacro("HAVE_PATHS_H", "1");

    mod.addCMacro("HAVE_ASPRINTF", "1");
    mod.addCMacro("HAVE_CFMAKERAW", "1");
    mod.addCMacro("HAVE_CLOCK_GETTIME", "1");
    mod.addCMacro("HAVE_DIRFD", "1");
    mod.addCMacro("HAVE_ERR_H", "1");
    mod.addCMacro("HAVE_FLOCK", "1");
    mod.addCMacro("HAVE_GETDTABLESIZE", "1");
    mod.addCMacro("HAVE_GETLINE", "1");
    mod.addCMacro("HAVE_MEMMEM", "1");
    mod.addCMacro("HAVE_SETENV", "1");
    mod.addCMacro("HAVE_STRCASESTR", "1");
    mod.addCMacro("HAVE_STRLCAT", "1");
    mod.addCMacro("HAVE_STRLCPY", "1");
    mod.addCMacro("HAVE_STRNDUP", "1");
    mod.addCMacro("HAVE_STRNLEN", "1");
    mod.addCMacro("HAVE_STRSEP", "1");
    mod.addCMacro("HAVE_SYSCONF", "1");
    // tiparm_s, not tiparm: ncurses 6.4+ tiparm() rejects string parameters
    // (e.g. the Ms clipboard capability), silently breaking OSC 52.
    mod.addCMacro("HAVE_TIPARM_S", "1");
}

fn addTargetDefines(mod: *std.Build.Module, os_tag: std.Target.Os.Tag) void {
    switch (os_tag) {
        .macos => {
            mod.addCMacro("BROKEN___DEAD", "1");
            mod.addCMacro("BROKEN_CMSG_FIRSTHDR", "1");
            mod.addCMacro("HAVE_FGETLN", "1");
            mod.addCMacro("HAVE_FORKPTY", "1");
            mod.addCMacro("HAVE_GETPEEREID", "1");
            mod.addCMacro("HAVE_GETPROGNAME", "1");
            mod.addCMacro("HAVE_LIBPROC_H", "1");
            mod.addCMacro("HAVE_PROC_PIDINFO", "1");
            mod.addCMacro("HAVE_STRTONUM", "1");
            mod.addCMacro("HAVE_SYS_SIGNAME", "1");
            mod.addCMacro("HAVE_UTIL_H", "1");
        },
        .linux => {
            mod.addCMacro("HAVE_FORKPTY", "1");
            mod.addCMacro("HAVE_MALLOC_TRIM", "1");
            mod.addCMacro("HAVE_PRCTL", "1");
            mod.addCMacro("HAVE_PR_SET_NAME", "1");
            mod.addCMacro("HAVE_PROC_PID", "1");
            mod.addCMacro("HAVE_PTY_H", "1");
            mod.addCMacro("HAVE_SO_PEERCRED", "1");
            mod.addCMacro("HAVE___PROGNAME", "1");
        },
        else => {},
    }
}

fn addDarwin(mod: *std.Build.Module, b: *std.Build) void {
    addTargetDefines(mod, .macos);
    addDarwinSDKIncludes(mod, b);

    mod.addCSourceFile(.{ .file = b.path("osdep-darwin.c"), .flags = &cflags });
    mod.addCSourceFile(.{ .file = b.path("compat/daemon-darwin.c"), .flags = &cflags });
    mod.linkSystemLibrary("util", .{});
}

fn addDarwinSDKIncludes(mod: *std.Build.Module, b: *std.Build) void {
    const sdkroot = b.graph.environ_map.get("SDKROOT") orelse return;
    if (sdkroot.len == 0)
        return;
    mod.addSystemIncludePath(.{ .cwd_relative = b.pathJoin(&.{ sdkroot, "usr/include" }) });
}

fn addLinux(mod: *std.Build.Module, b: *std.Build) void {
    addTargetDefines(mod, .linux);

    mod.addCSourceFile(.{ .file = b.path("osdep-linux.c"), .flags = &cflags });
    mod.addCSourceFiles(.{
        .root = b.path("."),
        .files = &compat_linux_sources,
        .flags = &cflags,
    });
    mod.linkSystemLibrary("util", .{});
}

fn addUnknownUnix(mod: *std.Build.Module, b: *std.Build, os_tag: std.Target.Os.Tag) void {
    std.log.warn("using osdep-unknown.c for unsupported target OS {s}", .{@tagName(os_tag)});
    mod.addCSourceFile(.{ .file = b.path("osdep-unknown.c"), .flags = &cflags });
}
