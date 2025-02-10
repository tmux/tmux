#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "tmux.h"
#include "xmalloc.h"

static int in_cygwin_virtual_filesystem(void);

#define PROFILE_TMPDIR "/AppData/Local/Temp/"

static char	tmpdir[4096]           = {0};
static char	socket_dir[4096]       = {0};
static char	tmpfile_template[4096] = {0};
static int	in_cygwin_fs	       = -1;

static int
in_cygwin_virtual_filesystem(void)
{
	if (in_cygwin_fs != -1)
		return in_cygwin_fs;

	in_cygwin_fs = 0;

	if (
		(access("/bin/sh",     F_OK) == 0) &&
		(access("/usr",        F_OK) == 0) &&
		(access("/home",       F_OK) == 0) &&
		(access("/proc",       F_OK) == 0) &&
		(access("/var",        F_OK) == 0) &&
		(access("/tmp",        F_OK) == 0) &&
		(access("/dev/null",   F_OK) == 0) &&
		(access("/dev/random", F_OK) == 0) &&
		(access("/dev/stdout", F_OK) == 0) &&
		(access("/proc/stat",  F_OK) == 0)
	) {
		in_cygwin_fs = 1;
	}

	return in_cygwin_fs;
}

const char *
win32_get_tmpdir(void)
{
	char	*profile_dir = NULL;

	if (*tmpdir)
		return tmpdir;

	if (in_cygwin_virtual_filesystem()) {
		strncpy(tmpdir, "/tmp/", 4096);
		return tmpdir;
	}

	if ((profile_dir = getenv("USERPROFILE")))
		snprintf(tmpdir, 4096, "%s%s", profile_dir, PROFILE_TMPDIR);

	return tmpdir;
}

const char *
win32_get_socket_dir_search_path(void)
{
	if (*socket_dir)
		return socket_dir;

	snprintf(socket_dir, 4096, "$TMUX_TMPDIR:%s", win32_get_tmpdir());

	return socket_dir;
}

const char *
win32_get_conf_search_path(void)
{
	if (in_cygwin_virtual_filesystem())
		return TMUX_CONF;

	return TMUX_CONF_WIN32;
}

const char *
win32_get_tmpfile_template(void)
{
	if (*tmpfile_template)
		return tmpfile_template;

	snprintf(tmpfile_template, 4096, "%s/tmux.XXXXXXXX", win32_get_tmpdir());

	return tmpfile_template;
}

const char *
win32_get_shell_cmd_switch(const char *shell)
{
	if (strstr(shell, "cmd.exe") != NULL)
		return "/c";
	else
		return "-c";
}
