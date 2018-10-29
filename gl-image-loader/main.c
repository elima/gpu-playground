#include <assert.h>
#include <GLES2/gl2.h>
#include <GLFW/glfw3.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "image.h"

#define IMAGE_FILENAME_DEFAULT "./igalia-white-text.png"

static bool
gl_utils_print_shader_log (GLuint shader)
{
   GLint length;
   char buffer[4096] = {0};
   GLint success;

   glGetShaderiv (shader, GL_INFO_LOG_LENGTH, &length);
   if (length == 0)
      return true;

   glGetShaderInfoLog (shader, 4096, NULL, buffer);
   if (strlen (buffer) > 0)
      printf ("Shader compilation log: %s\n", buffer);

   glGetShaderiv (shader, GL_COMPILE_STATUS, &success);

   return success == GL_TRUE;
}

static GLuint
gl_utils_load_shader (const char *shader_source, GLenum type)
{
   GLuint shader = glCreateShader (type);

   glShaderSource (shader, 1, &shader_source, NULL);
   assert (glGetError () == GL_NO_ERROR);
   glCompileShader (shader);
   assert (glGetError () == GL_NO_ERROR);

   gl_utils_print_shader_log (shader);

   return shader;
}

static GLuint
create_shader_program (void)
{
   const char *VERTEX_SOURCE =
      "attribute vec2 pos;\n"
      "attribute vec2 texture;\n"
      "varying vec2 v_texture;\n"
      "void main() {\n"
      "  v_texture = texture;\n"
      "  gl_Position = vec4(pos, 0, 1);\n"
      "}\n";

   const char *FRAGMENT_SOURCE =
      "precision mediump float;\n"
      "uniform sampler2D u_tex;\n"
      "varying vec2 v_texture;\n"
      "void main() {\n"
      "  gl_FragColor = texture2D(u_tex, v_texture);\n"
      "}\n";

   GLuint vertex_shader = gl_utils_load_shader (VERTEX_SOURCE,
                                                GL_VERTEX_SHADER);
   assert (vertex_shader >= 0);
   assert (glGetError () == GL_NO_ERROR);

   GLuint fragment_shader = gl_utils_load_shader (FRAGMENT_SOURCE,
                                                  GL_FRAGMENT_SHADER);
   assert (fragment_shader >= 0);
   assert (glGetError () == GL_NO_ERROR);

   GLuint program = glCreateProgram ();
   assert (glGetError () == GL_NO_ERROR);
   glAttachShader (program, vertex_shader);
   assert (glGetError () == GL_NO_ERROR);
   glAttachShader (program, fragment_shader);
   assert (glGetError () == GL_NO_ERROR);

   glBindAttribLocation (program, 0, "pos");
   glBindAttribLocation (program, 1, "texture");

   glLinkProgram (program);
   assert (glGetError () == GL_NO_ERROR);

   glDeleteShader (vertex_shader);
   glDeleteShader (fragment_shader);

   return program;
}

