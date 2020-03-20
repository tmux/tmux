
project('tmux',
    'c',

    version: '3.1',
    meson_version: '>=0.50.0',
    default_options: [
        'c_std=gnu99',
    ],
)

cc = meson.get_compiler('c')

config = configuration_data()
cflags = []

###############################################################################
# Project configuration

cflags += [ '-Wno-pointer-sign' ]

config.set_quoted('TMUX_VERSION', meson.project_version())

sysconfdir_abs = '/' / get_option('sysconfdir')
config.set('SYSCONFDIR', sysconfdir_abs)

config.set_quoted('TMUX_CONF', ':'.join(
    [ sysconfdir_abs/'tmux.conf', '~/.tmux.conf', '~/.config/tmux/tmux.conf' ]
))

###############################################################################
# System detection
platform = host_machine.system()
compiler = cc.get_id()

platform_aix        = platform == 'aix'
platform_hpux       = platform == 'hpux'
platform_darwin     = platform == 'darwin'
platform_dragonfly  = platform == 'dragonfly'
platform_linux      = platform == 'linux'
platform_freebsd    = platform == 'freebsd'
platform_netbsd     = platform == 'netbsd'
platform_openbsd    = platform == 'openbsd'
platform_sunos      = platform == 'sunos'
platform_windows    = platform == 'windows'

cc_gcc = compiler == 'gcc'

cc_suncc = cc.run('''
int main() {
    #ifdef __SUNPRO_C
    return 0;
    #endif
    return 1;
}
''').returncode() == 0

if platform_sunos
    if cc_gcc
        cflags += '-D_XPG6'
    else
        cflags += '-D_XPG4_2'
    endif
endif
if cc_suncc
    cflags += '-erroff=E_EMPTY_DECLARATION'
endif
if platform_aix
    cflags += '-D_LINUX_SOURCE_COMPAT=1'
endif
if platform_netbsd
    cflags += '-D_OPENBSD_SOURCE'
endif

if cc_gcc
    cflags += '-D_GNU_SOURCE'
endif

###############################################################################
# Yacc/Bison

flex_program = find_program('flex')
flex = generator(flex_program,
    output : '@PLAINNAME@.yy.c',
    arguments : ['-o', '@OUTPUT@', '@INPUT@'],
)

bison_program = find_program('bison')
bison = generator(bison_program,
    output : ['@BASENAME@.tab.c', '@BASENAME@.tab.h'],
    arguments : ['@INPUT@', '--defines=@OUTPUT1@', '--output=@OUTPUT0@']
)


###############################################################################
# Environment detection

check_headers = [
    'bitstring.h',
    'dirent.h',
    'err.h',
    'fcntl.h',
    'libutil.h',
    'ndir.h',
    'paths.h',
    'pty.h',
    'stdint.h',
    'sys/dir.h',
    'sys/ndir.h',
    'sys/tree.h',
    'util.h',
]

check_functions = [
    'dirfd',
    'flock',
    'prctl',
    'sysconf',
]

foreach header : check_headers
    macro = 'HAVE_@0@'.format(header.underscorify().to_upper())
    config.set(macro, cc.has_header(header))
endforeach

foreach function : check_functions
    macro = 'HAVE_@0@'.format(function.underscorify().to_upper())
    config.set(macro, cc.has_function(function))
endforeach


# For automatic detection
compat_functions = []
# Compat file names without extension, doesn't create HAVE_* defines
compat_apis = []

# Always use our getopt because 1) glibc's doesn't enforce argument order 2)
# musl does not set optarg to NULL for flags without arguments (although it is
# not required to, but it is helpful) 3) there are probably other weird
# implementations.
compat_apis += 'getopt'

if not cc.has_header('err.h')
    compat_apis += 'err'
endif


# Look for stravis, compat/{vis,unvis}.c used if missing.
strnvis_valid = cc.run('''
#include <vis.h>
// Should fail if incompatible definition
int strnvis(char* , const char* , size_t, int);
int main() { return 0; }
''').compiled()

vis_dq_valid = cc.has_header_symbol('vis.h', 'VIS_DQ')

if not (strnvis_valid and vis_dq_valid)
    compat_apis += [ 'vis', 'unvis' ]
endif



if platform_darwin
    config.set('BROKEN_CMSG_FIRSTHDR', true)
    compat_apis += [
        'daemon',
        'daemon-darwin'
    ]
endif

###############################################################################
# Dependencies

nodep = dependency('', required: false)

libevent = dependency('libevent')
libevent_header = cc.has_header('event.h', required: true)

libutil = cc.find_library('util')

nsl = cc.find_library('nsl')

ncurses = dependency('tinfo', required: false)
if not ncurses.found()
    ncurses = dependency('ncurses', required: false)
endif
if not ncurses.found()
    ncurses = dependency('ncursesw')
