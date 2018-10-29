#pragma once

#include "jpeg.h"
#include "png.h"
#include <stdint.h>
#include <stdbool.h>

enum o_image_format {
   O_IMAGE_FORMAT_INVALID,
   O_IMAGE_FORMAT_RGB,
   O_IMAGE_FORMAT_RGBA,
};

enum o_image_type {
   O_IMAGE_TYPE_INVALID,
   O_IMAGE_TYPE_PNG,
   O_IMAGE_TYPE_JPEG,
};

struct o_image {
   uint32_t width;
   uint32_t height;

   uint8_t type;
   uint32_t format;

   struct png_ctx png;
   struct jpeg_ctx jpeg;
};

bool
o_image_init_from_filename (struct o_image *self,
                            const char *filename);

void
o_image_clear (struct o_image *self);

ssize_t
o_image_read (struct o_image *self,
              void *buffer,
              size_t size,
              size_t *first_row,
              size_t *num_rows);
