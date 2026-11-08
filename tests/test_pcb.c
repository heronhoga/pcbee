#include "pcb.h"

#include <assert.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

static void close_float(float actual, float expected)
{
    assert(fabsf(actual - expected) < 0.0001f);
}

static void test_placement(void)
{
    PcbBoard board;
    pcb_init(&board);
    close_float(board.width, 101.6f);
    assert(board.component_count == 0 && board.next_id == 1);
    PcbPoint snapped = pcb_snap((PcbPoint){10.1f, 20.5f});
    close_float(snapped.x, 10.16f);
    close_float(snapped.y, 20.32f);
    for (int type = 0; type < PCB_FOOTPRINT_COUNT; ++type) {
        for (int rotation = 0; rotation < 360; rotation += 90) {
            int index = pcb_add_component(&board, (PcbFootprint)type, (PcbPoint){25.4f, 25.4f}, rotation);
            assert(index >= 0 && board.components[index].id == index + 1);
            assert(pcb_component_fits(&board, &board.components[index]));
        }
    }
    PcbComponent resistor = {1, PCB_RESISTOR, {25.4f, 25.4f}, 90, "R1"};
    PcbPoint pad = pcb_pad_position(&resistor, 0);
    close_float(pad.x, 25.4f);
    close_float(pad.y, 21.59f);
    assert(pcb_add_component(&board, PCB_DIP8, (PcbPoint){1, 1}, 0) == -1);
    assert(pcb_add_component(&board, PCB_RESISTOR, (PcbPoint){NAN, 20}, 0) == -1);
    assert(pcb_add_component(&board, PCB_RESISTOR, (PcbPoint){20, 20}, 45) == -1);
    assert(pcb_add_component(&board, PCB_FOOTPRINT_COUNT, (PcbPoint){20, 20}, 0) == -1);
    assert(strcmp(board.components[0].name, "R1") == 0);
    assert(pcb_rename_component(&board, 0, "Power_5V"));
    assert(!pcb_rename_component(&board, 0, ""));
    assert(!pcb_rename_component(&board, 0, "too many characters for label"));
    assert(!pcb_rename_component(&board, 0, "A B"));
    assert(!pcb_rename_component(&board, 0, "bad/label"));
    assert(!pcb_rename_component(&board, 1, "power_5v"));
    assert(!pcb_rename_component(&board, -1, "OK"));
    assert(strcmp(board.components[0].name, "Power_5V") == 0);
    assert(pcb_rename_component(&board, 0, "R1"));
    int count = board.component_count;
    int id = board.components[1].id;
    assert(pcb_remove_component(&board, 0));
    assert(board.component_count == count - 1 && board.components[0].id == id);
    assert(!pcb_remove_component(&board, -1));
    assert(!pcb_remove_component(&board, board.component_count));
    while (board.component_count < PCB_MAX_COMPONENTS)
        assert(pcb_add_component(&board, PCB_LED, (PcbPoint){20, 20}, 0) >= 0);
    assert(pcb_add_component(&board, PCB_LED, (PcbPoint){20, 20}, 0) == -1);
    pcb_init(&board);
    assert(pcb_add_component(&board, PCB_RESISTOR, (PcbPoint){25.4f, 25.4f}, 0) == 0);
    assert(pcb_rename_component(&board, 0, "R2"));
    assert(pcb_add_component(&board, PCB_RESISTOR, (PcbPoint){40.64f, 25.4f}, 0) == 1);
    assert(strcmp(board.components[1].name, "R3") == 0);
    pcb_init(&board);
    board.next_id = INT_MAX;
    assert(pcb_add_component(&board, PCB_LED, (PcbPoint){20, 20}, 0) == -1);
}

