#ifndef PCB_H
#define PCB_H

#include <stdbool.h>
#include <stddef.h>

#define PCB_MAX_COMPONENTS 128
#define PCB_MAX_TRACES 1024
#define PCB_MAX_PADS 8
#define PCB_HISTORY_SIZE 32
#define PCB_GRID 1.27f

typedef struct { float x, y; } PcbPoint;
typedef enum {
    PCB_RESISTOR, PCB_CAPACITOR, PCB_LED, PCB_HEADER, PCB_DIP8,
    PCB_FOOTPRINT_COUNT
} PcbFootprint;

typedef struct {
    PcbFootprint type;
    const char *name;
    const char *prefix;
    const char *description;
    float body_width, body_height;
    int pad_count;
    PcbPoint pads[PCB_MAX_PADS];
} PcbFootprintInfo;

typedef struct {
    int id;
    PcbFootprint type;
    PcbPoint position;
    int rotation;
} PcbComponent;

typedef struct {
    PcbPoint a, b;
    float width;
} PcbTrace;

typedef struct {
    float width, height;
    int next_id;
    int component_count, trace_count;
    PcbComponent components[PCB_MAX_COMPONENTS];
    PcbTrace traces[PCB_MAX_TRACES];
} PcbBoard;

/* Bounded, allocation-free snapshot history; one checkpoint per user action. */
typedef struct {
    PcbBoard states[PCB_HISTORY_SIZE];
    int start, count, cursor;
} PcbHistory;

const PcbFootprintInfo *pcb_footprint(PcbFootprint type);
void pcb_init(PcbBoard *board);
PcbPoint pcb_rotate(PcbPoint point, int rotation);
PcbPoint pcb_pad_position(const PcbComponent *component, int pad);
PcbPoint pcb_snap(PcbPoint point);
bool pcb_contains(const PcbBoard *board, PcbPoint point);
bool pcb_component_fits(const PcbBoard *board, const PcbComponent *component);
int pcb_add_component(PcbBoard *board, PcbFootprint type, PcbPoint position, int rotation);
bool pcb_remove_component(PcbBoard *board, int index);
bool pcb_remove_trace(PcbBoard *board, int index);
PcbPoint pcb_route_corner(PcbPoint a, PcbPoint b);
bool pcb_add_route(PcbBoard *board, PcbPoint a, PcbPoint b, float width);
float pcb_distance(PcbPoint a, PcbPoint b);
float pcb_segment_distance(PcbPoint p, PcbPoint a, PcbPoint b);
void pcb_history_init(PcbHistory *history, const PcbBoard *board);
void pcb_history_push(PcbHistory *history, const PcbBoard *board);
bool pcb_undo(PcbHistory *history, PcbBoard *board);
bool pcb_redo(PcbHistory *history, PcbBoard *board);
bool pcb_save(const PcbBoard *board, const char *path, char *error, size_t error_size);
bool pcb_load(PcbBoard *board, const char *path, char *error, size_t error_size);

#endif
