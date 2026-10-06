/*
 * Fuzz the tmux JSON parser and serializer.
 */

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "tmux.h"

struct event_base *libevent;

static void
walk_json(struct json_node *node)
{
	struct json_node	*child, *object;
	const char		*string;
	int64_t		 number;
	int		 boolean;

	if (node == NULL)
		return;

	(void)json_get_string(node, &string);
	(void)json_get_number(node, &number);
	(void)json_get_boolean(node, &boolean);

	if (json_get_object(node, &object) == 0) {
		(void)json_find(object, "V");
		(void)json_find(object, "L");
	}

	for (child = json_array_first(node); child != NULL;
	    child = json_array_next(child))
		walk_json(child);
}

int
LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	struct json_node	*node;
	char			*input, *serialized, *cause = NULL;

	if (size == 0 || size > 1U << 20)
		return 0;

	input = malloc(size + 1);
	if (input == NULL)
		return 0;
	memcpy(input, data, size);
	input[size] = '\0';

	node = json_parse(input, &cause);
	free(cause);
	if (node != NULL) {
		walk_json(node);
		serialized = json_to_string(node);
		free(serialized);
		json_destroy_node(node);
	}

	free(input);
	return 0;
}

int
LLVMFuzzerInitialize(__unused int *argc, __unused char ***argv)
{
	const struct options_table_entry	*oe;

	global_environ = environ_create();
	global_options = options_create(NULL);
	global_s_options = options_create(NULL);
	global_w_options = options_create(NULL);
	for (oe = options_table; oe->name != NULL; oe++) {
		if (oe->scope & OPTIONS_TABLE_SERVER)
			options_default(global_options, oe);
		if (oe->scope & OPTIONS_TABLE_SESSION)
			options_default(global_s_options, oe);
		if (oe->scope & OPTIONS_TABLE_WINDOW)
			options_default(global_w_options, oe);
	}
	libevent = osdep_event_init();
	socket_path = xstrdup("dummy");

	return 0;
}