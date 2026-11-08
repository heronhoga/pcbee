#include "export.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

static void read_file(const char *name, char *text, size_t capacity)
{
#ifdef _WIN32
    wchar_t wide[4096];
    assert(MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, name, -1, wide, 4096));
    FILE *file = _wfopen(wide, L"rb");
#else
    FILE *file = fopen(name, "rb");
#endif
    assert(file);
    size_t n = fread(text, 1, capacity - 1, file);
    assert(n < capacity - 1 && !ferror(file));
    text[n] = '\0';
    assert(fclose(file) == 0);
}

static int occurrences(const char *text, const char *needle)
{
    int count = 0;
    for (const char *p = text; (p = strstr(p, needle)) != NULL; p += strlen(needle)) ++count;
    return count;
}

int main(void)
{
    PcbBoard board;
    static char text[160000];
    char error[256];
    pcb_init(&board);
    int resistor = pcb_add_component(&board, PCB_RESISTOR, (PcbPoint){25.4f, 25.4f}, 0);
    assert(resistor == 0 && pcb_rename_component(&board, resistor, "INPUT_R"));
    assert(pcb_add_component(&board, PCB_HEADER, (PcbPoint){12.7f, 25.4f}, 90) == 1);
    PcbPoint a = pcb_pad_position(&board.components[0], 0);
    assert(pcb_add_route(&board, a, (PcbPoint){a.x, 30.48f}, 0.4f));
    assert(pcb_export_gerber(&board, "build/test-fab", error, sizeof(error)));
    assert(!error[0]);

    read_file("build/test-fab-F_Cu.gbr", text, sizeof(text));
    assert(strstr(text, "%FSLAX46Y46*%\n%MOMM*%"));
    assert(strstr(text, "%TF.FileFunction,Copper,L1,Top*%"));
    assert(strstr(text, "%ADD11R,1.800X1.800*%"));
    assert(strstr(text, "%ADD20C,0.400000*%"));
    assert(strstr(text, "X0021590000Y0050800000D03*"));
    assert(strstr(text, "X0021590000Y0050800000D02*"));
    assert(strstr(text, "X0021590000Y0045720000D01*"));
    assert(strstr(text, "M02*\n"));

    read_file("build/test-fab-B_Cu.gbr", text, sizeof(text));
    assert(strstr(text, "%TF.FileFunction,Copper,L2,Bot*%"));
    assert(occurrences(text, "D03*\n") == 6); /* Six plated pads, but no bottom routing. */
    assert(!strstr(text, "D01*"));
    read_file("build/test-fab-F_Mask.gbr", text, sizeof(text));
    assert(strstr(text, "%ADD10C,1.900*%"));
    assert(strstr(text, "%ADD11R,1.900X1.900*%"));
    assert(occurrences(text, "D03*\n") == 6);
    read_file("build/test-fab-B_Mask.gbr", text, sizeof(text));
    assert(occurrences(text, "D03*\n") == 6);
    read_file("build/test-fab-F_SilkS.gbr", text, sizeof(text));
    assert(strstr(text, "%TF.FileFunction,Legend,Top*%"));
    assert(occurrences(text, "D03*\n") > 20); /* Printable labels, not just comments. */
    read_file("build/test-fab-Edge_Cuts.gbr", text, sizeof(text));
    assert(strstr(text, "%TF.FileFunction,Profile,NP*%"));
    assert(strstr(text, "X0101600000Y0000000000D01*"));
    assert(strstr(text, "X0000000000Y0076200000D01*"));
    assert(occurrences(text, "D01*\n") == 4); /* Closed rectangular outline. */
    read_file("build/test-fab-PTH.drl", text, sizeof(text));
    assert(strstr(text, "METRIC,TZ\nT01C0.800\n%\nG90\nT01\n"));
    assert(strstr(text, "X21.590Y50.800\n"));
    assert(occurrences(text, "\nX") == 6);
    assert(strstr(text, "M30\n"));

    const char *unicode_prefix = "build/test-\xc3\xa9-\xe6\x9d\xbf fab";
    assert(pcb_export_gerber(&board, unicode_prefix, error, sizeof(error)));
    char unicode_path[128];
    snprintf(unicode_path, sizeof(unicode_path), "%s-PTH.drl", unicode_prefix);
    read_file(unicode_path, text, sizeof(text));
    assert(occurrences(text, "\nX") == 6);

    /* Re-export replaces an existing set; rejected geometry does not touch it. */
    assert(pcb_remove_trace(&board, 0));
    assert(pcb_export_gerber(&board, "build/test-fab", error, sizeof(error)));
    read_file("build/test-fab-F_Cu.gbr", text, sizeof(text));
    assert(!strstr(text, "%ADD20C"));
    board.traces[board.trace_count++] = (PcbTrace){{0, 20}, {10, 20}, 0.4f};
    assert(!pcb_export_gerber(&board, "build/test-fab", error, sizeof(error)));
    assert(error[0]);
    read_file("build/test-fab-F_Cu.gbr", text, sizeof(text));
    assert(!strstr(text, "%ADD20C"));
    assert(!pcb_export_gerber(&board, "build/no-such-dir/test", error, sizeof(error)));
    board.trace_count = 0;
    assert(!pcb_export_gerber(&board, "build/no-such-dir/test", error, sizeof(error)));
    assert(!pcb_export_gerber(&board, "", error, sizeof(error)));

    const char *files[] = {"F_Cu.gbr", "B_Cu.gbr", "F_Mask.gbr", "B_Mask.gbr",
                           "F_SilkS.gbr", "Edge_Cuts.gbr", "PTH.drl"};
    for (int i = 0; i < 7; ++i) {
        char path[128];
        snprintf(path, sizeof(path), "build/test-fab-%s", files[i]);
        assert(remove(path) == 0);
        snprintf(path, sizeof(path), "build/test-fab-%s.tmp", files[i]);
        FILE *tmp = fopen(path, "rb");
        assert(!tmp);
        snprintf(path, sizeof(path), "%s-%s", unicode_prefix, files[i]);
#ifdef _WIN32
        /* Windows UTF-8 file operations require the native wide-path APIs. */
        wchar_t wide[128];
        assert(MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, wide, 128));
        assert(DeleteFileW(wide));
#else
        assert(remove(path) == 0);
#endif
    }
    puts("Gerber and drill export tests passed.");
    return 0;
}
