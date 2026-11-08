#include "learn.h"
#include "i18n.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static char feedback[256];

static void place_parts(PcbBoard *board)
{
    pcb_init(board);
    for (int i = 0; i < LEARN_PARTS; ++i) {
        const PcbComponent *c = &learn_targets[i];
        assert(pcb_add_component(board, c->type, c->position, c->rotation) >= 0);
    }
}

static void route(PcbBoard *board, PcbPoint a, PcbPoint b)
{
    assert(pcb_add_route(board, a, b, 0.4f));
}

static void complete_board(PcbBoard *board)
{
    place_parts(board);
    for (int n = 0; n < 2; ++n) {
        PcbPoint a, b;
        learn_net_ends(n, &a, &b);
        route(board, a, b);
    }
    PcbPoint a, b;
    learn_net_ends(2, &a, &b);
    route(board, a, (PcbPoint){48.26f, 35.56f});
    route(board, (PcbPoint){48.26f, 35.56f}, (PcbPoint){20.32f, 35.56f});
    route(board, (PcbPoint){20.32f, 35.56f}, (PcbPoint){20.32f, 21.59f});
    route(board, (PcbPoint){20.32f, 21.59f}, b);
}

int main(void)
{
    assert(strcmp(tr("Save"), "Save") == 0);
    i18n_set_language(LANGUAGE_INDONESIAN);
    assert(strcmp(tr("Save"), "Simpan") == 0);
    assert(strcmp(tr("An untranslated project path.pcb"), "An untranslated project path.pcb") == 0);
    assert(!learn_check(&(PcbBoard){0}, 1, feedback, sizeof(feedback)));
    assert(strstr(feedback, "Letakkan"));
    i18n_set_language(LANGUAGE_ENGLISH);
    PcbBoard board;
    pcb_init(&board);
    assert(!learn_check(&board, 1, feedback, sizeof(feedback)));
    place_parts(&board);
    for (int s = 1; s <= 3; ++s) assert(learn_check(&board, s, feedback, sizeof(feedback)));
    assert(!learn_check(&board, 4, feedback, sizeof(feedback)));
    assert(strstr(feedback, "+5 V"));
    board.components[2].rotation = 90;
    assert(!learn_check(&board, 3, feedback, sizeof(feedback)));
    board.components[2].rotation = 270;
    assert(pcb_add_component(&board, PCB_LED, (PcbPoint){70, 50}, 0) >= 0);
    assert(!learn_placed(&board, 2));

    complete_board(&board);
    for (int s = 1; s < LEARN_STEPS; ++s)
        assert(learn_check(&board, s, feedback, sizeof(feedback)));
    PcbBoard valid = board;
    /* Undo/delete or move must invalidate previously completed work. */
    assert(pcb_remove_trace(&board, board.trace_count - 1));
    assert(!learn_check(&board, 7, feedback, sizeof(feedback)));
    board = valid;
    board.components[0].position.x += PCB_GRID;
    assert(!learn_check(&board, 7, feedback, sizeof(feedback)));
    board = valid;
    assert(pcb_add_component(&board, PCB_CAPACITOR, (PcbPoint){70, 50}, 0) >= 0);
    assert(!learn_check(&board, 7, feedback, sizeof(feedback)));

    /* Mid-segment crossings must connect, even without shared endpoints. */
    board = valid;
    route(&board, (PcbPoint){25.4f, 8.89f}, (PcbPoint){25.4f, 40.64f});
    assert(!learn_check(&board, 7, feedback, sizeof(feedback)));
    assert(strstr(feedback, "Unintended"));
    /* The resistor is not a copper short between its two pads. */
    board = valid;
    route(&board, pcb_pad_position(&board.components[1], 0),
                  pcb_pad_position(&board.components[1], 1));
    assert(!learn_check(&board, 7, feedback, sizeof(feedback)));
    /* Unused header pads cannot join any intended net. */
    board = valid;
    route(&board, pcb_pad_position(&board.components[0], 1),
                  pcb_pad_position(&board.components[0], 2));
    assert(!learn_check(&board, 7, feedback, sizeof(feedback)));
    board = valid;
    route(&board, (PcbPoint){60, 40}, (PcbPoint){70, 40});
    assert(!learn_check(&board, 7, feedback, sizeof(feedback)));
    assert(strstr(feedback, "stray"));

    /* Edge contact (including square pad corners) counts as copper contact. */
    place_parts(&board);
    PcbPoint a, b;
    learn_net_ends(0, &a, &b);
    route(&board, (PcbPoint){a.x + 0.85f, a.y - 0.85f}, b);
    assert(learn_check(&board, 4, feedback, sizeof(feedback)));
    place_parts(&board);
    route(&board, (PcbPoint){a.x, a.y - 1.2f}, b);
    assert(!learn_check(&board, 4, feedback, sizeof(feedback)));
    /* Touching trace edges, not only centerlines, are connected. */
    place_parts(&board);
    route(&board, a, (PcbPoint){a.x, 10});
    route(&board, (PcbPoint){a.x + 0.3f, 10}, b);
    assert(learn_check(&board, 4, feedback, sizeof(feedback)));
    board.traces[board.trace_count - 2].a.x += 0.3f;
    assert(!learn_check(&board, 4, feedback, sizeof(feedback)));

    /* Full capacity and invalid lesson steps are bounded. */
    board = valid;
    while (board.trace_count < PCB_MAX_TRACES) {
        board.traces[board.trace_count] = board.traces[0];
        ++board.trace_count;
    }
    assert(learn_check(&board, 7, feedback, sizeof(feedback)));
    assert(!learn_check(&board, 0, feedback, sizeof(feedback)));
    assert(!learn_check(&board, LEARN_STEPS, feedback, sizeof(feedback)));
    puts("PCBEE learning tests passed.");
    return 0;
}
