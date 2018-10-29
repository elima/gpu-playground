#pragma once

#include <unistd.h>
#include <setjmp.h>
#include <stdbool.h>
#include <stdio.h>
#include <jpeglib.h>
#include <stdint.h>

enum jpeg_status {
   JPEG_STATUS_NONE = 0,
   JPEG_STATUS_DECODE_READY,
   JPEG_STATUS_ENCODE_READY,
   JPEG_STATUS_ERROR,
   JPEG_STATUS_DONE,
};

enum jpeg_format {
  JPEG_FORMAT_UNKNOWN,        /* error/unspecified */
  JPEG_FORMAT_GRAYSCALE,      /* monochrome */
  JPEG_FORMAT_RGB,            /* red/green/blue as specified by the RGB_RED,
                                 RGB_GREEN, RGB_BLUE, and RGB_PIXELSIZE macros */
  JPEG_FORMAT_YCbCr,          /* Y/Cb/Cr (also known as YUV) */
  JPEG_FORMAT_CMYK,           /* C/M/Y/K */
  JPEG_FORMAT_YCCK,           /* Y/Cb/Cr/K */
  JPEG_FORMAT_EXT_RGB,        /* red/green/blue */
  JPEG_FORMAT_EXT_RGBX,       /* red/green/blue/x */
  JPEG_FORMAT_EXT_BGR,        /* blue/green/red */
  JPEG_FORMAT_EXT_BGRX,       /* blue/green/red/x */
  JPEG_FORMAT_EXT_XBGR,       /* x/blue/green/red */
  JPEG_FORMAT_EXT_XRGB,       /* x/red/green/blue */
  /* When out_color_space is set to JCS_EXT_RGBX, JCS_EXT_BGRX, JCS_EXT_XBGR,
     or JCS_EXT_XRGB during decompression, the X byte is undefined, and in
     order to ensure the best performance, libjpeg-turbo can set that byte to
     whatever value it wishes.  Use the following colorspace constants to
     ensure that the X byte is set to 0xFF, so that it can be interpreted as an
     opaque alpha channel. */
  JPEG_FORMAT_EXT_RGBA,       /* red/green/blue/alpha */
  JPEG_FORMAT_EXT_BGRA,       /* blue/green/red/alpha */
  JPEG_FORMAT_EXT_ABGR,       /* alpha/blue/green/red */
  JPEG_FORMAT_EXT_ARGB,       /* alpha/red/green/blue */
  JPEG_FORMAT_RGB565          /* 5-bit red/6-bit green/5-bit blue */
};

struct jpeg_ctx;

struct jpeg_error_handler {
   struct jpeg_error_mgr jpeg_error_mgr;

   struct jpeg_ctx *ctx;
   jmp_buf setjmp_buffer;
};

struct jpeg_ctx {
   FILE *file_obj;
   struct jpeg_decompress_struct cinfo;

   struct jpeg_error_mgr err_manager;

   enum jpeg_status status;

   uint32_t width;
   uint32_t height;
   uint32_t row_stride;
   enum jpeg_format format;

   struct jpeg_error_handler err_handler;
};

bool
jpeg_decoder_init_from_filename (struct jpeg_ctx *self,
                                 const char *filename);

void
jpeg_clear (struct jpeg_ctx *self);

ssize_t
jpeg_read (struct jpeg_ctx *self,
           void *buffer,
           size_t size,
           size_t *first_row,
           size_t *num_rows);
