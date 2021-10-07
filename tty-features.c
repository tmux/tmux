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
 * - default colours (under AX or op capabilities);
 * - AIX colours (under colors >= 16);
 * - alternate escape (if terminal is VT100-like).
 *
 * Also:
 * - DECFRA uses a flag instead of capabilities;
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
static const struct tty_feature tty_feature_title = {
	"title",
	tty_feature_title_capabilities,
	0
};

/* Terminal has mouse support. */
static const char *tty_feature_mouse_capabilities[] = {
	"kmous=\\E[M",
	NULL
};
static const struct tty_feature tty_feature_mouse = {
	"mouse",
	tty_feature_mouse_capabilities,
	0
};

/* Terminal can set the clipboard with OSC 52. */
static const char *tty_feature_clipboard_capabilities[] = {
	"Ms=\\E]52;%p1%s;%p2%s\\a",
	NULL
};
static const struct tty_feature tty_feature_clipboard = {
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
static const struct tty_feature tty_feature_rgb = {
	"RGB",
	tty_feature_rgb_capabilities,
	TERM_256COLOURS|TERM_RGBCOLOURS
};

/* Terminal supports 256 colours. */
static const char *tty_feature_256_capabilities[] = {
	"AX",
	"setab=\\E[%?%p1%{8}%<%t4%p1%d%e%p1%{16}%<%t10%p1%{8}%-%d%e48;5;%p1%d%;m",
	"setaf=\\E[%?%p1%{8}%<%t3%p1%d%e%p1%{16}%<%t9%p1%{8}%-%d%e38;5;%p1%d%;m",
	NULL
};
static const struct tty_feature tty_feature_256 = {
	"256",
	tty_feature_256_capabilities,
	TERM_256COLOURS
};

/* Terminal supports overline. */
static const char *tty_feature_overline_capabilities[] = {
	"Smol=\\E[53m",
	NULL
};
static const struct tty_feature tty_feature_overline = {
	"overline",
	tty_feature_overline_capabilities,
	0
};

/* Terminal supports underscore styles. */
static const char *tty_feature_usstyle_capabilities[] = {
	"Smulx=\\E[4::%p1%dm",
	"Setulc=\\E[58::2::%p1%{65536}%/%d::%p1%{256}%/%{255}%&%d::%p1%{255}%&%d%;m",
	"ol=\\E[59m",
	NULL
};
static const struct tty_feature tty_feature_usstyle = {
	"usstyle",
	tty_feature_usstyle_capabilities,
	0
};

/* Terminal supports bracketed paste. */
static const char *tty_feature_bpaste_capabilities[] = {
	"Enbp=\\E[?2004h",
	"Dsbp=\\E[?2004l",
	NULL
};
static const struct tty_feature tty_feature_bpaste = {
	"bpaste",
	tty_feature_bpaste_capabilities,
	0
};

/* Terminal supports focus reporting. */
static const char *tty_feature_focus_capabilities[] = {
	"Enfcs=\\E[?1004h",
	"Dsfcs=\\E[?1004l",
	NULL
};
static const struct tty_feature tty_feature_focus = {
	"focus",
	tty_feature_focus_capabilities,
	0
};

/* Terminal supports cursor styles. */
static const char *tty_feature_cstyle_capabilities[] = {
	"Ss=\\E[%p1%d q",
	"Se=\\E[2 q",
	NULL
};
static const struct tty_feature tty_feature_cstyle = {
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
static const struct tty_feature tty_feature_ccolour = {
	"ccolour",
	tty_feature_ccolour_capabilities,
	0
};

/* Terminal supports strikethrough. */
static const char *tty_feature_strikethrough_capabilities[] = {
	"smxx=\\E[9m",
	NULL
};
static const struct tty_feature tty_feature_strikethrough = {
	"strikethrough",
	tty_feature_strikethrough_capabilities,
	0
};

/* Terminal supports synchronized updates. */
static const char *tty_feature_sync_capabilities[] = {
	"Sync=\\EP=%p1%ds\\E\\\\",
	NULL
};
static const struct tty_feature tty_feature_sync = {
	"sync",
	tty_feature_sync_capabilities,
	0
};

/* Terminal supports extended keys. */
static const char *tty_feature_extkeys_capabilities[] = {
	"Eneks=\\E[>4;1m",
	"Dseks=\\E[>4m",
	NULL
};
static const struct tty_feature tty_feature_extkeys = {
	"extkeys",
	tty_feature_extkeys_capabilities,
	0
};

/* Terminal supports DECSLRM margins. */
static const char *tty_feature_margins_capabilities[] = {
	"Enmg=\\E[?69h",
	"Dsmg=\\E[?69l",
	"Clmg=\\E[s",
	"Cmg=\\E[%i%p1%d;%p2%ds",
	NULL
};
static const struct tty_feature tty_feature_margins = {
	"margins",
	tty_feature_margins_capabilities,
	TERM_DECSLRM
};

/* Terminal supports DECFRA rectangle fill. */
static const char *tty_feature_rectfill_capabilities[] = {
	"Rect",
	NULL
};
static const struct tty_feature tty_feature_rectfill = {
	"rectfill",
	tty_feature_rectfill_capabilities,
	TERM_DECFRA
};

/* Available terminal features. */
static const struct tty_feature *tty_features[] = {
	&tty_feature_256,
	&tty_feature_bpaste,
	&tty_feature_ccolour,
	&tty_feature_clipboard,
	&tty_feature_cstyle,
	&tty_feature_extkeys,
	&tty_feature_focus,
	&tty_feature_margins,
	&tty_feature_mouse,
	&tty_feature_overline,
	&tty_feature_rectfill,
	&tty_feature_rgb,
	&tty_feature_strikethrough,
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

	log_debug("adding terminal features %s", s);

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

void
tty_default_features(int *feat, const char *name, u_int version)
{
	static struct {
		const char	*name;
		u_int		 version;
		const char	*features;
	} table[] = {
#define TTY_FEATURES_BASE_MODERN_XTERM \
	"256,RGB,bpaste,clipboard,mouse,strikethrough,title"
		{ .name = "mintty",
		  .features = TTY_FEATURES_BASE_MODERN_XTERM
			      ",ccolour,cstyle,extkeys,margins,overline,usstyle"
		},
		{ .name = "tmux",
		  .features = TTY_FEATURES_BASE_MODERN_XTERM
			      ",ccolour,cstyle,focus,overline,usstyle"
		},
		{ .name = "rxvt-unicode",
		  .features = "256,bpaste,ccolour,cstyle,mouse,title"
		},
		{ .name = "iTerm2",
		  .features = TTY_FEATURES_BASE_MODERN_XTERM
			      ",cstyle,extkeys,margins,sync"
		},
		{ .name = "XTerm",
		  /*
		   * xterm also supports DECSLRM and DECFRA, but they can be
		   * disabled so not set it here - they will be added if
		   * secondary DA shows VT420.
		   */
		  .features = TTY_FEATURES_BASE_MODERN_XTERM
			      ",ccolour,cstyle,extkeys,focus"
		}
	};
	u_int	i;

	for (i = 0; i < nitems(table); i++) {
		if (strcmp(table[i].name, name) != 0)
			continue;
		if (version != 0 && version < table[i].version)
			continue;
		tty_add_features(feat, table[i].features, ",");
	}
}
