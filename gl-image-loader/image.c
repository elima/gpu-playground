#include <assert.h>
#include <errno.h>
#include "image.h"

bool
o_image_init_from_filename (struct o_image *self,
                            const char *filename)
{
   assert (self != NULL);
   assert (filename != NULL);

   /* Try PNG. */
   bool ok = png_decoder_init_from_filename (&self->png, filename);
   if (ok) {
      self->type = O_IMAGE_TYPE_PNG;
      self->width = self->png.width;
      self->height = self->png.height;

      switch (self->png.format) {
      case PNG_COLOR_TYPE_RGB:
         self->format = O_IMAGE_FORMAT_RGB;
         break;
      case PNG_COLOR_TYPE_RGB_ALPHA:
         self->format = O_IMAGE_FORMAT_RGBA;
         break;
      default:
         assert (!"PNG image format not handled\n");
      }
   } else {
      png_clear (&self->png);

      /* Try JPEG. */
      bool ok = jpeg_decoder_init_from_filename (&self->jpeg, filename);
      if (ok) {
         assert (self->jpeg.status == JPEG_STATUS_DECODE_READY);

         self->type = O_IMAGE_TYPE_JPEG;
         self->width = self->jpeg.width;
         self->height = self->jpeg.height;

         switch (self->jpeg.format) {
         case JPEG_FORMAT_RGB:
         case JPEG_FORMAT_EXT_RGB:
            self->format = O_IMAGE_FORMAT_RGB;
            break;
         case JPEG_FORMAT_EXT_RGBA:
            self->format = O_IMAGE_FORMAT_RGBA;
            break;
         default:
            printf ("JPEG format: %d\n", self->jpeg.format);
            assert (!"JPEG image format not handled\n");
         }
      } else {
         printf ("Unknown or unhandled image format.\n");
         return false;
      }
   }

   return true;
}

void
o_image_clear (struct o_image *self)
{
   assert (self != NULL);

   if (self->type == O_IMAGE_TYPE_PNG)
      png_clear (&self->png);
   else if (self->type == O_IMAGE_TYPE_JPEG)
      jpeg_clear (&self->jpeg);
}

ssize_t
o_image_read (struct o_image *self,
              void *buffer,
              size_t size,
              size_t *first_row,
              size_t *num_rows)
{
   assert (self != NULL);

   switch (self->type) {
   case O_IMAGE_TYPE_PNG:
      return png_read (&self->png,
                       buffer,
                       size,
                       first_row,
                       num_rows);

   case O_IMAGE_TYPE_JPEG:
      return jpeg_read (&self->jpeg,
                        buffer,
                        size,
                        first_row,
                        num_rows);

   default:
      errno = ENXIO;
      return -1;
   }
}
