/* Font IO is separate from raylib to avoid Windows header name collisions. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wchar.h>
#endif

unsigned char *font_read_data(const char *directory, int *size)
{
    FILE *file = NULL;
#ifdef _WIN32
    (void)directory;
    wchar_t path[4096];
    DWORD length = GetModuleFileNameW(NULL, path, 4096);
    if (!length || length >= 4096) return NULL;
    wchar_t *name = wcsrchr(path, L'\\');
    if (!name || (size_t)(name - path) + 50 >= 4096) return NULL;
    wcscpy(name + 1, L"assets\\fonts\\IBMPlexMono-Regular.ttf");
    file = _wfopen(path, L"rb");
#else
    char path[4096];
    const char *locations[] = {"assets/fonts/IBMPlexMono-Regular.ttf",
                               "../Resources/fonts/IBMPlexMono-Regular.ttf",
                               "../assets/fonts/IBMPlexMono-Regular.ttf"};
    for (int i = 0; i < 3 && !file; ++i) {
        int length = snprintf(path, sizeof(path), "%s%s", directory, locations[i]);
        if (length > 0 && (size_t)length < sizeof(path)) file = fopen(path, "rb");
    }
#endif
    if (!file) return NULL;
    if (fseek(file, 0, SEEK_END) != 0) { fclose(file); return NULL; }
    long length_bytes = ftell(file);
    if (length_bytes <= 0 || length_bytes > 4 * 1024 * 1024 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return NULL;
    }
    unsigned char *data = malloc((size_t)length_bytes);
    if (!data) { fclose(file); return NULL; }
    size_t count = fread(data, 1, (size_t)length_bytes, file);
    fclose(file);
    if (count != (size_t)length_bytes) { free(data); return NULL; }
    *size = (int)count;
    return data;
}
