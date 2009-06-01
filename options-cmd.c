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

void
set_option_string(struct cmd_ctx *ctx, struct options *oo,
    const struct set_option_entry *entry, char *value)
{
	if (value == NULL) {
		ctx->error(ctx, "empty value");
		return;
	}

	options_set_string(oo, entry->name, "%s", value);
	ctx->info(ctx, "set option: %s -> %s", entry->name, value);
}

void
set_option_number(struct cmd_ctx *ctx, struct options *oo,
    const struct set_option_entry *entry, char *value)
{
	long long	number;
	const char     *errstr;

	if (value == NULL) {
		ctx->error(ctx, "empty value");
		return;
	}

	number = strtonum(value, entry->minimum, entry->maximum, &errstr);
	if (errstr != NULL) {
		ctx->error(ctx, "value is %s: %s", errstr, value);
		return;
	}
	options_set_number(oo, entry->name, number);
	ctx->info(ctx, "set option: %s -> %lld", entry->name, number);
}

void
set_option_key(struct cmd_ctx *ctx, struct options *oo,
    const struct set_option_entry *entry, char *value)
{
	int	key;

	if (value == NULL) {
		ctx->error(ctx, "empty value");
		return;
	}

	if ((key = key_string_lookup_string(value)) == KEYC_NONE) {
		ctx->error(ctx, "unknown key: %s", value);
		return;
	}
	options_set_number(oo, entry->name, key);
	ctx->info(ctx,
	    "set option: %s -> %s", entry->name, key_string_lookup_key(key));
}

void
set_option_colour(struct cmd_ctx *ctx, struct options *oo,
    const struct set_option_entry *entry, char *value)
{
	int	colour;

	if (value == NULL) {
		ctx->error(ctx, "empty value");
		return;
	}

	if ((colour = colour_fromstring(value)) == -1) {
		ctx->error(ctx, "bad colour: %s", value);
		return;
	}

	options_set_number(oo, entry->name, colour);
	ctx->info(ctx,
	    "set option: %s -> %s", entry->name, colour_tostring(colour));
}

void
set_option_attributes(struct cmd_ctx *ctx, struct options *oo,
    const struct set_option_entry *entry, char *value)
{
	int	attr;

	if (value == NULL) {
		ctx->error(ctx, "empty value");
		return;
	}

	if ((attr = attributes_fromstring(value)) == -1) {
		ctx->error(ctx, "bad attributes: %s", value);
		return;
	}

	options_set_number(oo, entry->name, attr);
	ctx->info(ctx,
	    "set option: %s -> %s", entry->name, attributes_tostring(attr));
}

void
set_option_flag(struct cmd_ctx *ctx, struct options *oo,
    const struct set_option_entry *entry, char *value)
{
	int	flag;

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

	options_set_number(oo, entry->name, flag);
	ctx->info(ctx,
	    "set option: %s -> %s", entry->name, flag ? "on" : "off");
}

void
set_option_choice(struct cmd_ctx *ctx, struct options *oo,
    const struct set_option_entry *entry, char *value)
{
	const char     **choicep;
	int		 n, choice = -1;

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

	options_set_number(oo, entry->name, choice);
	ctx->info(ctx,
	    "set option: %s -> %s", entry->name, entry->choices[choice]);
}
