/*
 * Fuzz the SIXEL image payload parser.
 *
 * The production DCS handler passes the payload beginning with 'q' directly to
 * sixel_parse(). Keep this target on that same boundary so mutations exercise
 * SIXEL attributes, colour registers, repeat runs, and raster data without
 * requiring a pane, client, or terminal session.
 */

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "tmux.h"

#define SIXEL_FUZZER_MAXLEN 1024

int
LLVMFuzzerTestOneInput(const u_char *data, size_t size)
{
	struct sixel_image *image;
	u_int width, height;
	char *payload;

	if (size < 2 || size > SIXEL_FUZZER_MAXLEN || data[0] != 'q')
		return (0);

	payload = malloc(size + 1);
	if (payload == NULL)
		return (0);
	memcpy(payload, data, size);
	payload[size] = '\0';

	image = sixel_parse(payload, size, 0, 8, 16);
	free(payload);
	if (image == NULL)
		return (0);

	sixel_size_in_cells(image, &width, &height);
	(void)width;
	(void)height;
	sixel_free(image);
	return (0);
}
