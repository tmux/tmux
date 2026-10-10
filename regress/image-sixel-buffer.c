/* Exercise SIXEL output lengths at the encoder's allocation boundary. */

#undef NDEBUG
#include <assert.h>

#include "../tmux.h"

#ifdef ENABLE_IMAGES
#include "../image-sixel.c"
#include "../xmalloc.c"

/* Abort if an allocation or formatting check fails. */
void
fatal(__unused const char *fmt, ...)
{
	abort();
}

/* Abort if an allocation or formatting check fails. */
void
fatalx(__unused const char *fmt, ...)
{
	abort();
}

/* Encode lengths below, at and above the initial 8192-byte allocation. */
int
main(void)
{
	struct sixel_image	 image = { 0 };
	u_int			 colour, width, x;
	size_t			 size;
	char			*data;

	colour = (2u << 25) | (100u << 16) | (100u << 8) | 100u;
	image.sy = 2;
	image.ncolours = 1;
	image.used_colours = 1;
	image.colours = &colour;
	image.lines = xcalloc(2, sizeof *image.lines);

	/* Headers and controls add 26 bytes to these uncompressed rasters. */
	for (width = 8165; width <= 8167; width++) {
		image.sx = width;
		image.lines[0].sx = width;
		image.lines[1].sx = width;
		image.lines[0].pixels = xcalloc(width, sizeof(uint16_t));
		image.lines[1].pixels = xcalloc(width, sizeof(uint16_t));

		/* Alternate row bits so adjacent columns cannot be compressed. */
		for (x = 0; x < width; x++)
			image.lines[x % 2].pixels[x] = 1;
		data = sixel_print(&image, NULL, &size);
		assert(data != NULL);
		assert(size == width + 26);
		assert(data[size] == '\0');
		assert(strlen(data) == size);
		free(data);
		free(image.lines[0].pixels);
		free(image.lines[1].pixels);
	}
	free(image.lines);
	return (0);
}
#else
/* Skip the encoder check when image support is disabled. */
int
main(void)
{
	return (0);
}
#endif
