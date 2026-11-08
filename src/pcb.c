#include "pcb.h"

#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const PcbFootprintInfo footprints[PCB_FOOTPRINT_COUNT] = {
    { PCB_RESISTOR, "Resistor", "R", "Axial / 7.62 mm pitch", 5.08f, 2.54f, 2,
      {{-3.81f, 0}, {3.81f, 0}} },
    { PCB_CAPACITOR, "Capacitor", "C", "Radial / 2.54 mm pitch", 5.08f, 5.08f, 2,
      {{-1.27f, 0}, {1.27f, 0}} },
    { PCB_LED, "LED", "D", "3 mm / 2.54 mm pitch", 3.81f, 3.81f, 2,
      {{-1.27f, 0}, {1.27f, 0}} },
    { PCB_HEADER, "Pin header", "J", "1 x 4 / 2.54 mm pitch", 10.16f, 2.54f, 4,
      {{-3.81f, 0}, {-1.27f, 0}, {1.27f, 0}, {3.81f, 0}} },
    { PCB_DIP8, "DIP-8", "U", "Through-hole / 7.62 mm", 5.08f, 10.16f, 8,
      {{-3.81f, -3.81f}, {-3.81f, -1.27f}, {-3.81f, 1.27f}, {-3.81f, 3.81f},
       {3.81f, 3.81f}, {3.81f, 1.27f}, {3.81f, -1.27f}, {3.81f, -3.81f}} }
};

static bool finite_point(PcbPoint point)
{
    return isfinite(point.x) && isfinite(point.y);
}

const PcbFootprintInfo *pcb_footprint(PcbFootprint type)
{
    return type >= 0 && type < PCB_FOOTPRINT_COUNT ? &footprints[type] : NULL;
}

void pcb_init(PcbBoard *board)
{
    memset(board, 0, sizeof(*board));
    board->width = 101.6f;
    board->height = 76.2f;
    board->next_id = 1;
}

PcbPoint pcb_rotate(PcbPoint p, int rotation)
{
    switch ((rotation % 360 + 360) % 360) {
        case 90: return (PcbPoint){-p.y, p.x};
        case 180: return (PcbPoint){-p.x, -p.y};
        case 270: return (PcbPoint){p.y, -p.x};
        default: return p;
    }
}

PcbPoint pcb_pad_position(const PcbComponent *component, int pad)
{
    const PcbFootprintInfo *info = pcb_footprint(component->type);
    if (!info || pad < 0 || pad >= info->pad_count) return component->position;
    PcbPoint p = pcb_rotate(info->pads[pad], component->rotation);
    return (PcbPoint){p.x + component->position.x, p.y + component->position.y};
}

PcbPoint pcb_snap(PcbPoint p)
{
    return (PcbPoint){roundf(p.x / PCB_GRID) * PCB_GRID, roundf(p.y / PCB_GRID) * PCB_GRID};
}

bool pcb_contains(const PcbBoard *board, PcbPoint p)
{
    return finite_point(p) && p.x >= 0 && p.y >= 0 &&
           p.x <= board->width && p.y <= board->height;
}

bool pcb_component_fits(const PcbBoard *board, const PcbComponent *component)
{
    const PcbFootprintInfo *info = pcb_footprint(component->type);
    if (!info || !finite_point(component->position) || component->rotation < 0 ||
        component->rotation >= 360 || component->rotation % 90 != 0) return false;
    for (int x = -1; x <= 1; x += 2) {
        for (int y = -1; y <= 1; y += 2) {
            PcbPoint corner = pcb_rotate((PcbPoint){x * info->body_width / 2,
                y * info->body_height / 2}, component->rotation);
            corner.x += component->position.x;
            corner.y += component->position.y;
            if (!pcb_contains(board, corner)) return false;
        }
    }
    for (int i = 0; i < info->pad_count; ++i) {
        PcbPoint p = pcb_pad_position(component, i);
        if (!pcb_contains(board, (PcbPoint){p.x - 0.9f, p.y - 0.9f}) ||
            !pcb_contains(board, (PcbPoint){p.x + 0.9f, p.y + 0.9f})) return false;
    }
    return true;
}

int pcb_add_component(PcbBoard *board, PcbFootprint type, PcbPoint position, int rotation)
{
    PcbComponent component = {board->next_id, type, position, rotation};
    if (board->component_count >= PCB_MAX_COMPONENTS || board->next_id == INT_MAX ||
        !pcb_component_fits(board, &component)) return -1;
    int index = board->component_count++;
    board->components[index] = component;
    board->next_id++;
    return index;
}

