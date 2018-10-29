#include <assert.h>
#include <errno.h>
#include "png.h"
#include <stdlib.h>
#include <string.h>

bool
png_decoder_init_from_filename (struct png_ctx *self,
                                const char *filename)
{
   assert (self != NULL);
   assert (filename != NULL);

   memset (self, 0x00, sizeof (struct png_ctx));

  self->file_obj = fopen (filename, "rb");
  if (self->file_obj == NULL)
     return false;

  /* Check PNG signature. */
  uint8_t header[8];
  ssize_t read_size = fread (header, 1, 8, self->file_obj);
  assert (read_size > 0);

  if (png_sig_cmp ((png_const_bytep) header, 0, 8) != 0) {
     png_clear (self);
     errno = EINVAL;
     return false;
  }

  /* Create the PNG decoder object. */
  self->png_ptr =
     png_create_read_struct (PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  if (self->png_ptr == NULL) {
     png_clear (self);
     errno = ENOMEM;
     return false;
  }

  /* Create the PNG info object. */
  self->info_ptr = png_create_info_struct (self->png_ptr);
  if (self->info_ptr == NULL) {
     png_clear (self);
     errno = ENOMEM;
     return false;
  }

  /* Initialize error handling. */
  if (setjmp (png_jmpbuf (self->png_ptr)) != 0) {
     png_clear (self);
     errno = ENOMEM;
     return false;
  }

  png_init_io (self->png_ptr, self->file_obj);
  png_set_sig_bytes (self->png_ptr, 8);

  /* @FIXME: does this generates errors? */
  png_read_info (self->png_ptr, self->info_ptr);

  self->width = png_get_image_width (self->png_ptr, self->info_ptr);
  self->height = png_get_image_height (self->png_ptr, self->info_ptr);
  assert (self->width > 0 && self->height > 0);

  self->format = png_get_color_type (self->png_ptr, self->info_ptr);
  self->row_stride = png_get_rowbytes (self->png_ptr, self->info_ptr);

  self->status = PNG_STATUS_DECODE_READY;

  return true;
}

void
png_clear (struct png_ctx *self)
{
   assert (self != NULL);

   if (self->png_ptr != NULL) {
      if (self->info_ptr != NULL)
         png_destroy_info_struct (self->png_ptr, &self->info_ptr);

      png_destroy_read_struct (&self->png_ptr, &self->info_ptr, NULL);
   }

   if (self->file_obj != NULL) {
      fclose (self->file_obj);
      self->file_obj = NULL;
   }

   self->status = PNG_STATUS_NONE;
}

ssize_t
png_read (struct png_ctx *self,
          void *buffer,
          size_t size,
          size_t *first_row,
          size_t *num_rows)
{
   assert (self != NULL);
   assert (self->status == PNG_STATUS_DECODE_READY ||
           self->status == PNG_STATUS_DONE);
   assert (size == 0 || buffer != NULL);
   assert (self->row_stride > 0 && size >= self->row_stride);

   size_t _num_rows = 0;
   size_t _first_row = 0;
   size_t result = 0;

   if (self->status == PNG_STATUS_DONE)
      goto out;

   _first_row = self->last_decoded_row;
   uint32_t max_read_rows = size / self->row_stride;

#define MIN(a,b) (a > b ? b : a)
   _num_rows = MIN (self->height - self->last_decoded_row,
                    max_read_rows);
#undef MIN

   png_bytepp rows = calloc (_num_rows, sizeof (png_bytep));
   for (int32_t i = 0; i < _num_rows; i++)
      rows[i] = buffer + (i * self->row_stride);

   png_read_rows (self->png_ptr,
                  rows,
                  NULL,
                  _num_rows);
   free (rows);

   self->last_decoded_row += _num_rows;
   result = _num_rows * self->row_stride;

   if (self->last_decoded_row == self->height) {
      png_read_end (self->png_ptr, self->info_ptr);

      self->status = PNG_STATUS_DONE;
   }

 out:
   if (first_row != NULL)
      *first_row = _first_row;

   if (num_rows != NULL)
      *num_rows = _num_rows;

   return result;
}