endif
config.set('HAVE_NCURSES_H', true)
config.set('HAVE_CURSES_H', false)


utempter = nodep
if get_option('utempter')
    cc.has_header('utempter.h', required: true)
    utempter = cc.find_library('utempter', required: true)
endif
config.set('HAVE_UTEMPTER', utempter.found())


utf8proc = nodep
if get_option('utf8proc')
    utf8proc = cc.find_library('utf8proc', required: true)
    cc.has_header('utf8proc.h', required: true)
    # Check functions of library
    if not cc.has_function('utf8proc_charwidth', dependencies: utf8proc)
        error('Found incomplete utf8proc, please disable.')
    endif
    compat_apis += 'utf8proc'
endif
config.set('HAVE_UTF8PROC', utf8proc.found())

# check for b64_ntop
have_b64_ntop = cc.run('''
    #include <sys/types.h>
    #include <netinet/in.h>
    #include <resolv.h>
    int main() {
        b64_ntop(NULL, 0, NULL, 0);
        return 0;
    }
''').compiled()

resolv = nodep
if not have_b64_ntop
    resolv = cc.find_library('resolv')
    have_b64_ntop = cc.run('''
        #include <sys/types.h>
        #include <netinet/in.h>
        #include <resolv.h>
        int main() {
            b64_ntop(NULL, 0, NULL, 0);
            return 0;
        }
    ''', dependencies: resolv).compiled()
    if not have_b64_ntop
        resolv = nodep
    endif
endif
if not have_b64_ntop
    compat_apis += 'base64'
endif
config.set('HAVE_B64_NTOP', have_b64_ntop)


found_cmsg_data = cc.run('''
    #include <sys/socket.h>
    int main() {
        #ifdef CMSG_DATA
        return 0;
        #endif
        return 1;
    }
''').returncode() == 0

if not found_cmsg_data
    found_cmsg_data = cc.run('''
        #define _XOPEN_SOURCE 1
        #define _XOPEN_SOURCE_EXTENDED 1
        #include <sys/socket.h>
        int main() {
            #ifdef CMSG_DATA
            return 0;
            #endif
            return 1;
        }
    ''').returncode() == 0

    if found_cmsg_data
        cflags += [
            '-D_XOPEN_SOURCE',
            '-D_XOPEN_SOURCE_EXTENDED',
        ]
    endif
endif

if not found_cmsg_data
    error('CMSG_DATA not found')
endif


# Look for imsg_init in libutil.
have_imsg = cc.has_function('imsg_init', dependencies: libutil)
config.set('HAVE_IMSG', have_imsg)
if not have_imsg
    compat_apis += [ 'imsg', 'imsg-buffer' ]
endif

# Look for fdforkpty in libutil.
have_fdforkpty = cc.has_function('fdforkpty', dependencies: libutil)
config.set('HAVE_FDFORKPTY', have_fdforkpty)
if not have_fdforkpty
    compat_apis += 'fdforkpty'
endif

have_forkpty = cc.has_function('forkpty', dependencies: libutil)
config.set('HAVE_FORKPTY', have_forkpty)
if not have_forkpty
    compat_apis += 'forkpty-@0@'.format(platform)
endif

have_queue_h = (
    cc.has_header_symbol('sys/queue.h', 'TAILQ_CONCAT') and
    cc.has_header_symbol('sys/queue.h', 'TAILQ_PREV')   and
    cc.has_header_symbol('sys/queue.h', 'TAILQ_REPLACE')
)
config.set('HAVE_QUEUE_H', have_queue_h)

# Checking for progname
have_program_invocation_short_name = cc.run('''
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
int main() {
    const char *cp = program_invocation_short_name;
    printf("%s\n", cp);
    return 0;
}
''')
config.set('HAVE_PROGRAM_INVOCATION_SHORT_NAME',
    have_program_invocation_short_name.compiled()
)

have___progname = cc.run('''
#include <stdio.h>
#include <stdlib.h>
extern char* __progname;
int main() {
    const char *cp = __progname;
    printf("%s\n", cp);
    return 0;
}
''')
config.set('HAVE___PROGNAME', have___progname.compiled())


# Look for prctl(PR_SET_NAME).
have_pr_set_name = cc.has_header_symbol('sys/prctl.h', 'PR_SET_NAME')
config.set('HAVE_PR_SET_NAME', have_pr_set_name)

# Look for fcntl(F_CLOSEM).
have_f_closem = cc.has_header_symbol('fcntl.h', 'F_CLOSEM')
config.set('HAVE_FCNTL_CLOSEM', have_f_closem)

# Look for /proc/$$.
have_proc_pid = run_command('test', '-d', '/proc/$$').returncode() == 0
config.set('HAVE_PROC_PID', have_proc_pid)


add_project_arguments(cflags, language: 'c')
add_project_arguments('-include', 'config.h', language: 'c')

