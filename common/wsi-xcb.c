#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xcb/xcb.h>
#include "wsi.h"

static struct {
   xcb_connection_t* conn;
   xcb_screen_t* screen;
   xcb_window_t win;
   xcb_intern_atom_reply_t* atom_wm_delete_window;
   WsiExposeEvent expose_event;
} xcb_data = { 0, };

static bool
wsi_handle_event_xcb (xcb_generic_event_t* event)
{
   while (event != NULL) {
      uint8_t event_code = event->response_type & 0x7f;

      switch (event_code) {
      case XCB_EXPOSE:
         if (xcb_data.expose_event != NULL)
            xcb_data.expose_event ();
         break;

      case XCB_CLIENT_MESSAGE:
         if ((* (xcb_client_message_event_t*) event).data.data32[0] ==
             (* xcb_data.atom_wm_delete_window).atom) {
            return false;
         }
         break;

      case XCB_KEY_RELEASE: {
         const xcb_key_release_event_t* key =
            (const xcb_key_release_event_t*) event;
         switch (key->detail) {
         case 0x9:
            /* ESC key */
            return false;
            break;

         case 0x29:
            /* F key */
            wsi_toggle_fullscreen ();
            break;

         default:
            printf ("key pressed: %x\n", key->detail);
            break;
         }
         break;
      }

      default:
         break;
      }

      free (event);
      event = xcb_poll_for_event (xcb_data.conn);
   }

   return true;
}

void
wsi_toggle_fullscreen (void)
{
   static bool fullscreen_mode = false;

   fullscreen_mode = ! fullscreen_mode;

   xcb_intern_atom_cookie_t cookie =
      xcb_intern_atom (xcb_data.conn, 0, 13, "_NET_WM_STATE");
   xcb_intern_atom_reply_t* reply = xcb_intern_atom_reply (xcb_data.conn,
                                                           cookie,
                                                           0);
   xcb_atom_t atom1 = reply->atom;
   free (reply);

   cookie = xcb_intern_atom (xcb_data.conn, 0, 24, "_NET_WM_STATE_FULLSCREEN");
   reply = xcb_intern_atom_reply (xcb_data.conn, cookie, 0);
   xcb_atom_t atom2 = reply->atom;
   free (reply);

   xcb_client_message_event_t msg = {0};
   msg.response_type = XCB_CLIENT_MESSAGE;
   msg.window = xcb_data.win;
   msg.format = 32;
   msg.type = atom1;
   memset (msg.data.data32, 0, 5 * sizeof (uint32_t));
   msg.data.data32[0] = fullscreen_mode ? 1 : 0;
   msg.data.data32[1] = atom2;

   xcb_send_event (xcb_data.conn,
                   true,
                   xcb_data.screen->root,
                   XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY,
                   (const char *) &msg);
   xcb_flush (xcb_data.conn);
}

bool
wsi_init (const char* win_title,
          uint32_t width,
          uint32_t height,
          WsiExposeEvent expose_event)
{
   /* connection to the X server */
   xcb_data.conn = xcb_connect (NULL, NULL);
   if (xcb_data.conn == NULL) {
      printf ("XCB: Error: Failed to connect to X server\n");
      return false;
   }
   printf ("XCB: Connected to the X server\n");

   /* Get the first screen */
   const xcb_setup_t* xcb_setup  = xcb_get_setup (xcb_data.conn);
   xcb_screen_iterator_t iter   = xcb_setup_roots_iterator (xcb_setup);
   xcb_data.screen = iter.data;

   /* Create the window */
   xcb_data.win = xcb_generate_id (xcb_data.conn);

   uint32_t value_mask, value_list[32];
   value_mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
   value_list[0] = xcb_data.screen->black_pixel;
   value_list[1] = XCB_EVENT_MASK_KEY_RELEASE | XCB_EVENT_MASK_EXPOSURE |
      XCB_EVENT_MASK_STRUCTURE_NOTIFY;

   xcb_create_window (xcb_data.conn,                 /* Connection          */
                      XCB_COPY_FROM_PARENT,          /* depth (same as root)*/
                      xcb_data.win,                  /* window Id           */
                      xcb_data.screen->root,         /* parent window       */
                      0, 0,                          /* x, y                */
                      width, height,                 /* width, height       */
                      0,                             /* border_width        */
                      XCB_WINDOW_CLASS_INPUT_OUTPUT, /* class               */
                      xcb_data.screen->root_visual,  /* visual              */
                      value_mask, value_list);       /* masks, not used yet */

   xcb_intern_atom_cookie_t cookie =
      xcb_intern_atom (xcb_data.conn, 1, 12, "WM_PROTOCOLS");
   xcb_intern_atom_reply_t* reply = xcb_intern_atom_reply (xcb_data.conn,
                                                           cookie,
                                                           0);

   xcb_intern_atom_cookie_t cookie1 =
      xcb_intern_atom (xcb_data.conn, 0, 16, "WM_DELETE_WINDOW");
   xcb_data.atom_wm_delete_window = xcb_intern_atom_reply (xcb_data.conn,
                                                           cookie1,
                                                           0);

   xcb_change_property (xcb_data.conn,
                        XCB_PROP_MODE_REPLACE,
                        xcb_data.win,
                        (*reply).atom,
                        4, 32, 1,
                        &(*xcb_data.atom_wm_delete_window).atom);

   free (reply);

   /* Make sure commands are sent before we pause so that the window gets
    * shown.
    */
   xcb_flush (xcb_data.conn);

   xcb_data.expose_event = expose_event;

   return true;
}

void
wsi_get_connection_and_window (const void** conn, const void** win)
{
   if (conn != NULL)
      *conn = (const void*) xcb_data.conn;

   if (win != NULL)
      *win = (const void*) &xcb_data.win;
}

bool
wsi_wait_for_events (void)
{
   xcb_generic_event_t* event;

   event = xcb_wait_for_event (xcb_data.conn);
   return wsi_handle_event_xcb (event);
}

void
wsi_window_show (void)
{
   xcb_map_window (xcb_data.conn, xcb_data.win);
}

void
wsi_finish (void)
{
   if (xcb_data.conn == NULL)
      return;

   /* Unmap the window from the screen */
   xcb_unmap_window (xcb_data.conn, xcb_data.win);

   free (xcb_data.atom_wm_delete_window);

   /* disconnect from the X server */
   xcb_disconnect (xcb_data.conn);
}
