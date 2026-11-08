/* Keep Windows headers separate from raylib's conflicting API names. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <stdlib.h>

int main(int argc, char **argv);

/* A GUI subsystem executable, with UTF-8 arguments even on non-UTF-8 systems. */
int WINAPI WinMain(HINSTANCE instance, HINSTANCE previous, LPSTR command, int show)
{
    (void)instance;
    (void)previous;
    (void)command;
    (void)show;
    int argc = 0;
    wchar_t **wide = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!wide) return 1;
    char **argv = calloc((size_t)argc + 1, sizeof(*argv));
    if (!argv) { LocalFree(wide); return 1; }
    int result = 1;
    for (int i = 0; i < argc; ++i) {
        int size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide[i], -1,
                                       NULL, 0, NULL, NULL);
        if (size <= 0) goto cleanup;
        argv[i] = malloc((size_t)size);
        if (!argv[i] || !WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide[i], -1,
                                            argv[i], size, NULL, NULL)) goto cleanup;
    }
    result = main(argc, argv);
cleanup:
    for (int i = 0; i < argc; ++i) free(argv[i]);
    free(argv);
    LocalFree(wide);
    return result;
}