static void test_routing(void)
{
    PcbBoard board;
    pcb_init(&board);
    assert(pcb_add_route(&board, (PcbPoint){10, 10}, (PcbPoint){30, 20}, 0.6f));
    assert(board.trace_count == 2);
    close_float(board.traces[0].b.x, 20);
    close_float(board.traces[0].b.y, 20);
    close_float(pcb_distance(board.traces[0].b, board.traces[1].a), 0);
    assert(pcb_add_route(&board, (PcbPoint){30, 20}, (PcbPoint){30, 40}, 0.4f));
    assert(board.trace_count == 3);
    assert(pcb_add_route(&board, (PcbPoint){30, 40}, (PcbPoint){10, 20}, 1));
    assert(board.trace_count == 4);
    assert(!pcb_add_route(&board, (PcbPoint){10, 10}, (PcbPoint){10, 10}, 0.6f));
    assert(!pcb_add_route(&board, (PcbPoint){-1, 10}, (PcbPoint){10, 10}, 0.6f));
    assert(!pcb_add_route(&board, (PcbPoint){10, 10}, (PcbPoint){20, 10}, INFINITY));
    assert(!pcb_add_route(&board, (PcbPoint){10, 10}, (PcbPoint){20, 10}, 0));
    assert(pcb_remove_trace(&board, 0));
    assert(board.trace_count == 3);
    close_float(board.traces[0].a.x, 20);
    assert(!pcb_remove_trace(&board, 3));
    close_float(pcb_segment_distance((PcbPoint){5, 3}, (PcbPoint){0, 0}, (PcbPoint){10, 0}), 3);
    close_float(pcb_segment_distance((PcbPoint){-3, 4}, (PcbPoint){0, 0}, (PcbPoint){10, 0}), 5);
    close_float(pcb_segment_distance((PcbPoint){3, 4}, (PcbPoint){0, 0}, (PcbPoint){0, 0}), 5);
    while (board.trace_count < PCB_MAX_TRACES - 1)
        assert(pcb_add_route(&board, (PcbPoint){10, 10}, (PcbPoint){20, 10}, 0.6f));
    /* A two-segment route must fail atomically when only one slot remains. */
    assert(!pcb_add_route(&board, (PcbPoint){10, 10}, (PcbPoint){30, 20}, 0.6f));
    assert(board.trace_count == PCB_MAX_TRACES - 1);
    assert(pcb_add_route(&board, (PcbPoint){10, 10}, (PcbPoint){20, 10}, 0.6f));
    assert(!pcb_add_route(&board, (PcbPoint){10, 10}, (PcbPoint){20, 10}, 0.6f));
}

static void test_history(void)
{
    PcbBoard board;
    static PcbHistory history;
    pcb_init(&board);
    pcb_history_init(&history, &board);
    assert(!pcb_undo(&history, &board));
    assert(!pcb_redo(&history, &board));
    for (int i = 0; i < 50; ++i) {
        assert(pcb_add_component(&board, PCB_LED, (PcbPoint){20, 20}, 0) >= 0);
        pcb_history_push(&history, &board);
    }
    int undos = 0;
    while (pcb_undo(&history, &board)) ++undos;
    assert(undos == PCB_HISTORY_SIZE - 1);
    assert(board.component_count == 50 - undos);
    int redos = 0;
    while (pcb_redo(&history, &board)) ++redos;
    assert(redos == undos && board.component_count == 50);
    assert(pcb_undo(&history, &board));
    assert(pcb_undo(&history, &board));
    assert(pcb_remove_component(&board, 0));
    pcb_history_push(&history, &board);
    assert(board.component_count == 47);
    assert(!pcb_redo(&history, &board));
    assert(pcb_undo(&history, &board) && board.component_count == 48);
    assert(pcb_redo(&history, &board) && board.component_count == 47);
}