###############################################################################
# Sources

subdir('compat')
libcompat = static_library('compat',
    compat_sources,
)

configure_file(
    output: 'config.h',
    configuration: config,
)

tmux_sources = files(
    'alerts.c',
    'arguments.c',
    'attributes.c',
    'cfg.c',
    'client.c',
    'cmd-attach-session.c',
    'cmd-bind-key.c',
    'cmd-break-pane.c',
    'cmd-capture-pane.c',
    'cmd-choose-tree.c',
    'cmd-command-prompt.c',
    'cmd-confirm-before.c',
    'cmd-copy-mode.c',
    'cmd-detach-client.c',
    'cmd-display-menu.c',
    'cmd-display-message.c',
    'cmd-display-panes.c',
    'cmd-find-window.c',
    'cmd-find.c',
    'cmd-if-shell.c',
    'cmd-join-pane.c',
    'cmd-kill-pane.c',
    'cmd-kill-server.c',
    'cmd-kill-session.c',
    'cmd-kill-window.c',
    'cmd-list-buffers.c',
    'cmd-list-clients.c',
    'cmd-list-keys.c',
    'cmd-list-panes.c',
    'cmd-list-sessions.c',
    'cmd-list-windows.c',
    'cmd-load-buffer.c',
    'cmd-lock-server.c',
    'cmd-move-window.c',
    'cmd-new-session.c',
    'cmd-new-window.c',
    # 'cmd-parse.y',
    'cmd-paste-buffer.c',
    'cmd-pipe-pane.c',
    'cmd-queue.c',
    'cmd-refresh-client.c',
    'cmd-rename-session.c',
    'cmd-rename-window.c',
    'cmd-resize-pane.c',
    'cmd-resize-window.c',
    'cmd-respawn-pane.c',
    'cmd-respawn-window.c',
    'cmd-rotate-window.c',
    'cmd-run-shell.c',
    'cmd-save-buffer.c',
    'cmd-select-layout.c',
    'cmd-select-pane.c',
    'cmd-select-window.c',
    'cmd-send-keys.c',
    'cmd-set-buffer.c',
    'cmd-set-environment.c',
    'cmd-set-option.c',
    'cmd-show-environment.c',
    'cmd-show-messages.c',
    'cmd-show-options.c',
    'cmd-source-file.c',
    'cmd-split-window.c',
    'cmd-swap-pane.c',
    'cmd-swap-window.c',
    'cmd-switch-client.c',
    'cmd-unbind-key.c',
    'cmd-wait-for.c',
    'cmd.c',
    'colour.c',
    'control-notify.c',
    'control.c',
    'environ.c',
    'file.c',
    'format-draw.c',
    'format.c',
    'grid-view.c',
    'grid.c',
    'input-keys.c',
    'input.c',
    'job.c',
    'key-bindings.c',
    'key-string.c',
    'layout-custom.c',
    'layout-set.c',
    'layout.c',
    'log.c',
    'menu.c',
    'mode-tree.c',
    'names.c',
    'notify.c',
    'options-table.c',
    'options.c',
    'paste.c',
    'proc.c',
    'regsub.c',
    'resize.c',
    'screen-redraw.c',
    'screen-write.c',
    'screen.c',
    'server-client.c',
    'server-fn.c',
    'server.c',
    'session.c',
    'spawn.c',
    'status.c',
    'style.c',
    'tmux.c',
    'tty-acs.c',
    'tty-keys.c',
    'tty-term.c',
    'tty.c',
    'utf8.c',
    'window-buffer.c',
    'window-client.c',
    'window-clock.c',
    'window-copy.c',
    'window-tree.c',
    'window.c',
    'xmalloc.c',
    'xterm-keys.c',
)

tmux_sources += 'osdep-@0@.c'.format(platform)

tmux_sources += bison.process('cmd-parse.y')

tmux = executable('tmux',
    tmux_sources,

    dependencies: [
        libevent,
        ncurses,
        utempter,
        utf8proc,
        libutil,
        resolv,
    ],
    link_with: [
        libcompat,
    ],
    install: true,
)

###############################################################################
# Data

man_format = 'mdoc'

if platform_sunos and (not run_command('nroff', '--version').contains('GNU'))
    man_format = 'man'
endif


man_input = configure_file(
    input : 'tmux.1',
    output: 'tmux.1.out',
    configuration: config,
)

if man_format == 'mdoc'
    man_output = 'tmux.1.mdoc'
    man_command = [ 'cat', '@INPUT@', ]
else
    man_output = 'tmux.1.man'
    man_command = [ 'awk', '-f', files('mdoc2man.awk'), '@INPUT@', ]
endif

custom_target('man',
    input : man_input,
    output: man_output,
    command: man_command,
    capture: true,

    install: true,
    install_dir: get_option('mandir'),
)