bool pcb_remove_component(PcbBoard *board, int index)
{
    if (index < 0 || index >= board->component_count) return false;
    memmove(&board->components[index], &board->components[index + 1],
            (size_t)(board->component_count - index - 1) * sizeof(PcbComponent));
    board->component_count--;
    return true;
}

bool pcb_remove_trace(PcbBoard *board, int index)
{
    if (index < 0 || index >= board->trace_count) return false;
    memmove(&board->traces[index], &board->traces[index + 1],
            (size_t)(board->trace_count - index - 1) * sizeof(PcbTrace));
    board->trace_count--;
    return true;
}

float pcb_distance(PcbPoint a, PcbPoint b)
{
    return hypotf(a.x - b.x, a.y - b.y);
}

float pcb_segment_distance(PcbPoint p, PcbPoint a, PcbPoint b)
{
    float dx = b.x - a.x, dy = b.y - a.y;
    float length2 = dx * dx + dy * dy;
    if (length2 < 0.000001f) return pcb_distance(p, a);
    float t = ((p.x - a.x) * dx + (p.y - a.y) * dy) / length2;
    t = fmaxf(0, fminf(1, t));
    return pcb_distance(p, (PcbPoint){a.x + t * dx, a.y + t * dy});
}

PcbPoint pcb_route_corner(PcbPoint a, PcbPoint b)
{
    float dx = b.x - a.x, dy = b.y - a.y;
    float diagonal = fminf(fabsf(dx), fabsf(dy));
    return (PcbPoint){a.x + copysignf(diagonal, dx), a.y + copysignf(diagonal, dy)};
}

bool pcb_add_route(PcbBoard *board, PcbPoint a, PcbPoint b, float width)
{
    if (!pcb_contains(board, a) || !pcb_contains(board, b) || !isfinite(width) ||
        width < 0.1f || width > 5.0f || pcb_distance(a, b) < 0.001f) return false;
    PcbPoint corner = pcb_route_corner(a, b);
    bool first = pcb_distance(a, corner) > 0.001f;
    bool second = pcb_distance(corner, b) > 0.001f;
    if (board->trace_count + first + second > PCB_MAX_TRACES) return false;
    if (first) board->traces[board->trace_count++] = (PcbTrace){a, corner, width};
    if (second) board->traces[board->trace_count++] = (PcbTrace){corner, b, width};
    return true;
}

void pcb_history_init(PcbHistory *history, const PcbBoard *board)
{
    history->start = 0;
    history->count = 1;
    history->cursor = 0;
    history->states[0] = *board;
}

void pcb_history_push(PcbHistory *history, const PcbBoard *board)
{
    history->count = history->cursor + 1;
    if (history->count == PCB_HISTORY_SIZE) {
        history->start = (history->start + 1) % PCB_HISTORY_SIZE;
        history->count--;
    }
    history->states[(history->start + history->count) % PCB_HISTORY_SIZE] = *board;
    history->count++;
    history->cursor = history->count - 1;
}

bool pcb_undo(PcbHistory *history, PcbBoard *board)
{
    if (history->cursor == 0) return false;
    history->cursor--;
    *board = history->states[(history->start + history->cursor) % PCB_HISTORY_SIZE];
    return true;
}

bool pcb_redo(PcbHistory *history, PcbBoard *board)
{
    if (history->cursor + 1 >= history->count) return false;
    history->cursor++;
    *board = history->states[(history->start + history->cursor) % PCB_HISTORY_SIZE];
    return true;
}

static bool fail(char *error, size_t size, const char *message)
{
    if (error && size) snprintf(error, size, "%s", message);
    return false;
}