static void test_name_history(void)
{
    PcbBoard board;
    static PcbHistory history;
    pcb_init(&board);
    assert(pcb_add_component(&board, PCB_LED, (PcbPoint){25.4f, 25.4f}, 0) == 0);
    pcb_history_init(&history, &board);
    assert(pcb_rename_component(&board, 0, "LED_PWR"));
    pcb_history_push(&history, &board);
    assert(pcb_rename_component(&board, 0, "D"));
    pcb_history_push(&history, &board);
    assert(pcb_undo(&history, &board) && strcmp(board.components[0].name, "LED_PWR") == 0);
    assert(pcb_undo(&history, &board) && strcmp(board.components[0].name, "D1") == 0);
    assert(pcb_redo(&history, &board) && strcmp(board.components[0].name, "LED_PWR") == 0);
    assert(pcb_redo(&history, &board) && strcmp(board.components[0].name, "D") == 0);
    char error[256];
    assert(pcb_save(&board, "build/test-named.pcb", error, sizeof(error)));
    PcbBoard loaded;
    assert(pcb_load(&loaded, "build/test-named.pcb", error, sizeof(error)));
    assert(memcmp(&board, &loaded, sizeof(board)) == 0);
    assert(remove("build/test-named.pcb") == 0);
}

static void write_fixture(const char *content)
{
    FILE *file = fopen("build/test-invalid.pcb", "w");
    assert(file);
    assert(fputs(content, file) >= 0);
    assert(fclose(file) == 0);
}

static void test_persistence(void)
{
    PcbBoard board, loaded;
    char error[256];
    pcb_init(&board);
    assert(pcb_load(&board, "examples/demo.pcb", error, sizeof(error)));
    assert(board.component_count == 5 && board.trace_count == 8);
    assert(strcmp(board.components[0].name, "J1") == 0); /* v1 fallback */
    assert(pcb_rename_component(&board, 0, "Input_R"));
    assert(pcb_save(&board, "build/test-roundtrip.pcb", error, sizeof(error)));
    pcb_init(&loaded);
    assert(pcb_load(&loaded, "build/test-roundtrip.pcb", error, sizeof(error)));
    assert(memcmp(&board, &loaded, sizeof(board)) == 0);
    /* Replacing an existing file must also round-trip exactly. */
    assert(pcb_remove_component(&board, 0));
    assert(pcb_save(&board, "build/test-roundtrip.pcb", error, sizeof(error)));
    assert(pcb_load(&loaded, "build/test-roundtrip.pcb", error, sizeof(error)));
    assert(loaded.component_count == 4 && loaded.components[0].id == 2);
    PcbBoard before = loaded;
    const char *invalid[] = {
        "", "PCBEE 3\n", "PCBEE 2\n", "PCBEE 1\nBOARD nan 76.2 1\n",
        "PCBEE 2\nBOARD 101.6 76.2 3\nCOMPONENTS 1\n1 0 20 20 0 bad/name\nTRACES 0\nEND\n",
        "PCBEE 2\nBOARD 101.6 76.2 3\nCOMPONENTS 1\n1 0 20 20 0 VeryLongComponentName\nTRACES 0\nEND\n",
        "PCBEE 2\nBOARD 101.6 76.2 3\nCOMPONENTS 2\n1 0 20 20 0 input\n2 1 40 40 0 INPUT\nTRACES 0\nEND\n",
        "PCBEE 1\nBOARD 101.6 inf 1\n",
        "PCBEE 1\nBOARD 101.6 76.2 9999999999999999999999999\n",
        "PCBEE 1\nBOARD 101.6 76.2 1\nCOMPONENTS 129\n",
        "PCBEE 1\nBOARD 101.6 76.2 1\nCOMPONENTS -1\n",
        "PCBEE 1\nBOARD 101.6 76.2 2\nCOMPONENTS 1\n1 99 20 20 0\nTRACES 0\nEND\n",
        "PCBEE 1\nBOARD 101.6 76.2 2\nCOMPONENTS 1\n1 0 20 20 45\nTRACES 0\nEND\n",
        "PCBEE 1\nBOARD 101.6 76.2 2\nCOMPONENTS 1\n1 0 0 0 0\nTRACES 0\nEND\n",
        "PCBEE 1\nBOARD 101.6 76.2 2\nCOMPONENTS 2\n1 0 20 20 0\n1 0 30 30 0\nTRACES 0\nEND\n",
        "PCBEE 1\nBOARD 101.6 76.2 1\nCOMPONENTS 1\n1 0 20 20 0\nTRACES 0\nEND\n",
        "PCBEE 1\nBOARD 101.6 76.2 1\nCOMPONENTS 0\nTRACES 1025\n",
        "PCBEE 1\nBOARD 101.6 76.2 1\nCOMPONENTS 0\nTRACES 1\n10 10 20 20 -1\nEND\n",
        "PCBEE 1\nBOARD 101.6 76.2 1\nCOMPONENTS 0\nTRACES 1\n10 10 200 20 0.6\nEND\n",
        "PCBEE 1\nBOARD 101.6 76.2 1\nCOMPONENTS 0\nTRACES 1\n10 10 10 10 0.6\nEND\n",
        "PCBEE 1\nBOARD 101.6 76.2 1\nCOMPONENTS 0\nTRACES 1\n10 10 20\n",
        "PCBEE 1\nBOARD 101.6 76.2 1\nCOMPONENTS 0\nTRACES 0\nEND\nJUNK\n"
    };
    for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
        write_fixture(invalid[i]);
        assert(!pcb_load(&loaded, "build/test-invalid.pcb", error, sizeof(error)));
        assert(error[0]);
        assert(memcmp(&before, &loaded, sizeof(loaded)) == 0);
    }
    assert(!pcb_load(&loaded, "build/does-not-exist.pcb", error, sizeof(error)));
    assert(memcmp(&before, &loaded, sizeof(loaded)) == 0);
    assert(!pcb_save(&board, "build/no-such-directory/board.pcb", error, sizeof(error)));
    assert(error[0]);
    /* Replacement failure must be reported, and its temporary file removed. */
    assert(!pcb_save(&board, "build", error, sizeof(error)));
    FILE *temporary = fopen("build.tmp", "r");
    assert(!temporary);
    assert(error[0]);
    pcb_init(&board);
    assert(pcb_save(&board, "build/test-roundtrip.pcb", error, sizeof(error)));
    assert(pcb_load(&loaded, "build/test-roundtrip.pcb", error, sizeof(error)));
    assert(memcmp(&board, &loaded, sizeof(board)) == 0);
    remove("build/test-roundtrip.pcb");
    remove("build/test-invalid.pcb");
}

