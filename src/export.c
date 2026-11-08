#include "export.h"

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
static bool wide_name(const char *name, wchar_t *wide, int count)
{
    return MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, name, -1, wide, count) != 0;
}
static FILE *open_file(const char *name)
{
    wchar_t wide[4096];
    return wide_name(name, wide, 4096) ? _wfopen(wide, L"wb") : NULL;
}
static void delete_file(const char *name)
{
    wchar_t wide[4096];
    if (wide_name(name, wide, 4096)) DeleteFileW(wide);
}
static bool replace_file(const char *from, const char *to)
{
    wchar_t a[4096], b[4096];
    return wide_name(from, a, 4096) && wide_name(to, b, 4096) &&
           MoveFileExW(a, b, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
}
#else
static FILE *open_file(const char *name) { return fopen(name, "wb"); }
static void delete_file(const char *name) { remove(name); }
static bool replace_file(const char *from, const char *to) { return rename(from, to) == 0; }
#endif

static bool fail(char *error, size_t size, const char *message)
{
    if (error && size) snprintf(error, size, "%s", message);
    return false;
}

/* Quantize model floats to 1 um so binary float error in 76.2 - y does not
   turn an intended 50.800 mm coordinate into 50.799996 mm. */
static long coordinate(float mm) { return lround((double)mm * 1000.0) * 1000L; }

static void point(FILE *file, const PcbBoard *board, PcbPoint p, const char *operation)
{
    fprintf(file, "X%010ldY%010ld%s*\n", coordinate(p.x), coordinate(board->height - p.y), operation);
}

static void header(FILE *file, const char *function)
{
    fprintf(file, "G04 PCBEE RS-274X, millimeters, top-view coordinates*\n"
                  "%%FSLAX46Y46*%%\n%%MOMM*%%\n%%TF.FileFunction,%s*%%\n%%LPD*%%\n", function);
}

static void pads(FILE *file, const PcbBoard *board)
{
    for (int i = 0; i < board->component_count; ++i) {
        const PcbComponent *c = &board->components[i];
        int count = pcb_footprint(c->type)->pad_count;
        for (int p = 0; p < count; ++p) {
            fprintf(file, "D%d*\n", p == 0 ? 11 : 10);
            point(file, board, pcb_pad_position(c, p), "D03");
        }
    }
}

static void copper_layer(FILE *file, const PcbBoard *board, bool top)
{
    header(file, top ? "Copper,L1,Top" : "Copper,L2,Bot");
    fprintf(file, "%%ADD10C,1.800*%%\n%%ADD11R,1.800X1.800*%%\n");
    if (top) {
        for (int i = 0; i < board->trace_count; ++i)
            fprintf(file, "%%ADD%dC,%.6f*%%\n", 20 + i, board->traces[i].width);
    }
    pads(file, board);
    if (top) for (int i = 0; i < board->trace_count; ++i) {
        fprintf(file, "D%d*\n", 20 + i);
        point(file, board, board->traces[i].a, "D02");
        point(file, board, board->traces[i].b, "D01");
    }
    fputs("M02*\n", file);
}

static void mask_layer(FILE *file, const PcbBoard *board, bool top)
{
    header(file, top ? "Soldermask,Top" : "Soldermask,Bot");
    fputs("G04 Positive image: flashes are mask openings*\n"
          "%ADD10C,1.900*%\n%ADD11R,1.900X1.900*%\n", file);
    pads(file, board);
    fputs("M02*\n", file);
}

static void outline_layer(FILE *file, const PcbBoard *board)
{
    header(file, "Profile,NP");
    fputs("%ADD10C,0.100*%\nD10*\n", file);
    point(file, board, (PcbPoint){0, 0}, "D02");
    point(file, board, (PcbPoint){board->width, 0}, "D01");
    point(file, board, (PcbPoint){board->width, board->height}, "D01");
    point(file, board, (PcbPoint){0, board->height}, "D01");
    point(file, board, (PcbPoint){0, 0}, "D01");
    fputs("M02*\n", file);
}

/* Small 5x7 stroke-by-dot font for printable reference labels. */
static const struct { char letter; unsigned char rows[7]; } glyphs[] = {
    {'A',{14,17,17,31,17,17,17}}, {'B',{30,17,17,30,17,17,30}},
    {'C',{14,17,16,16,16,17,14}}, {'D',{30,17,17,17,17,17,30}},
    {'E',{31,16,16,30,16,16,31}}, {'F',{31,16,16,30,16,16,16}},
    {'G',{14,17,16,23,17,17,14}}, {'H',{17,17,17,31,17,17,17}},
    {'I',{31,4,4,4,4,4,31}}, {'J',{7,2,2,2,18,18,12}},
    {'K',{17,18,20,24,20,18,17}}, {'L',{16,16,16,16,16,16,31}},
    {'M',{17,27,21,21,17,17,17}}, {'N',{17,25,21,19,17,17,17}},
    {'O',{14,17,17,17,17,17,14}}, {'P',{30,17,17,30,16,16,16}},
    {'Q',{14,17,17,17,21,18,13}}, {'R',{30,17,17,30,20,18,17}},
    {'S',{15,16,16,14,1,1,30}}, {'T',{31,4,4,4,4,4,4}},
    {'U',{17,17,17,17,17,17,14}}, {'V',{17,17,17,17,17,10,4}},
    {'W',{17,17,17,21,21,21,10}}, {'X',{17,17,10,4,10,17,17}},
    {'Y',{17,17,10,4,4,4,4}}, {'Z',{31,1,2,4,8,16,31}},
    {'0',{14,17,19,21,25,17,14}}, {'1',{4,12,4,4,4,4,14}},
    {'2',{14,17,1,2,4,8,31}}, {'3',{30,1,1,14,1,1,30}},
    {'4',{2,6,10,18,31,2,2}}, {'5',{31,16,16,30,1,1,30}},
    {'6',{14,16,16,30,17,17,14}}, {'7',{31,1,2,4,8,8,8}},
    {'8',{14,17,17,14,17,17,14}}, {'9',{14,17,17,15,1,1,14}},
    {'-',{0,0,0,31,0,0,0}}, {'_',{0,0,0,0,0,0,31}}
};

static bool near_open_pad(const PcbBoard *board, PcbPoint p)
{
    for (int i = 0; i < board->component_count; ++i) {
        const PcbComponent *c = &board->components[i];
        for (int j = 0; j < pcb_footprint(c->type)->pad_count; ++j) {
            PcbPoint pad = pcb_pad_position(c, j);
            if (j == 0 ? (fabsf(p.x - pad.x) < 1.22f && fabsf(p.y - pad.y) < 1.22f) :
                         pcb_distance(p, pad) < 1.22f) return true;
        }
    }
    return false;
}

static void silk_layer(FILE *file, const PcbBoard *board)
{
    header(file, "Legend,Top");
    fputs("%ADD10C,0.180*%\nD10*\n", file);
    for (int i = 0; i < board->component_count; ++i) {
        const PcbComponent *c = &board->components[i];
        const PcbFootprintInfo *info = pcb_footprint(c->type);
        size_t len = strlen(c->name);
        float width = (float)len * 1.38f;
        if (width > board->width - 0.4f) continue;
        float x = fmaxf(0.2f, fminf(board->width - width - 0.2f, c->position.x - width / 2));
        float radius = (c->rotation % 180 ? info->body_width : info->body_height) / 2;
        float y = c->position.y - radius - 2.1f;
        if (y < 0.2f) y = c->position.y + radius + 0.6f;
        if (y + 1.6f > board->height - 0.2f) continue;
        for (size_t n = 0; n < len; ++n) {
            char letter = (char)toupper((unsigned char)c->name[n]);
            for (size_t g = 0; g < sizeof(glyphs) / sizeof(glyphs[0]); ++g) {
                if (glyphs[g].letter != letter) continue;
                for (int row = 0; row < 7; ++row) for (int col = 0; col < 5; ++col) {
                    if (!(glyphs[g].rows[row] & (1 << (4 - col)))) continue;
                    PcbPoint dot = {x + (float)n * 1.38f + (float)col * 0.22f,
                                    y + (float)row * 0.22f};
                    if (!near_open_pad(board, dot)) point(file, board, dot, "D03");
                }
                break;
            }
        }
    }
    fputs("M02*\n", file);
}

static void drill_file(FILE *file, const PcbBoard *board)
{
    fputs("M48\n;PCBEE plated through-holes (mm, top view)\nFMAT,2\nMETRIC,TZ\nT01C0.800\n%\nG90\nT01\n", file);
    for (int i = 0; i < board->component_count; ++i) {
        const PcbComponent *c = &board->components[i];
        for (int p = 0; p < pcb_footprint(c->type)->pad_count; ++p) {
            PcbPoint hole = pcb_pad_position(c, p);
            fprintf(file, "X%.3fY%.3f\n", (double)coordinate(hole.x) / 1000000.0,
                    (double)coordinate(board->height - hole.y) / 1000000.0);
        }
    }
    fputs("M30\n", file);
}

static bool valid_board(const PcbBoard *board)
{
    if (!board || !isfinite(board->width) || !isfinite(board->height) || board->width < 10 ||
        board->height < 10 || board->width > 1000 || board->height > 1000 ||
        board->component_count < 0 || board->component_count > PCB_MAX_COMPONENTS ||
        board->trace_count < 0 || board->trace_count > PCB_MAX_TRACES) return false;
    for (int i = 0; i < board->component_count; ++i) {
        const PcbComponent *c = &board->components[i];
        if (!pcb_component_fits(board, c) || !c->name[0] ||
            !memchr(c->name, '\0', PCB_NAME_SIZE)) return false;
        for (int p = 0; p < pcb_footprint(c->type)->pad_count; ++p) {
            PcbPoint pad = pcb_pad_position(c, p);
            /* 1.9 mm mask opening plus at least 0.25 mm to the outline. */
            if (pad.x < 1.2f || pad.y < 1.2f ||
                pad.x > board->width - 1.2f || pad.y > board->height - 1.2f) return false;
        }
    }
    for (int i = 0; i < board->trace_count; ++i) {
        const PcbTrace *t = &board->traces[i];
        if (!pcb_contains(board, t->a) || !pcb_contains(board, t->b) ||
            !isfinite(t->width) || t->width < 0.1f || t->width > 5.0f) return false;
        float r = t->width / 2 + 0.25f;
        if (t->a.x < r || t->a.y < r || t->b.x < r || t->b.y < r ||
            t->a.x > board->width - r || t->a.y > board->height - r ||
            t->b.x > board->width - r || t->b.y > board->height - r) return false;
    }
    return true;
}

bool pcb_export_gerber(const PcbBoard *board, const char *prefix, char *error, size_t error_size)
{
    if (!prefix || !*prefix || strlen(prefix) > 3900)
        return fail(error, error_size, "Enter a shorter export prefix.");
    if (!valid_board(board)) return fail(error, error_size, "Board geometry touches an edge or is invalid. Move copper away from the outline.");
    const char *names[7] = {"F_Cu.gbr", "B_Cu.gbr", "F_Mask.gbr", "B_Mask.gbr",
                            "F_SilkS.gbr", "Edge_Cuts.gbr", "PTH.drl"};
    char paths[7][4096], temps[7][4096];
    for (int i = 0; i < 7; ++i) {
        if (snprintf(paths[i], sizeof(paths[i]), "%s-%s", prefix, names[i]) >= (int)sizeof(paths[i]) ||
            snprintf(temps[i], sizeof(temps[i]), "%s.tmp", paths[i]) >= (int)sizeof(temps[i]))
            return fail(error, error_size, "Export prefix is too long.");
    }
    int count = 0;
    bool written = true;
    for (int i = 0; i < 7; ++i) {
        FILE *file = open_file(temps[i]);
        if (!file) { written = false; break; }
        ++count;
        switch (i) {
            case 0: copper_layer(file, board, true); break;
            case 1: copper_layer(file, board, false); break;
            case 2: mask_layer(file, board, true); break;
            case 3: mask_layer(file, board, false); break;
            case 4: silk_layer(file, board); break;
            case 5: outline_layer(file, board); break;
            default: drill_file(file, board); break;
        }
        bool ok = !ferror(file);
        if (fclose(file) != 0) ok = false;
        if (!ok) { written = false; break; }
    }
    if (!written) {
        for (int i = 0; i < count; ++i) delete_file(temps[i]);
        return fail(error, error_size, "Could not write fabrication files. Check the output directory.");
    }
    for (int i = 0; i < 7; ++i) {
        if (!replace_file(temps[i], paths[i])) {
            for (int j = i; j < 7; ++j) delete_file(temps[j]);
            return fail(error, error_size, "Could not replace all fabrication files. Check the output directory.");
        }
    }
    if (error && error_size) error[0] = '\0';
    return true;
}