bool pcb_save(const PcbBoard *board, const char *path, char *error, size_t error_size)
{
    char temporary[4096];
    if (snprintf(temporary, sizeof(temporary), "%s.tmp", path) >= (int)sizeof(temporary))
        return fail(error, error_size, "Project path is too long.");
    FILE *file = fopen(temporary, "w");
    if (!file) return fail(error, error_size, strerror(errno));
    bool ok = fprintf(file, "PCBEE 1\nBOARD %.9g %.9g %d\nCOMPONENTS %d\n",
        board->width, board->height, board->next_id, board->component_count) > 0;
    for (int i = 0; i < board->component_count && ok; ++i) {
        const PcbComponent *c = &board->components[i];
        ok = fprintf(file, "%d %d %.9g %.9g %d\n", c->id, (int)c->type,
                     c->position.x, c->position.y, c->rotation) > 0;
    }
    ok = ok && fprintf(file, "TRACES %d\n", board->trace_count) > 0;
    for (int i = 0; i < board->trace_count && ok; ++i) {
        const PcbTrace *t = &board->traces[i];
        ok = fprintf(file, "%.9g %.9g %.9g %.9g %.9g\n", t->a.x, t->a.y,
                     t->b.x, t->b.y, t->width) > 0;
    }
    ok = ok && fprintf(file, "END\n") > 0;
    if (fclose(file) != 0) ok = false;
    if (!ok) {
        remove(temporary);
        return fail(error, error_size, "Could not finish writing the project.");
    }
    if (rename(temporary, path) != 0) {
        int saved_errno = errno;
        remove(temporary);
        return fail(error, error_size, strerror(saved_errno));
    }
    if (error && error_size) error[0] = '\0';
    return true;
}

static bool read_word(FILE *file, const char *expected)
{
    char word[128];
    return fscanf(file, "%127s", word) == 1 && strcmp(word, expected) == 0;
}

static bool read_int(FILE *file, int *value)
{
    char word[128], *end;
    if (fscanf(file, "%127s", word) != 1) return false;
    errno = 0;
    long parsed = strtol(word, &end, 10);
    if (errno || end == word || *end || parsed < INT_MIN || parsed > INT_MAX) return false;
    *value = (int)parsed;
    return true;
}

static bool read_float(FILE *file, float *value)
{
    char word[128], *end;
    if (fscanf(file, "%127s", word) != 1) return false;
    errno = 0;
    float parsed = strtof(word, &end);
    if (errno || end == word || *end || !isfinite(parsed)) return false;
    *value = parsed;
    return true;
}

bool pcb_load(PcbBoard *board, const char *path, char *error, size_t error_size)
{
    FILE *file = fopen(path, "r");
    if (!file) return fail(error, error_size, strerror(errno));
    PcbBoard loaded;
    pcb_init(&loaded);
    int version;
    bool ok = read_word(file, "PCBEE") && read_int(file, &version) && version == 1;
    ok = ok && read_word(file, "BOARD") && read_float(file, &loaded.width) &&
         read_float(file, &loaded.height) && read_int(file, &loaded.next_id) &&
         loaded.width >= 10 && loaded.width <= 1000 &&
         loaded.height >= 10 && loaded.height <= 1000 && loaded.next_id > 0;
    ok = ok && read_word(file, "COMPONENTS") && read_int(file, &loaded.component_count) &&
         loaded.component_count >= 0 &&
         loaded.component_count <= PCB_MAX_COMPONENTS;
    for (int i = 0; ok && i < loaded.component_count; ++i) {
        PcbComponent *c = &loaded.components[i];
        int type;
        ok = read_int(file, &c->id) && read_int(file, &type) &&
             read_float(file, &c->position.x) && read_float(file, &c->position.y) &&
             read_int(file, &c->rotation);
        if (!ok) break;
        c->type = (PcbFootprint)type;
        ok = c->id > 0 && c->id < loaded.next_id && pcb_component_fits(&loaded, c);
        for (int j = 0; ok && j < i; ++j) ok = c->id != loaded.components[j].id;
    }
    ok = ok && read_word(file, "TRACES") && read_int(file, &loaded.trace_count) &&
         loaded.trace_count >= 0 &&
         loaded.trace_count <= PCB_MAX_TRACES;
    for (int i = 0; ok && i < loaded.trace_count; ++i) {
        PcbTrace *t = &loaded.traces[i];
        ok = read_float(file, &t->a.x) && read_float(file, &t->a.y) &&
             read_float(file, &t->b.x) && read_float(file, &t->b.y) &&
             read_float(file, &t->width) && pcb_contains(&loaded, t->a) &&
             pcb_contains(&loaded, t->b) && isfinite(t->width) &&
             t->width >= 0.1f && t->width <= 5 && pcb_distance(t->a, t->b) > 0.001f;
    }
    ok = ok && read_word(file, "END");
    char extra[2];
    ok = ok && fscanf(file, "%1s", extra) == EOF && !ferror(file);
    fclose(file);
    if (!ok) return fail(error, error_size, "Invalid or unsupported PCBEE project.");
    *board = loaded;
    if (error && error_size) error[0] = '\0';
    return true;
}
