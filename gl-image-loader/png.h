#pragma once

#define PNG_DEBUG 3
#include <png.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>

enum png_status {
   PNG_STATUS_NONE = 0,
   PNG_STATUS_DECODE_READY,
   PNG_STATUS_ENCODE_READY,
   PNG_STATUS_ERROR,
   PNG_STATUS_DONE,
};

struct png_ctx {
   FILE *file_obj;

   png_structp png_ptr;
   png_infop info_ptr;

   enum png_status status;

   uint32_t width;
   uint32_t height;
   size_t row_stride;
   uint8_t format;

   uint32_t last_decoded_row;
};

bool
png_decoder_init_from_filename (struct png_ctx *self,
                                const char *filename);

void
png_clear (struct png_ctx *self);

ssize_t
png_read (struct png_ctx *self,
          void *buffer,
          size_t size,
          size_t *first_row,
          size_t *num_rows);