int32_t
main (int32_t argc, char *argv[])
{
   printf ("Usage: %s <path-to-PNG-or-JPEG-image>\n", argv[0]);

   /* Load an decode an image. */
   static struct o_image image;

   const char *image_url;
   if (argc > 1)
      image_url = argv[1];
   else
      image_url = IMAGE_FILENAME_DEFAULT;

   /* This loads the image header (metadata), but doesn't load any pixel
    * data or do any decoding.
    */
   if (! o_image_init_from_filename (&image, image_url))
      return -1;

   GLFWwindow* window;

   /* Initialize GLFW. */
   if (! glfwInit ())
      return -1;

   /* Select an OpenGL-ES 2.0 profile. */
   glfwWindowHint (GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
   glfwWindowHint (GLFW_CONTEXT_VERSION_MAJOR, 2);
   glfwWindowHint (GLFW_CONTEXT_VERSION_MINOR, 0);

   /* Create a windowed mode window and its OpenGL context */
   window = glfwCreateWindow (image.width,
                              image.height,
                              "GL Image Loader",
                              NULL,
                              NULL);
   if (window == NULL) {
      glfwTerminate ();
      return -1;
   }

   /* Make the window's context current */
   glfwMakeContextCurrent (window);

   /* Dump some GL capabilities. */
   const GLubyte *gles_version = glGetString (GL_VERSION);
   printf ("%s\n", (char *) gles_version);

   /* Create a texture for the image. */
   GLuint tex;
   glGenTextures (1, &tex);
   assert (glGetError () == GL_NO_ERROR);
   assert (tex > 0);
   glBindTexture (GL_TEXTURE_2D, tex);
   assert (glGetError () == GL_NO_ERROR);
   glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
   glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

   /* Load the image into the texture, progressively in chunks of max
    * BLOCK_SIZE bytes. */
#define BLOCK_SIZE (8192 * 1)
   uint8_t buf[BLOCK_SIZE] = {0, };
   size_t first_row;
   size_t num_rows;

   GLuint format = image.format == O_IMAGE_FORMAT_RGB ?
      GL_RGB : GL_RGBA;

   /* Allocate the texture size. */
   glTexImage2D (GL_TEXTURE_2D,
                 0,
                 format,
                 image.width, image.height,
                 0,
                 format,
                 GL_UNSIGNED_BYTE,
                 NULL);
   assert (glGetError () == GL_NO_ERROR);

   ssize_t size_read;
   do {
      size_read = o_image_read (&image,
                                buf,
                                BLOCK_SIZE,
                                &first_row,
                                &num_rows);
      assert (size_read >= 0);
      if (size_read > 0) {
         glTexSubImage2D (GL_TEXTURE_2D,
                          0,
                          0, first_row,
                          image.width, num_rows,
                          format,
                          GL_UNSIGNED_BYTE,
                          buf);
         assert (glGetError () == GL_NO_ERROR);
      }
   } while (size_read > 0);

   glBindTexture (GL_TEXTURE_2D, 0);
#undef BLOCK_SIZE

   /* Create shader program to sample the texture. */
   GLuint program = create_shader_program ();
   glUseProgram (program);
   assert (glGetError () == GL_NO_ERROR);

   /* Loop until the user closes the window */
   while (! glfwWindowShouldClose (window)) {
      /* Render here */
      glClearColor (0.25, 0.25, 0.25, 0.5);
      glClear (GL_COLOR_BUFFER_BIT);

      /* Bind the texture. */
      glActiveTexture (GL_TEXTURE0);
      glBindTexture (GL_TEXTURE_2D, tex);
      assert (glGetError () == GL_NO_ERROR);

      /* Enable blending for transparent PNGs. */
      glEnable (GL_BLEND);
      glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

      /* Draw a quad. */
      static const GLfloat s_vertices[4][2] = {
         { -1.0,  1.0 },
         {  1.0,  1.0 },
         { -1.0, -1.0 },
         {  1.0, -1.0 },
      };

      static const GLfloat s_texturePos[4][2] = {
         { 0, 0 },
         { 1, 0 },
         { 0, 1 },
         { 1, 1 },
      };

      glVertexAttribPointer (0, 2, GL_FLOAT, GL_FALSE, 0, s_vertices);
      glVertexAttribPointer (1, 2, GL_FLOAT, GL_FALSE, 0, s_texturePos);

      glEnableVertexAttribArray (0);
      glEnableVertexAttribArray (1);

      glDrawArrays (GL_TRIANGLE_STRIP, 0, 4);

      glDisableVertexAttribArray (0);
      glDisableVertexAttribArray (1);

      /* Swap front and back buffers */
      glfwSwapBuffers (window);

      /* Poll and process events */
      glfwPollEvents ();
   }

   glfwTerminate ();

   return 0;
}
