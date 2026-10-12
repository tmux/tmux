/*
 * Copyright (c) 2026 Ilya Priven <ikon@google.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER
 * IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>
#include <sys/syscall.h>

#include <linux/keyctl.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#include "tmux.h"

/* Serial of the session keyring we joined, or 0 if none. */
static long	keyring_serial;

/*
 * Create a new session keyring since the current one belongs to the login
 * session which we're meant to outlive (and pam_keyinit(8) is likely to revoke
 * the login session's keyring when it ends).
 */
void
new_session_keyring(void)
{
	long	serial;

	serial = syscall(SYS_keyctl, KEYCTL_JOIN_SESSION_KEYRING, NULL);
	if (serial == -1) {
		log_debug("%s: join failed: %s", __func__, strerror(errno));
		return;
	}
	if (syscall(SYS_keyctl, KEYCTL_LINK, KEY_SPEC_USER_KEYRING,
	    KEY_SPEC_SESSION_KEYRING) == -1)
		log_debug("%s: link failed: %s", __func__, strerror(errno));
	keyring_serial = serial;
}

void
revoke_session_keyring(void)
{
	if (keyring_serial == 0)
		return;
	if (syscall(SYS_keyctl, KEYCTL_REVOKE, keyring_serial) == -1)
		log_debug("%s: revoke failed: %s", __func__, strerror(errno));
}
