/* $OpenBSD$ */

/*
 * Copyright (c) 2008 Nicholas Marriott <nicm@users.sourceforge.net>
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

const char *
set_option_print(const struct set_option_entry *entry, struct options_entry *o)
{
	static char	out[BUFSIZ];
	const char     *s;

	*out = '\0';
	switch (entry->type) {
		case SET_OPTION_STRING:
			xsnprintf(out, sizeof out, "\"%s\"", o->str);
			break;
		case SET_OPTION_NUMBER:
			xsnprintf(out, sizeof out, "%lld", o->num);
			break;
		case SET_OPTION_KEY:
			s = key_string_lookup_key(o->num);
			xsnprintf(out, sizeof out, "%s", s);
			break;
		case SET_OPTION_COLOUR:
			s = colour_tostring(o->num);
			xsnprintf(out, sizeof out, "%s", s);
			break;
		case SET_OPTION_ATTRIBUTES:
			s = attributes_tostring(o->num);
			xsnprintf(out, sizeof out, "%s", s);
			break;
		case SET_OPTION_FLAG:
			if (o->num)
				strlcpy(out, "on", sizeof out);
			else
				strlcpy(out, "off", sizeof out);
			break;
		case SET_OPTION_CHOICE:
			s = entry->choices[o->num];
			xsnprintf(out, sizeof out, "%s", s);
			break;
	}
	return (out);
}

void
set_option_string(struct cmd_ctx *ctx, struct options *oo,
    const struct set_option_entry *entry, char *value, int append)
{
	struct options_entry	*o;
	char			*oldvalue, *newvalue;

	if (value == NULL) {
		ctx->error(ctx, "empty value");
		return;
	}

	if (append) {
		oldvalue = options_get_string(oo, entry->name);
		xasprintf(&newvalue, "%s%s", oldvalue, value);
	} else
		newvalue = value;
		
	o = options_set_string(oo, entry->name, "%s", newvalue);
	ctx->info(
	    ctx, "set option: %s -> %s", o->name, set_option_print(entry, o));

	if (newvalue != value)
		xfree(newvalue);
}

void
set_option_number(struct cmd_ctx *ctx, struct options *oo,
    const struct set_option_entry *entry, char *value)
{
	struct options_entry	*o;
	long long		 number;
	const char     		*errstr;

	if (value == NULL) {
		ctx->error(ctx, "empty value");
		return;
	}

	number = strtonum(value, entry->minimum, entry->maximum, &errstr);
	if (errstr != NULL) {
		ctx->error(ctx, "value is %s: %s", errstr, value);
		return;
	}

	o = options_set_number(oo, entry->name, number);
	ctx->info(
	    ctx, "set option: %s -> %s", o->name, set_option_print(entry, o));
}

void
set_option_key(struct cmd_ctx *ctx, struct options *oo,
    const struct set_option_entry *entry, char *value)
{
	struct options_entry	*o;
	int			 key;

	if (value == NULL) {
		ctx->error(ctx, "empty value");
		return;
	}

	if ((key = key_string_lookup_string(value)) == KEYC_NONE) {
		ctx->error(ctx, "unknown key: %s", value);
		return;
	}

	o = options_set_number(oo, entry->name, key);
	ctx->info(
	    ctx, "set option: %s -> %s", o->name, set_option_print(entry, o));
}

void
set_option_colour(struct cmd_ctx *ctx, struct options *oo,
    const struct set_option_entry *entry, char *value)
{
	struct options_entry	*o;
	int			 colour;

	if (value == NULL) {
		ctx->error(ctx, "empty value");
		return;
	}

	if ((colour = colour_fromstring(value)) == -1) {
		ctx->error(ctx, "bad colour: %s", value);
		return;
	}

	o = options_set_number(oo, entry->name, colour);
	ctx->info(
	    ctx, "set option: %s -> %s", o->name, set_option_print(entry, o));
}

void
set_option_attributes(struct cmd_ctx *ctx, struct options *oo,
    const struct set_option_entry *entry, char *value)
{
	struct options_entry	*o;
	int			 attr;

	if (value == NULL) {
		ctx->error(ctx, "empty value");
		return;
	}

	if ((attr = attributes_fromstring(value)) == -1) {
		ctx->error(ctx, "bad attributes: %s", value);
		return;
	}

	o = options_set_number(oo, entry->name, attr);
	ctx->info(
	    ctx, "set option: %s -> %s", o->name, set_option_print(entry, o));
}

void
set_option_flag(struct cmd_ctx *ctx, struct options *oo,
    const struct set_option_entry *entry, char *value)
{
	struct options_entry	*o;
	int			 flag;

	if (value == NULL || *value == '\0')
		flag = !options_get_number(oo, entry->name);
	else {
		if ((value[0] == '1' && value[1] == '\0') ||
		    strcasecmp(value, "on") == 0 ||
		    strcasecmp(value, "yes") == 0)
			flag = 1;
		else if ((value[0] == '0' && value[1] == '\0') ||
		    strcasecmp(value, "off") == 0 ||
		    strcasecmp(value, "no") == 0)
			flag = 0;
		else {
			ctx->error(ctx, "bad value: %s", value);
			return;
		}
	}

	o = options_set_number(oo, entry->name, flag);
	ctx->info(
	    ctx, "set option: %s -> %s", o->name, set_option_print(entry, o));
}

void
set_option_choice(struct cmd_ctx *ctx, struct options *oo,
    const struct set_option_entry *entry, char *value)
{
	struct options_entry	*o;
	const char     	       **choicep;
	int		 	 n, choice = -1;

	if (value == NULL) {
		ctx->error(ctx, "empty value");
		return;
	}

	n = 0;
	for (choicep = entry->choices; *choicep != NULL; choicep++) {
		n++;
		if (strncmp(*choicep, value, strlen(value)) != 0)
			continue;

		if (choice != -1) {
			ctx->error(ctx, "ambiguous option: %s", value);
			return;
		}
		choice = n - 1;
	}
	if (choice == -1) {
		ctx->error(ctx, "unknown option: %s", value);
		return;
	}

	o = options_set_number(oo, entry->name, choice);
	ctx->info(
	    ctx, "set option: %s -> %s", o->name, set_option_print(entry, o));
}
