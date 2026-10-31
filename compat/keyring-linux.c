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
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "tmux.h"

/*
 * Create a new session keyring since the current one belongs to the login
 * session which we're meant to outlive (and pam_keyinit(8) is likely to revoke
 * the login session's keyring when it ends). Link the keys from the current
 * session keyring into it so they remain available after that.
 */
void
fork_session_keyring(void)
{
	int32_t	*keys = NULL;
	long	 size = 0, n, i;

	/* KEYCTL_READ returns the full size; retry if keys were added. */
	for (;;) {
		n = syscall(SYS_keyctl, KEYCTL_READ, KEY_SPEC_SESSION_KEYRING,
		    keys, size);
		if (n <= size)
			break;
		size = n;
		keys = xrealloc(keys, size);
	}
	n = n > 0 ? n / sizeof *keys : 0;

	/*
	 * Link the current session keyring into the thread keyring so we still
	 * possess its keys (and so are allowed to link them) after leaving it.
	 */
	if (syscall(SYS_keyctl, KEYCTL_LINK, KEY_SPEC_SESSION_KEYRING,
	    KEY_SPEC_THREAD_KEYRING) == -1)
		log_debug("%s: link failed: %s", __func__, strerror(errno));

	if (syscall(SYS_keyctl, KEYCTL_JOIN_SESSION_KEYRING, NULL) == -1) {
		log_debug("%s: join failed: %s", __func__, strerror(errno));
		goto out;
	}
	for (i = 0; i < n; i++) {
		if (syscall(SYS_keyctl, KEYCTL_LINK, keys[i],
		    KEY_SPEC_SESSION_KEYRING) == -1) {
			log_debug("%s: link %d failed: %s", __func__, keys[i],
			    strerror(errno));
		}
	}
	if (syscall(SYS_keyctl, KEYCTL_LINK, KEY_SPEC_USER_KEYRING,
	    KEY_SPEC_SESSION_KEYRING) == -1)
		log_debug("%s: link failed: %s", __func__, strerror(errno));

out:
	syscall(SYS_keyctl, KEYCTL_CLEAR, KEY_SPEC_THREAD_KEYRING);
	free(keys);
}
