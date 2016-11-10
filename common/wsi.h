#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef void (* WsiExposeEvent) (void);

bool wsi_init                      (const char* win_title,
                                    uint32_t width,
                                    uint32_t height,
                                    WsiExposeEvent expose_event);

void wsi_get_connection_and_window (const void** conn,
                                    const void** win);

void wsi_toggle_fullscreen         (void);

bool wsi_wait_for_events           (void);

void wsi_window_show               (void);

void wsi_finish                    (void);
