/* $OpenBSD$ */

/*
 * Copyright (c) 2020 Nicholas Marriott <nicholas.marriott@gmail.com>
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

#include <stdlib.h>
#include <string.h>

#include "tmux.h"

/*
 * Still hardcoded:
 * - bracket paste (sent if application asks for it);
 * - mouse (under kmous capability);
 * - focus events (under focus-events option);
 * - default colours (under AX or op capabilities);
 * - AIX colours (under colors >= 16);
 * - alternate escape (under XT).
 *
 * Also:
 * - XT is used to decide whether to send DA and DSR;
 * - DECSLRM and DECFRA use a flag instead of capabilities;
 * - UTF-8 is a separate flag on the client; needed for unattached clients.
 */

/* A named terminal feature. */
struct tty_feature {
	const char	 *name;
	const char	**capabilities;
	int		  flags;
};

/* Terminal has xterm(1) title setting. */
static const char *tty_feature_title_capabilities[] = {
	"tsl=\\E]0;", /* should be using TS really */
	"fsl=\\a",
	NULL
};
static struct tty_feature tty_feature_title = {
	"title",
	tty_feature_title_capabilities,
	0
};

/* Terminal can set the clipboard with OSC 52. */
static const char *tty_feature_clipboard_capabilities[] = {
	"Ms=\\E]52;%p1%s;%p2%s\\a",
	NULL
};
static struct tty_feature tty_feature_clipboard = {
	"clipboard",
	tty_feature_clipboard_capabilities,
	0
};

/*
 * Terminal supports RGB colour. This replaces setab and setaf also since
 * terminals with RGB have versions that do not allow setting colours from the
 * 256 palette.
 */
static const char *tty_feature_rgb_capabilities[] = {
	"AX",
	"setrgbf=\\E[38;2;%p1%d;%p2%d;%p3%dm",
	"setrgbb=\\E[48;2;%p1%d;%p2%d;%p3%dm",
	"setab=\\E[%?%p1%{8}%<%t4%p1%d%e%p1%{16}%<%t10%p1%{8}%-%d%e48;5;%p1%d%;m",
	"setaf=\\E[%?%p1%{8}%<%t3%p1%d%e%p1%{16}%<%t9%p1%{8}%-%d%e38;5;%p1%d%;m",
	NULL
};
static struct tty_feature tty_feature_rgb = {
	"RGB",
	tty_feature_rgb_capabilities,
	(TERM_256COLOURS|TERM_RGBCOLOURS)
};

/* Terminal supports 256 colours. */
static const char *tty_feature_256_capabilities[] = {
	"AX",
	"setab=\\E[%?%p1%{8}%<%t4%p1%d%e%p1%{16}%<%t10%p1%{8}%-%d%e48;5;%p1%d%;m",
	"setaf=\\E[%?%p1%{8}%<%t3%p1%d%e%p1%{16}%<%t9%p1%{8}%-%d%e38;5;%p1%d%;m",
	NULL
};
static struct tty_feature tty_feature_256 = {
	"256",
	tty_feature_256_capabilities,
	TERM_256COLOURS
};

/* Terminal supports overline. */
static const char *tty_feature_overline_capabilities[] = {
	"Smol=\\E[53m",
	NULL
};
static struct tty_feature tty_feature_overline = {
	"overline",
	tty_feature_overline_capabilities,
	0
};

/* Terminal supports underscore styles. */
static const char *tty_feature_usstyle_capabilities[] = {
	"Smulx=\E[4::%p1%dm",
	"Setulc=\E[58::2::%p1%{65536}%/%d::%p1%{256}%/%{255}%&%d::%p1%{255}%&%d%;m",
	NULL
};
static struct tty_feature tty_feature_usstyle = {
	"usstyle",
	tty_feature_usstyle_capabilities,
	0
};

/* Terminal supports cursor styles. */
static const char *tty_feature_cstyle_capabilities[] = {
	"Ss=\\E[%p1%d q",
	"Se=\\E[2 q",
	NULL
};
static struct tty_feature tty_feature_cstyle = {
	"cstyle",
	tty_feature_cstyle_capabilities,
	0
};

/* Terminal supports cursor colours. */
static const char *tty_feature_ccolour_capabilities[] = {
	"Cs=\\E]12;%p1%s\\a",
	"Cr=\\E]112\\a",
	NULL
};
static struct tty_feature tty_feature_ccolour = {
	"ccolour",
	tty_feature_ccolour_capabilities,
	0
};

/* Terminal supports synchronized updates. */
static const char *tty_feature_sync_capabilities[] = {
	"Sync=\\EP=%p1%ds\\E\\\\",
	NULL
};
static struct tty_feature tty_feature_sync = {
	"sync",
	tty_feature_sync_capabilities,
	0
};

/* Terminal supports DECSLRM margins. */
static struct tty_feature tty_feature_margins = {
	"margins",
	NULL,
	TERM_DECSLRM
};

/* Terminal supports DECFRA rectangle fill. */
static struct tty_feature tty_feature_rectfill = {
	"rectfill",
	NULL,
	TERM_DECFRA
};

/* Available terminal features. */
static const struct tty_feature *tty_features[] = {
	&tty_feature_256,
	&tty_feature_clipboard,
	&tty_feature_ccolour,
	&tty_feature_cstyle,
	&tty_feature_margins,
	&tty_feature_overline,
	&tty_feature_rectfill,
	&tty_feature_rgb,
	&tty_feature_sync,
	&tty_feature_title,
	&tty_feature_usstyle
};

void
tty_add_features(int *feat, const char *s, const char *separators)
{
	const struct tty_feature	 *tf;
	char				 *next, *loop, *copy;
	u_int				  i;

	loop = copy = xstrdup(s);
	while ((next = strsep(&loop, separators)) != NULL) {
		for (i = 0; i < nitems(tty_features); i++) {
			tf = tty_features[i];
			if (strcasecmp(tf->name, next) == 0)
				break;
		}
		if (i == nitems(tty_features)) {
			log_debug("unknown terminal feature: %s", next);
			break;
		}
		if (~(*feat) & (1 << i)) {
			log_debug("adding terminal feature: %s", tf->name);
			(*feat) |= (1 << i);
		}
	}
	free(copy);
}

const char *
tty_get_features(int feat)
{
	const struct tty_feature	*tf;
	static char			 s[512];
	u_int				 i;

	*s = '\0';
	for (i = 0; i < nitems(tty_features); i++) {
		if (~feat & (1 << i))
			continue;
		tf = tty_features[i];

		strlcat(s, tf->name, sizeof s);
		strlcat(s, ",", sizeof s);
	}
	if (*s != '\0')
		s[strlen(s) - 1] = '\0';
	return (s);
}

int
tty_apply_features(struct tty_term *term, int feat)
{
	const struct tty_feature	 *tf;
	const char			**capability;
	u_int				  i;

	if (feat == 0)
		return (0);
	log_debug("applying terminal features: %s", tty_get_features(feat));

	for (i = 0; i < nitems(tty_features); i++) {
		if ((term->features & (1 << i)) || (~feat & (1 << i)))
			continue;
		tf = tty_features[i];

		log_debug("applying terminal feature: %s", tf->name);
		if (tf->capabilities != NULL) {
			capability = tf->capabilities;
			while (*capability != NULL) {
				log_debug("adding capability: %s", *capability);
				tty_term_apply(term, *capability, 1);
				capability++;
			}
		}
		term->flags |= tf->flags;
	}
	if ((term->features | feat) == term->features)
		return (0);
	term->features |= feat;
	return (1);
}
