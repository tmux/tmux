#ifndef IMAGE_KITTY_H
#define IMAGE_KITTY_H

struct kitty_image {
	uint8_t		*data;
	size_t		 data_len;
	uint32_t	 width;
	uint32_t	 height;
	uint32_t	 format; /* GhosttyKittyImageFormat */
	uint32_t	 compression; /* GhosttyKittyImageCompression */
	uint32_t	 source_x;
	uint32_t	 source_y;
	uint32_t	 source_width;
	uint32_t	 source_height;
	uint32_t	 image_id;
	uint32_t	 placement_id;
};

#endif