static void test_platform_paths(void)
{
    /* UTF-8 bytes, independent of the compiler's source/execution encoding. */
    const char *path = "build/test-\xc3\xa9-\xe6\x9d\xbf space.pcb";
    PcbBoard board, loaded;
    char error[256];
    pcb_init(&board);
    assert(pcb_save(&board, path, error, sizeof(error)));
    assert(pcb_add_component(&board, PCB_LED, (PcbPoint){20, 20}, 0) == 0);
    assert(pcb_save(&board, path, error, sizeof(error)));
    assert(pcb_load(&loaded, path, error, sizeof(error)));
    assert(memcmp(&board, &loaded, sizeof(board)) == 0);
#ifdef _WIN32
    /* A sharing violation must leave the previous project readable/intact. */
    const wchar_t *wide = L"build/test-\u00e9-\u677f space.pcb";
    HANDLE lock = CreateFileW(wide, GENERIC_READ, FILE_SHARE_READ, NULL,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    assert(lock != INVALID_HANDLE_VALUE);
    pcb_init(&board);
    assert(!pcb_save(&board, path, error, sizeof(error)));
    assert(error[0]);
    assert(CloseHandle(lock));
    assert(pcb_load(&board, path, error, sizeof(error)));
    assert(memcmp(&board, &loaded, sizeof(board)) == 0);
    assert(DeleteFileW(wide));
#else
    assert(remove(path) == 0);
#endif
}

int main(void)
{
    test_placement();
    test_routing();
    test_history();
    test_name_history();
    test_persistence();
    test_platform_paths();
    printf("All PCB core tests passed. Board: %zu bytes; bounded history: %zu bytes.\n",
           sizeof(PcbBoard), sizeof(PcbHistory));
    return 0;
}
