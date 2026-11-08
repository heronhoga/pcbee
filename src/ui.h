#ifndef UI_H
#define UI_H

#include "raylib.h"

bool ui_load_font(void);
void ui_unload_font(void);
/* Literal rendering/measurement: callers translate UI copy, never user paths. */
void ui_text(const char *copy, int x, int y, int size, Color color);
int ui_text_width(const char *copy, int size);

#endif
