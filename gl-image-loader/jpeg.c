#include <assert.h>
#include <errno.h>
#include "jpeg.h"
#include <string.h>

static void
handle_error_exit (j_common_ptr cinfo)
{
   struct jpeg_error_handler *err_handler =
      (struct jpeg_error_handler *) cinfo->err;

   /* Display the message. */
   (*cinfo->err->output_message) (cinfo);

   err_handler->ctx->status = JPEG_STATUS_ERROR;

   longjmp (err_handler->setjmp_buffer, 1);
}

/* public API */

bool
jpeg_decoder_init_from_filename (struct jpeg_ctx *self,
                                 const char *filename)
{
   assert (self != NULL);
   assert (filename != NULL);

   memset (self, 0x00, sizeof (struct jpeg_ctx));

   self->file_obj = fopen (filename, "rb");
   if (self->file_obj == NULL)
      return false;

   /* Set an error manager. */
   self->cinfo.err =
      jpeg_std_error (&self->err_handler.jpeg_error_mgr);
   self->err_handler.jpeg_error_mgr.error_exit =
      handle_error_exit;

   self->err_handler.ctx = self;

   if (setjmp (self->err_handler.setjmp_buffer) != 0) {
      /* @FIXME: check the error code and fill errno accordingly */
      jpeg_clear (self);
      return false;
   }

   /* Create and set up the decompression object. */
   jpeg_create_decompress (&self->cinfo);
   jpeg_stdio_src (&self->cinfo, self->file_obj);

   /* Read JPEG header. */
   int result = jpeg_read_header (&self->cinfo, true);
   if (result != JPEG_HEADER_OK) {
      jpeg_clear (self);
      return false;
   }

   jpeg_start_decompress (&self->cinfo);

   self->row_stride = self->cinfo.output_width * self->cinfo.output_components;
   self->width = self->cinfo.output_width;
   self->height = self->cinfo.output_height;
   self->format = self->cinfo.out_color_space;

   self->status = JPEG_STATUS_DECODE_READY;

   return true;
}

void
jpeg_clear (struct jpeg_ctx *self)
{
   if (self->file_obj != NULL) {
      fclose (self->file_obj);
      self->file_obj = NULL;
   }

   if (self->status != JPEG_STATUS_NONE)
      jpeg_destroy_decompress (&self->cinfo);

   self->status = JPEG_STATUS_NONE;
}

ssize_t
jpeg_read (struct jpeg_ctx *self,
           void *buffer,
           size_t size,
           size_t *first_row,
           size_t *num_rows)
{
   assert (self != NULL);
   assert (self->status == JPEG_STATUS_DECODE_READY ||
           self->status == JPEG_STATUS_DONE);
   assert (size == 0 || buffer != NULL);
   assert (self->row_stride > 0 && size >= self->row_stride);

   size_t _num_rows = 0;
   size_t _first_row = 0;
   size_t result = 0;

   if (self->status == JPEG_STATUS_DONE)
      goto out;

   _first_row = self->cinfo.output_scanline;

   uint32_t lines = size / self->row_stride;
   for (int32_t i = 0; i < lines; i++) {
      uint8_t *rowptr[1];
      rowptr[0] = buffer + self->row_stride * i;

      jpeg_read_scanlines (&self->cinfo, rowptr, 1);
      if (self->status == JPEG_STATUS_ERROR) {
         /* @FIXME: handle exit errors here */
         return -1;
      }

      _num_rows++;
   }

   _num_rows = self->cinfo.output_scanline - _first_row;

   if (self->cinfo.output_scanline == self->cinfo.output_height) {
      jpeg_finish_decompress (&self->cinfo);
      self->status = JPEG_STATUS_DONE;
   }

   result = _num_rows * self->row_stride;

 out:
   if (first_row != NULL)
      *first_row = _first_row;

   if (num_rows != NULL)
      *num_rows = _num_rows;

   return result;
}
