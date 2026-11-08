#include "ui.h"

#include <math.h>
#include <stdlib.h>

unsigned char *font_read_data(const char *directory, int *size);
static Font font;
static bool loaded;

bool ui_load_font(void)
{
    font = GetFontDefault();
    int size = 0;
    unsigned char *data = font_read_data(GetApplicationDirectory(), &size);
    if (data) {
        font = LoadFontFromMemory(".ttf", data, size, 48, NULL, 224);
        free(data);
        loaded = font.texture.id != 0 && font.texture.id != GetFontDefault().texture.id;
    }
    if (loaded) SetTextureFilter(font.texture, TEXTURE_FILTER_BILINEAR);
    else {
        font = GetFontDefault();
        TraceLog(LOG_WARNING, "PCBEE: bundled monospace font could not be loaded");
    }
    return loaded;
}

void ui_unload_font(void)
{
    if (loaded) UnloadFont(font);
    loaded = false;
}

void ui_text(const char *copy, int x, int y, int size, Color color)
{
    DrawTextEx(font, copy, (Vector2){(float)x, (float)y}, (float)size, 0, color);
}

int ui_text_width(const char *copy, int size)
{
    return (int)ceilf(MeasureTextEx(font, copy, (float)size, 0).x);
}
