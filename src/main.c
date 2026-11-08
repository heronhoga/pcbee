#include "pcb.h"
#include "learn.h"
#include "raylib.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef PCBEE_VERSION
#define PCBEE_VERSION "0.2.0"
#endif

#define SIDEBAR 228
#define HEADER 76
#define FOOTER 34
#define PATH_SIZE 1024
#define LESSON_PANEL 304

typedef enum { TOOL_SELECT, TOOL_ROUTE, TOOL_PLACE } Tool;
typedef enum { MODAL_NONE, MODAL_OPEN, MODAL_SAVE, MODAL_NEW_CONFIRM,
               MODAL_OPEN_CONFIRM, MODAL_QUIT_CONFIRM, MODAL_HELP, MODAL_LEARN_START } Modal;
typedef struct {
    PcbBoard board, saved;
    PcbHistory history;
    Camera2D camera;
    Tool tool;
    Modal modal;
    PcbFootprint footprint;
    int rotation, selected_component, selected_trace;
    bool routing, dragging, running;
    PcbPoint route_start, drag_origin, drag_offset;
    float trace_width;
    char path[PATH_SIZE], input[PATH_SIZE], message[256];
    double message_until;
    bool learning, lesson_passed;
    int lesson_step;
    PcbBoard lesson_checked;
    char lesson_feedback[256];
} App;

static App app;
static const Color background = {18, 23, 29, 255};
static const Color panel = {25, 31, 39, 255};
static const Color raised = {35, 43, 53, 255};
static const Color border = {49, 60, 70, 255};
static const Color text = {228, 235, 240, 255};
static const Color muted = {139, 155, 169, 255};
static const Color accent = {247, 192, 86, 255};
static const Color copper = {239, 133, 100, 255};
static const Color silk = {182, 214, 202, 255};
static const Color selected = {100, 215, 236, 255};

static Rectangle canvas(void)
{
    return (Rectangle){SIDEBAR, HEADER, GetScreenWidth() - SIDEBAR - (app.learning ? LESSON_PANEL : 0),
                       GetScreenHeight() - HEADER - FOOTER};
}

static Vector2 screen_point(PcbPoint p)
{
    return GetWorldToScreen2D((Vector2){p.x, p.y}, app.camera);
}

static PcbPoint world_point(Vector2 p)
{
    Vector2 v = GetScreenToWorld2D(p, app.camera);
    return (PcbPoint){v.x, v.y};
}

static bool dirty(void)
{
    return memcmp(&app.board, &app.saved, sizeof(PcbBoard)) != 0;
}

static void notify(const char *message)
{
    snprintf(app.message, sizeof(app.message), "%s", message);
    app.message_until = GetTime() + 5.0;
}

static void fit_board(void)
{
    Rectangle area = canvas();
    app.camera.offset = (Vector2){area.x + area.width / 2, area.y + area.height / 2};
    app.camera.target = (Vector2){app.board.width / 2, app.board.height / 2};
    app.camera.zoom = fminf((area.width - 120) / app.board.width,
                          (area.height - 120) / app.board.height);
}

static void finish_interaction(void)
{
    if (app.dragging && app.selected_component >= 0)
        app.board.components[app.selected_component].position = app.drag_origin;
    app.dragging = false;
    app.routing = false;
}

static void clear_selection(void)
{
    app.selected_component = -1;
    app.selected_trace = -1;
}

static void set_tool(Tool tool)
{
    finish_interaction();
    clear_selection();
    app.tool = tool;
}

static void open_modal(Modal modal)
{
    finish_interaction();
    app.modal = modal;
    snprintf(app.input, sizeof(app.input), "%s", app.path);
    /* Discard the shortcut's text event before opening a path field. */
    while (GetCharPressed() != 0) {}
}

static void default_path(void)
{
    snprintf(app.path, sizeof(app.path), "board.pcb");
#ifdef __APPLE__
    /* Finder may launch bundles with an unwritable working directory. */
    const char *home = getenv("HOME");
    if (home) snprintf(app.path, sizeof(app.path), "%s/board.pcb", home);
#endif
}

static void new_board(void)
{
    app.learning = false;
    pcb_init(&app.board);
    app.saved = app.board;
    pcb_history_init(&app.history, &app.board);
    set_tool(TOOL_SELECT);
    default_path();
    app.modal = MODAL_NONE;
    fit_board();
    notify("New board ready. Choose a footprint to start placing.");
}

static void lesson_step(int step)
{
    set_tool(TOOL_SELECT);
    app.lesson_step = step;
    app.lesson_passed = false;
    app.lesson_feedback[0] = '\0';
    if (step >= 1 && step <= 3) {
        app.footprint = learn_targets[step - 1].type;
        app.rotation = 0;
        /* Revisiting a placed part should move it rather than add a duplicate. */
        bool exists = false;
        for (int i = 0; i < app.board.component_count; ++i)
            exists = exists || app.board.components[i].type == app.footprint;
        if (!exists) set_tool(TOOL_PLACE);
    } else if (step >= 4 && step <= 6) set_tool(TOOL_ROUTE);
}

static void start_lesson(void)
{
    new_board();
    app.learning = true;
    app.board.width = 63.5f;
    app.board.height = 45.72f;
    app.saved = app.board;
    pcb_history_init(&app.history, &app.board);
    app.trace_width = 0.4f;
    char *name = strrchr(app.path, '/');
    name = name ? name + 1 : app.path;
    snprintf(name, sizeof(app.path) - (size_t)(name - app.path), "led-practice.pcb");
    lesson_step(0);
    fit_board();
    notify("LED lesson started. Answer the question in the learning panel.");
}

static void leave_lesson(void)
{
    set_tool(TOOL_SELECT);
    app.learning = false;
    fit_board();
    notify("Lesson closed. Your practice board is still here; Save to keep it.");
}

static void check_lesson(void)
{
    finish_interaction();
    app.lesson_passed = learn_check(&app.board, app.lesson_step,
                                   app.lesson_feedback, sizeof(app.lesson_feedback));
    app.lesson_checked = app.board;
}

static void update_lesson(void)
{
    if (app.learning && app.lesson_passed && app.lesson_step > 0 &&
        memcmp(&app.board, &app.lesson_checked, sizeof(PcbBoard)) != 0) {
        app.lesson_passed = false;
        snprintf(app.lesson_feedback, sizeof(app.lesson_feedback),
                 "Board changed. Check this step again before continuing.");
    }
}

static void request_new(void)
{
    finish_interaction();
    if (dirty()) open_modal(MODAL_NEW_CONFIRM);
    else new_board();
}

static void request_open(void)
{
    finish_interaction();
    open_modal(dirty() ? MODAL_OPEN_CONFIRM : MODAL_OPEN);
}

static void undo(bool redo)
{
    finish_interaction();
    clear_selection();
    bool changed = redo ? pcb_redo(&app.history, &app.board) : pcb_undo(&app.history, &app.board);
    notify(changed ? (redo ? "Redo complete." : "Undo complete.") : "No more history.");
}

static bool button(Rectangle rect, const char *label, bool active, bool enabled)
{
    bool hover = enabled && CheckCollisionPointRec(GetMousePosition(), rect);
    DrawRectangleRounded(rect, 0.18f, 4, active && enabled ? accent : hover ? border : raised);
    int width = MeasureText(label, 16);
    DrawText(label, (int)(rect.x + (rect.width - width) / 2), (int)(rect.y + (rect.height - 16) / 2),
             16, active && enabled ? background : enabled ? text : muted);
    return hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

static void draw_line(PcbPoint a, PcbPoint b, float width, Color color)
{
    Vector2 p = screen_point(a), q = screen_point(b);
    float thickness = fmaxf(1.0f, width * app.camera.zoom);
    DrawLineEx(p, q, thickness, color);
    DrawCircleV(p, thickness / 2, color);
    DrawCircleV(q, thickness / 2, color);
}

static void draw_component(const PcbComponent *component, Color color, bool highlight, bool ghost)
{
    const PcbFootprintInfo *info = pcb_footprint(component->type);
    float zoom = app.camera.zoom;
    PcbPoint corners[4] = {{-info->body_width / 2, -info->body_height / 2},
        {info->body_width / 2, -info->body_height / 2},
        {info->body_width / 2, info->body_height / 2},
        {-info->body_width / 2, info->body_height / 2}};
    Vector2 points[4];
    for (int i = 0; i < 4; ++i) {
        PcbPoint p = pcb_rotate(corners[i], component->rotation);
        p.x += component->position.x;
        p.y += component->position.y;
        points[i] = screen_point(p);
    }
    Color outline = highlight ? selected : color;
    if (component->type == PCB_LED || component->type == PCB_CAPACITOR) {
        Vector2 center = screen_point(component->position);
        DrawPolyLinesEx(center, 32, info->body_width * zoom / 2, 0, 1.5f, outline);
    } else {
        for (int i = 0; i < 4; ++i) DrawLineEx(points[i], points[(i + 1) % 4], 1.5f, outline);
    }
    if (component->type == PCB_RESISTOR) {
        draw_line(pcb_pad_position(component, 0), pcb_pad_position(component, 1), 0.16f, outline);
    }
    for (int i = 0; i < info->pad_count; ++i) {
        Vector2 p = screen_point(pcb_pad_position(component, i));
        float radius = fmaxf(2, 0.9f * zoom);
        Color pad_color = ghost ? Fade(outline, 0.7f) : highlight ? selected : accent;
        if (i == 0) DrawRectangleV((Vector2){p.x - radius, p.y - radius},
                                   (Vector2){2 * radius, 2 * radius}, pad_color);
        else DrawCircleV(p, radius, pad_color);
        DrawCircleV(p, fmaxf(1, 0.4f * zoom), background);
    }
    if (!ghost && zoom >= 3) {
        Vector2 center = screen_point(component->position);
        float height = component->rotation % 180 ? info->body_width : info->body_height;
        const char *label = TextFormat("%s%d", info->prefix, component->id);
        DrawText(label, (int)(center.x - MeasureText(label, 14) / 2),
                 (int)(center.y - height * zoom / 2 - 19), 14, outline);
    }
    if (highlight) {
        Vector2 center = screen_point(component->position);
        DrawCircleLines((int)center.x, (int)center.y, 4, selected);
    }
}

static void lesson_marker(PcbPoint point, const char *label, Color color)
{
    Vector2 p = screen_point(point);
    DrawCircleLines((int)p.x, (int)p.y, fmaxf(8, 1.3f * app.camera.zoom), color);
    int width = MeasureText(label, 12);
    DrawRectangle((int)p.x - width / 2 - 4, (int)p.y - 30, width + 8, 18, panel);
    DrawText(label, (int)p.x - width / 2, (int)p.y - 27, 12, color);
}

static void draw_lesson_hints(void)
{
    if (!app.learning) return;
    int step = app.lesson_step;
    if (step >= 1 && step <= 3) {
        const PcbComponent *target = &learn_targets[step - 1];
        if (!learn_placed(&app.board, step - 1)) draw_component(target, selected, false, true);
        Vector2 p = screen_point(target->position);
        DrawCircleLines((int)p.x, (int)p.y, 7, selected);
        DrawLine((int)p.x - 12, (int)p.y, (int)p.x + 12, (int)p.y, selected);
        DrawLine((int)p.x, (int)p.y - 12, (int)p.x, (int)p.y + 12, selected);
        const char *label = TextFormat("TARGET: %s / %d deg", pcb_footprint(target->type)->name, target->rotation);
        DrawText(label, (int)p.x - MeasureText(label, 12) / 2, (int)p.y + 45, 12, selected);
    } else if (step >= 4 && step <= 6) {
        PcbPoint a, b;
        learn_net_ends(step - 4, &a, &b);
        /* Dotted connection hint, never actual copper or a suggested safe route. */
        Vector2 p = screen_point(a), q = screen_point(b);
        float length = sqrtf((q.x - p.x) * (q.x - p.x) + (q.y - p.y) * (q.y - p.y));
        for (float d = 0; d < length; d += 10) {
            float t = d / length;
            DrawCircleV((Vector2){p.x + (q.x - p.x) * t, p.y + (q.y - p.y) * t}, 1.5f, Fade(selected, 0.65f));
        }
        const char *from[] = {"1: +5 V", "1: resistor pad 2", "1: LED K (square)"};
        const char *to[] = {"2: resistor pad 1", "2: LED A (round)", "2: header pad 2"};
        lesson_marker(a, from[step - 4], selected);
        lesson_marker(b, to[step - 4], accent);
    }
}

static PcbPoint snap_cursor(PcbPoint raw, bool pads)
{
    PcbPoint result = pcb_snap(raw);
    if (!pads) return result;
    float nearest = 12.0f / app.camera.zoom;
    for (int i = 0; i < app.board.component_count; ++i) {
        const PcbComponent *component = &app.board.components[i];
        const PcbFootprintInfo *info = pcb_footprint(component->type);
        for (int j = 0; j < info->pad_count; ++j) {
            PcbPoint p = pcb_pad_position(component, j);
            float distance = pcb_distance(raw, p);
            if (distance < nearest) { result = p; nearest = distance; }
        }
    }
    for (int i = 0; i < app.board.trace_count; ++i) {
        PcbPoint ends[2] = {app.board.traces[i].a, app.board.traces[i].b};
        for (int j = 0; j < 2; ++j) {
            float distance = pcb_distance(raw, ends[j]);
            if (distance < nearest) { result = ends[j]; nearest = distance; }
        }
    }
    return result;
}

static int hit_component(PcbPoint point)
{
    for (int i = app.board.component_count - 1; i >= 0; --i) {
        const PcbComponent *c = &app.board.components[i];
        const PcbFootprintInfo *info = pcb_footprint(c->type);
        PcbPoint local = pcb_rotate((PcbPoint){point.x - c->position.x, point.y - c->position.y},
                                   360 - c->rotation);
        if (fabsf(local.x) <= info->body_width / 2 + 0.7f &&
            fabsf(local.y) <= info->body_height / 2 + 0.7f) return i;
        for (int j = 0; j < info->pad_count; ++j)
            if (pcb_distance(point, pcb_pad_position(c, j)) < 1.1f) return i;
    }
    return -1;
}

static int hit_trace(PcbPoint point)
{
    for (int i = app.board.trace_count - 1; i >= 0; --i) {
        PcbTrace *t = &app.board.traces[i];
        if (pcb_segment_distance(point, t->a, t->b) < t->width / 2 + 5 / app.camera.zoom)
            return i;
    }
    return -1;
}

static void rotate_selection(void)
{
    if (app.tool == TOOL_PLACE) app.rotation = (app.rotation + 90) % 360;
    else if (app.selected_component >= 0 && !app.dragging) {
        PcbComponent c = app.board.components[app.selected_component];
        c.rotation = (c.rotation + 90) % 360;
        if (pcb_component_fits(&app.board, &c)) {
            app.board.components[app.selected_component] = c;
            pcb_history_push(&app.history, &app.board);
        } else notify("Rotation would put the footprint outside the board.");
    }
}

static void delete_selection(void)
{
    finish_interaction();
    bool changed = pcb_remove_component(&app.board, app.selected_component);
    changed = pcb_remove_trace(&app.board, app.selected_trace) || changed;
    if (changed) pcb_history_push(&app.history, &app.board);
    clear_selection();
}

static void keyboard(void)
{
    bool command = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL) ||
                   IsKeyDown(KEY_LEFT_SUPER) || IsKeyDown(KEY_RIGHT_SUPER);
    bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    if (command) {
        if (IsKeyPressed(KEY_S)) open_modal(MODAL_SAVE);
        else if (IsKeyPressed(KEY_O)) request_open();
        else if (IsKeyPressed(KEY_N)) request_new();
        else if (IsKeyPressed(KEY_Z)) undo(shift);
        else if (IsKeyPressed(KEY_Y)) undo(true);
        return;
    }
    if (IsKeyPressed(KEY_ONE) || IsKeyPressed(KEY_V)) set_tool(TOOL_SELECT);
    if (IsKeyPressed(KEY_TWO) || IsKeyPressed(KEY_T)) set_tool(TOOL_ROUTE);
    if (IsKeyPressed(KEY_R)) rotate_selection();
    if (IsKeyPressed(KEY_F)) fit_board();
    if (IsKeyPressed(KEY_DELETE) || IsKeyPressed(KEY_BACKSPACE)) delete_selection();
    if (IsKeyPressed(KEY_F1)) open_modal(MODAL_HELP);
    if (IsKeyPressed(KEY_F2)) {
        if (app.learning) leave_lesson();
        else open_modal(MODAL_LEARN_START);
    }
    if (IsKeyPressed(KEY_ENTER)) app.routing = false;
    if (IsKeyPressed(KEY_ESCAPE)) {
        if (app.routing || app.dragging) finish_interaction();
        else set_tool(TOOL_SELECT);
    }
}

static void update_canvas(void)
{
    Vector2 mouse = GetMousePosition();
    bool over = CheckCollisionPointRec(mouse, canvas());
    bool panning = IsMouseButtonDown(MOUSE_BUTTON_MIDDLE) ||
                   (IsKeyDown(KEY_SPACE) && IsMouseButtonDown(MOUSE_BUTTON_LEFT));
    if (over && panning && !app.dragging) {
        Vector2 delta = GetMouseDelta();
        app.camera.target.x -= delta.x / app.camera.zoom;
        app.camera.target.y -= delta.y / app.camera.zoom;
    }
    float wheel = GetMouseWheelMove();
    if (over && wheel != 0) {
        PcbPoint before = world_point(mouse);
        app.camera.zoom = fmaxf(0.3f, fminf(60, app.camera.zoom * powf(1.15f, wheel)));
        PcbPoint after = world_point(mouse);
        app.camera.target.x += before.x - after.x;
        app.camera.target.y += before.y - after.y;
    }
    PcbPoint raw = world_point(mouse);
    PcbPoint point = snap_cursor(raw, app.tool == TOOL_ROUTE);
    if (app.dragging) {
        if (over && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            PcbComponent c = app.board.components[app.selected_component];
            c.position = pcb_snap((PcbPoint){raw.x - app.drag_offset.x, raw.y - app.drag_offset.y});
            if (pcb_component_fits(&app.board, &c)) app.board.components[app.selected_component] = c;
        }
        if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
            app.dragging = false;
            if (pcb_distance(app.drag_origin, app.board.components[app.selected_component].position) > 0.001f) {
                pcb_history_push(&app.history, &app.board);
                notify("Footprint moved. Existing traces keep their positions.");
            }
        }
        return;
    }
    if (over && IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
        if (app.routing) app.routing = false;
        else set_tool(TOOL_SELECT);
    }
    if (!over || panning || !IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) return;
    if (app.tool == TOOL_SELECT) {
        clear_selection();
        app.selected_component = hit_component(raw);
        if (app.selected_component >= 0) {
            app.drag_origin = app.board.components[app.selected_component].position;
            app.drag_offset = (PcbPoint){raw.x - app.drag_origin.x, raw.y - app.drag_origin.y};
            app.dragging = true;
        } else app.selected_trace = hit_trace(raw);
    } else if (app.tool == TOOL_PLACE) {
        int index = pcb_add_component(&app.board, app.footprint, point, app.rotation);
        if (index >= 0) pcb_history_push(&app.history, &app.board);
        else notify("Cannot place here: board edge or 128-component limit.");
    } else if (pcb_contains(&app.board, point)) {
        if (!app.routing) { app.routing = true; app.route_start = point; }
        else if (pcb_add_route(&app.board, app.route_start, point, app.trace_width)) {
            pcb_history_push(&app.history, &app.board);
            app.route_start = point;
        } else notify("Route not added: zero length or 1024-segment limit.");
    }
}

static void draw_board(void)
{
    Rectangle area = canvas();
    BeginScissorMode((int)area.x, (int)area.y, (int)area.width, (int)area.height);
    Vector2 origin = screen_point((PcbPoint){0, 0});
    float width = app.board.width * app.camera.zoom, height = app.board.height * app.camera.zoom;
    DrawRectangleRounded((Rectangle){origin.x + 7, origin.y + 9, width, height}, 0.015f, 4, (Color){9, 14, 18, 200});
    DrawRectangleV(origin, (Vector2){width, height}, (Color){26, 53, 47, 255});
    DrawRectangleLinesEx((Rectangle){origin.x, origin.y, width, height}, 1.5f, (Color){78, 126, 108, 255});
    float step = PCB_GRID;
    while (step * app.camera.zoom < 9) step *= 2;
    PcbPoint low = world_point((Vector2){area.x, area.y});
    PcbPoint high = world_point((Vector2){area.x + area.width, area.y + area.height});
    float first_x = fmaxf(step, ceilf(low.x / step) * step);
    float first_y = fmaxf(step, ceilf(low.y / step) * step);
    for (float x = first_x; x < fminf(app.board.width, high.x); x += step) {
        for (float y = first_y; y < fminf(app.board.height, high.y); y += step) {
            Vector2 p = screen_point((PcbPoint){x, y});
            DrawPixel((int)p.x, (int)p.y, (Color){57, 85, 75, 255});
        }
    }
    for (int i = 0; i < app.board.trace_count; ++i) {
        PcbTrace *trace = &app.board.traces[i];
        if (i == app.selected_trace) draw_line(trace->a, trace->b, trace->width + 0.65f, Fade(selected, 0.5f));
        draw_line(trace->a, trace->b, trace->width, i == app.selected_trace ? selected : copper);
    }
    for (int i = 0; i < app.board.component_count; ++i)
        draw_component(&app.board.components[i], silk, i == app.selected_component, false);
    draw_lesson_hints();

    bool over = CheckCollisionPointRec(GetMousePosition(), area) && app.modal == MODAL_NONE;
    if (over && !IsKeyDown(KEY_SPACE)) {
        PcbPoint point = snap_cursor(world_point(GetMousePosition()), app.tool == TOOL_ROUTE);
        if (app.tool == TOOL_PLACE) {
            PcbComponent ghost = {0, app.footprint, point, app.rotation};
            draw_component(&ghost, pcb_component_fits(&app.board, &ghost) ? selected : RED, false, true);
        }
        if (app.tool == TOOL_ROUTE && pcb_contains(&app.board, point)) {
            Vector2 p = screen_point(point);
            DrawCircleLines((int)p.x, (int)p.y, 6, selected);
            DrawLine((int)p.x - 10, (int)p.y, (int)p.x + 10, (int)p.y, Fade(selected, 0.5f));
            DrawLine((int)p.x, (int)p.y - 10, (int)p.x, (int)p.y + 10, Fade(selected, 0.5f));
            if (app.routing) {
                PcbPoint corner = pcb_route_corner(app.route_start, point);
                draw_line(app.route_start, corner, app.trace_width, Fade(copper, 0.65f));
                draw_line(corner, point, app.trace_width, Fade(copper, 0.65f));
            }
        }
    }
    DrawRectangleRounded((Rectangle){area.x + 22, area.y + 18, 142, 27}, 0.3f, 4, panel);
    DrawCircle((int)area.x + 36, (int)area.y + 31, 4, copper);
    DrawText("TOP COPPER", (int)area.x + 48, (int)area.y + 25, 12, text);
    const char *size = TextFormat("%.1f x %.1f mm", app.board.width, app.board.height);
    DrawText(size, (int)(area.x + area.width - MeasureText(size, 14) - 22), (int)area.y + 25, 14, muted);
    const char *hint = app.tool == TOOL_PLACE ? "Click to place  /  R to rotate  /  Esc to select" :
        app.tool == TOOL_ROUTE ? "Click to route  /  Right-click or Enter to finish" :
        "Drag to move  /  Scroll to zoom  /  Space + drag to pan";
    int hint_width = MeasureText(hint, 14);
    int hint_x = (int)(area.x + (area.width - hint_width) / 2);
    DrawRectangleRounded((Rectangle){hint_x - 14, area.y + area.height - 42, hint_width + 28, 28}, 0.25f, 4, panel);
    DrawText(hint, hint_x, (int)(area.y + area.height - 35), 14, muted);
    if (!app.learning && app.board.component_count == 0 && app.board.trace_count == 0 && app.tool == TOOL_SELECT) {
        Vector2 center = screen_point((PcbPoint){app.board.width / 2, app.board.height / 2});
        const char *title = "Your next circuit starts here";
        DrawText(title, (int)center.x - MeasureText(title, 22) / 2, (int)center.y - 22, 22, silk);
        const char *subtitle = "Choose a footprint from the left to place it.";
        DrawText(subtitle, (int)center.x - MeasureText(subtitle, 16) / 2, (int)center.y + 12, 16, muted);
    }
    EndScissorMode();
}

static void draw_toolbar(void)
{
    bool enabled = app.modal == MODAL_NONE;
    DrawRectangle(0, 0, GetScreenWidth(), HEADER, panel);
    DrawLine(0, HEADER - 1, GetScreenWidth(), HEADER - 1, border);
    DrawPoly((Vector2){32, 36}, 6, 14, 30, accent);
    DrawCircle(32, 36, 5, background);
    DrawText("PCBEE", 55, 19, 26, text);
    DrawText("PCB LAYOUT / v" PCBEE_VERSION, 56, 49, 10, muted);
    float x = SIDEBAR + 20;
    if (button((Rectangle){x, 22, 62, 32}, "New", false, enabled)) request_new();
    if (button((Rectangle){x + 70, 22, 62, 32}, "Open", false, enabled)) request_open();
    if (button((Rectangle){x + 140, 22, 70, 32}, "Save", true, enabled)) open_modal(MODAL_SAVE);
    if (button((Rectangle){x + 230, 22, 62, 32}, "Undo", false, enabled)) undo(false);
    if (button((Rectangle){x + 300, 22, 62, 32}, "Redo", false, enabled)) undo(true);
    if (button((Rectangle){x + 382, 22, 72, 32}, "Fit [F]", false, enabled)) fit_board();
    if (button((Rectangle){x + 462, 22, 62, 32}, "Help", false, enabled)) open_modal(MODAL_HELP);
    if (button((Rectangle){x + 532, 22, 100, 32}, "Learn PCB", app.learning, enabled)) {
        if (app.learning) leave_lesson();
        else open_modal(MODAL_LEARN_START);
    }
    DrawText(dirty() ? "UNSAVED" : "SAVED", GetScreenWidth() - 105, 33, 13, dirty() ? accent : muted);
}

/* The built-in font uses ASCII lesson copy. Wrap at words within the panel. */
static int wrapped_text(const char *copy, int x, int y, int width, int font, Color color)
{
    while (*copy) {
        char line[256];
        int count = 0, space = -1;
        while (copy[count] && copy[count] != '\n' && count < (int)sizeof(line) - 1) {
            line[count] = copy[count];
            line[count + 1] = '\0';
            if (MeasureText(line, font) > width && count > 0) break;
            if (copy[count] == ' ') space = count;
            ++count;
        }
        if (copy[count] && copy[count] != '\n' && space > 0) count = space;
        line[count] = '\0';
        DrawText(line, x, y, font, color);
        y += font + 5;
        copy += count;
        while (*copy == ' ') ++copy;
        if (*copy == '\n') ++copy;
    }
    return y;
}

static void draw_lesson_panel(void)
{
    if (!app.learning) return;
    static const char *const titles[LEARN_STEPS] = {
        "Plan the LED circuit", "Place the connector", "Place the resistor", "Place the LED",
        "Route the supply", "Route the LED drive", "Close the return loop", "Review your board"
    };
    static const char *const concepts[LEARN_STEPS] = {
        "An LED needs a series resistor to limit current. With 5 V power and a 2 V LED: R = (5 - 2) / I. Which resistor gives about 4.4 mA?",
        "A footprint is the physical pattern for a part. Place connectors near an edge for access. The square pad is pad 1; we will use it for +5 V.",
        "This 680 ohm resistor limits LED current. Keep connected parts close, with room for soldering. Its two pads must remain separate copper nets.",
        "LEDs have polarity. In this lesson, square pad = cathode (K), round pad = anode (A). Real parts must be checked against their datasheets.",
        "A net is a group of connected pins. Connect header pad 1 to resistor pad 1. The blue dotted line is a connection hint, not copper.",
        "Connect resistor pad 2 to the LED's round anode pad. The resistor separates +5 V from LED drive; do not bridge its two pads.",
        "Current needs a return path. Connect the LED's square cathode pad to header pad 2. Keep header pads 3 and 4 isolated.",
        "Review all three nets: +5 V, LED drive, and GND. Each must be continuous and separate. Moving a footprint leaves its old traces behind."
    };
    static const char *const tasks[LEARN_STEPS] = {
        "Choose a resistor:",
        "Pin header is selected. Press R once (90 deg), then click the blue center cross. Esc exits placement; V lets you drag a part.",
        "Resistor is selected. Keep it horizontal (0 deg) and click the target center. Use only one resistor.",
        "LED is selected. Press R three times (270 deg), then click the target. The square cathode pad should be below the round anode.",
        "Route is selected. Click pad 1, then pad 2 as highlighted. Enter finishes. Use 0.4 mm copper for this exercise.",
        "Click the highlighted resistor pad, then LED anode. Enter finishes. Crossing or touching copper joins it on this single layer.",
        "Use waypoints below the LED, then back up to header pad 2 from the right. Avoid other pads. Enter finishes; Delete removes selected segments.",
        "Check the whole board below. Fix any missing connections, shorts, or stray copper. This lesson check is not manufacturing DRC."
    };
    bool enabled = app.modal == MODAL_NONE;
    int step = app.lesson_step;
    int left = GetScreenWidth() - LESSON_PANEL, x = left + 18, width = LESSON_PANEL - 36;
    int bottom = GetScreenHeight() - FOOTER;
    DrawRectangle(left, HEADER, LESSON_PANEL, bottom - HEADER, panel);
    DrawLine(left, HEADER, left, bottom, border);
    DrawText(TextFormat("LED LAB / STEP %d OF %d", step + 1, LEARN_STEPS), x, HEADER + 20, 12, accent);
    DrawText(titles[step], x, HEADER + 46, 21, text);
    for (int i = 0; i < LEARN_STEPS; ++i)
        DrawRectangle(x + i * 34, HEADER + 80, 28, 4, i <= step ? accent : border);
    wrapped_text(concepts[step], x, HEADER + 104, width, 15, text);
    DrawText("YOUR TASK", x, HEADER + 232, 12, selected);
    wrapped_text(tasks[step], x, HEADER + 254, width, 14, muted);
    if (step == 0) {
        const char *answers[] = {"68 ohm", "680 ohm", "6.8k ohm"};
        for (int i = 0; i < 3; ++i) {
            if (button((Rectangle){x + i * 92.0f, HEADER + 286, 84, 34}, answers[i],
                       app.lesson_passed && i == 1, enabled)) {
                app.lesson_passed = i == 1;
                app.lesson_checked = app.board;
                snprintf(app.lesson_feedback, sizeof(app.lesson_feedback), "%s", i == 1 ?
                         "Correct! (5 - 2) / 680 = 0.0044 A, or 4.4 mA. Now build the layout." :
                         i == 0 ? "68 ohms gives about 44 mA, ten times our target. Try a larger resistor." :
                                  "6.8k ohms gives about 0.44 mA, ten times too little. Try a smaller resistor.");
            }
        }
    }
    int feedback_y = HEADER + 362;
    BeginScissorMode(x, feedback_y, width, bottom - 124 - feedback_y);
    wrapped_text(app.lesson_feedback[0] ? app.lesson_feedback :
                 "Follow the task, then check your work. You can use Undo and try again.",
                 x, feedback_y, width, 14, app.lesson_passed ? silk : accent);
    EndScissorMode();
    if (button((Rectangle){x, bottom - 116, 128, 34}, "Back", false, enabled && step > 0))
        lesson_step(step - 1);
    if (step > 0 && button((Rectangle){x + 140, bottom - 116, 128, 34}, "Check step", false, enabled))
        check_lesson();
    if (step < LEARN_STEPS - 1) {
        if (button((Rectangle){x, bottom - 74, width, 32}, "Next step", true, enabled && app.lesson_passed)) {
            /* Revalidate before advancing, including edits made since a check. */
            if (step > 0) check_lesson();
            if (app.lesson_passed) lesson_step(step + 1);
        }
    } else if (button((Rectangle){x, bottom - 74, width, 32}, "Save practice board", true, enabled && app.lesson_passed))
        open_modal(MODAL_SAVE);
    if (button((Rectangle){x, bottom - 34, 128, 26}, "Restart", false, enabled)) open_modal(MODAL_LEARN_START);
    if (button((Rectangle){x + 140, bottom - 34, 128, 26}, "Exit lesson", false, enabled)) leave_lesson();
}

static void draw_sidebar(void)
{
    bool enabled = app.modal == MODAL_NONE;
    DrawRectangle(0, HEADER, SIDEBAR, GetScreenHeight() - HEADER - FOOTER, panel);
    DrawLine(SIDEBAR - 1, HEADER, SIDEBAR - 1, GetScreenHeight() - FOOTER, border);
    DrawText("TOOLS", 20, 97, 12, muted);
    if (button((Rectangle){16, 122, 94, 36}, "Select  1", app.tool == TOOL_SELECT, enabled)) set_tool(TOOL_SELECT);
    if (button((Rectangle){118, 122, 94, 36}, "Route  2", app.tool == TOOL_ROUTE, enabled)) set_tool(TOOL_ROUTE);
    DrawText("FOOTPRINTS", 20, 184, 12, muted);
    for (int i = 0; i < PCB_FOOTPRINT_COUNT; ++i) {
        const PcbFootprintInfo *info = pcb_footprint((PcbFootprint)i);
        Rectangle rect = {16, 208 + i * 57.0f, 196, 49};
        bool active = app.tool == TOOL_PLACE && app.footprint == (PcbFootprint)i;
        bool hover = enabled && CheckCollisionPointRec(GetMousePosition(), rect);
        DrawRectangleRounded(rect, 0.15f, 4, active ? (Color){58, 53, 39, 255} : hover ? border : raised);
        if (active) DrawRectangle((int)rect.x, (int)rect.y + 7, 3, 35, accent);
        DrawText(info->name, 29, (int)rect.y + 8, 17, active ? accent : text);
        DrawText(info->description, 29, (int)rect.y + 30, 10, muted);
        if (hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            set_tool(TOOL_PLACE);
            app.footprint = (PcbFootprint)i;
            app.rotation = 0;
        }
    }
    DrawText("TRACE WIDTH / mm", 20, 514, 12, muted);
    const float widths[] = {0.4f, 0.6f, 1.0f};
    for (int i = 0; i < 3; ++i) {
        if (button((Rectangle){16 + i * 68.0f, 539, 60, 30}, TextFormat("%.1f", widths[i]),
                   app.trace_width == widths[i], enabled)) app.trace_width = widths[i];
    }
    DrawLine(16, 590, 212, 590, border);
    DrawText(TextFormat("%d components", app.board.component_count), 20, 607, 15, text);
    DrawText(TextFormat("%d trace segments", app.board.trace_count), 20, 631, 15, text);
    if (GetScreenHeight() >= 780) {
        DrawText("R    Rotate footprint", 20, GetScreenHeight() - 101, 12, muted);
        DrawText("Del  Delete selection", 20, GetScreenHeight() - 80, 12, muted);
        DrawText("F1   All shortcuts", 20, GetScreenHeight() - 59, 12, muted);
    }
}

static void draw_status(void)
{
    int y = GetScreenHeight() - FOOTER;
    DrawRectangle(0, y, GetScreenWidth(), FOOTER, panel);
    DrawLine(0, y, GetScreenWidth(), y, border);
    const char *status = GetTime() < app.message_until ? app.message : app.path;
    BeginScissorMode(0, y, GetScreenWidth() - 380, FOOTER);
    DrawText(status, 18, y + 11, 12, GetTime() < app.message_until ? accent : muted);
    EndScissorMode();
    PcbPoint p = pcb_snap(world_point(GetMousePosition()));
    DrawText(TextFormat("X %6.2f   Y %6.2f mm  |  Grid 1.27 mm", p.x, p.y),
             GetScreenWidth() - 358, y + 11, 12, muted);
}

static void submit_path(void)
{
    if (!app.input[0]) { notify("Enter a project path first."); return; }
    char error[256];
    bool saving = app.modal == MODAL_SAVE;
    bool ok = saving ? pcb_save(&app.board, app.input, error, sizeof(error)) :
                       pcb_load(&app.board, app.input, error, sizeof(error));
    if (!ok) { notify(error); return; }
    snprintf(app.path, sizeof(app.path), "%s", app.input);
    app.saved = app.board;
    if (!saving) {
        app.learning = false;
        pcb_history_init(&app.history, &app.board);
        set_tool(TOOL_SELECT);
        fit_board();
    }
    app.modal = MODAL_NONE;
    notify(saving ? "Project saved." : "Project opened.");
}

static void draw_modal(void)
{
    if (app.modal == MODAL_NONE) return;
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), (Color){0, 0, 0, 160});
    bool help = app.modal == MODAL_HELP;
    bool learn = app.modal == MODAL_LEARN_START;
    bool path = app.modal == MODAL_OPEN || app.modal == MODAL_SAVE;
    float width = 650, height = help ? 500 : learn ? 300 : 250;
    float x = (GetScreenWidth() - width) / 2, y = (GetScreenHeight() - height) / 2;
    DrawRectangleRounded((Rectangle){x, y, width, height}, 0.035f, 4, panel);
    const char *title = help ? "A small workbench for your next idea." :
        learn ? "Learn PCB design: LED lab" :
        path ? (app.modal == MODAL_SAVE ? "Save project" : "Open project") : "Unsaved changes";
    DrawText(title, (int)x + 28, (int)y + 25, 24, text);
    if (help) {
        const char *lines[] = {
            "1 / V                 Select; drag a footprint to move it",
            "2 / T                 Route top-copper traces (45 / 90 degrees)",
            "R                     Rotate a footprint by 90 degrees",
            "Delete / Backspace    Delete the selected footprint or segment",
            "Scroll                Zoom around the pointer",
            "Middle drag / Space   Pan (Space + left drag)",
            "F                     Fit the board in the window",
            "Esc / Right-click     Cancel placement or finish a route",
            "Ctrl/Cmd + S / O / N  Save / open / new project",
            "Ctrl/Cmd + Z          Undo (Shift + Z or Ctrl + Y to redo)",
            "F2 / Learn PCB        Start or close the interactive LED lesson"
        };
        for (int i = 0; i < 11; ++i) DrawText(lines[i], (int)x + 28, (int)y + 74 + i * 27, 14, muted);
        DrawText("Layout prototype: no netlist, general DRC, or manufacturing export.",
                 (int)x + 28, (int)y + 387, 14, accent);
        if (button((Rectangle){x + width - 116, y + height - 58, 88, 34}, "Got it", true, true) ||
            IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_ENTER)) app.modal = MODAL_NONE;
    } else if (learn) {
        wrapped_text("Build a 5 V LED indicator with a resistor quiz, placement targets, routing hints, and checks on your actual board. About 10-15 minutes.",
                     (int)x + 28, (int)y + 70, (int)width - 56, 16, text);
        wrapped_text(dirty() ?
                     "Starting replaces this board and discards unsaved edits. Cancel and Save first to keep your work." :
                     "Start opens a fresh practice board. Save your layout at any time; lesson progress lasts for this session.",
                     (int)x + 28, (int)y + 145, (int)width - 56, 15, dirty() ? accent : muted);
        if (button((Rectangle){x + width - 254, y + height - 58, 88, 34}, "Cancel", false, true) ||
            IsKeyPressed(KEY_ESCAPE)) app.modal = MODAL_NONE;
        if (button((Rectangle){x + width - 156, y + height - 58, 128, 34},
                   dirty() ? "Discard & start" : "Start lesson", true, true)) start_lesson();
    } else if (path) {
        DrawText("Project file path (.pcb). Relative paths use the working directory.",
                 (int)x + 28, (int)y + 65, 14, muted);
        int ch;
        while ((ch = GetCharPressed()) != 0) {
            size_t length = strlen(app.input);
            int bytes = 0;
            const char *encoded = CodepointToUTF8(ch, &bytes);
            if (ch >= 32 && ch != 127 && bytes > 0 && length + (size_t)bytes < sizeof(app.input)) {
                memcpy(app.input + length, encoded, (size_t)bytes);
                app.input[length + (size_t)bytes] = '\0';
            }
        }
        bool command = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL) ||
                       IsKeyDown(KEY_LEFT_SUPER) || IsKeyDown(KEY_RIGHT_SUPER);
        if (command && IsKeyPressed(KEY_A)) app.input[0] = '\0';
        if (command && IsKeyPressed(KEY_V)) {
            const char *clipboard = GetClipboardText();
            if (clipboard && strlen(clipboard) < sizeof(app.input))
                snprintf(app.input, sizeof(app.input), "%s", clipboard);
            else if (clipboard) notify("The pasted path is too long.");
        }
        if ((IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) && app.input[0]) {
            size_t length = strlen(app.input) - 1;
            while (length > 0 && ((unsigned char)app.input[length] & 0xc0) == 0x80) --length;
            app.input[length] = '\0';
        }
        DrawRectangleRounded((Rectangle){x + 28, y + 96, width - 56, 43}, 0.1f, 4, background);
        DrawRectangleLinesEx((Rectangle){x + 28, y + 96, width - 56, 43}, 1, accent);
        const char *visible = app.input;
        while (*visible && MeasureText(visible, 18) > width - 82) {
            ++visible;
            while (((unsigned char)*visible & 0xc0) == 0x80) ++visible;
        }
        DrawText(visible, (int)x + 40, (int)y + 109, 18, text);
        if (GetTime() < app.message_until) {
            BeginScissorMode((int)x + 28, (int)y + 146, (int)width - 56, 22);
            DrawText(app.message, (int)x + 28, (int)y + 148, 13, accent);
            EndScissorMode();
        } else DrawText("Ctrl/Cmd + A clears the path; Ctrl/Cmd + V pastes a path.",
                        (int)x + 28, (int)y + 148, 12, muted);
        if (button((Rectangle){x + width - 214, y + height - 58, 88, 34}, "Cancel", false, true) ||
            IsKeyPressed(KEY_ESCAPE)) app.modal = MODAL_NONE;
        if (button((Rectangle){x + width - 116, y + height - 58, 88, 34}, "Confirm", true, true) ||
            IsKeyPressed(KEY_ENTER)) {
            if (app.modal != MODAL_NONE) submit_path();
        }
    } else {
        DrawText("This board has unsaved edits. Discard them to continue?",
                 (int)x + 28, (int)y + 82, 17, muted);
        DrawText("Cancel, then Save to keep your work.", (int)x + 28, (int)y + 114, 15, muted);
        if (button((Rectangle){x + width - 238, y + height - 58, 88, 34}, "Cancel", false, true) ||
            IsKeyPressed(KEY_ESCAPE)) app.modal = MODAL_NONE;
        if (button((Rectangle){x + width - 140, y + height - 58, 112, 34}, "Discard", true, true)) {
            Modal action = app.modal;
            app.modal = MODAL_NONE;
            if (action == MODAL_NEW_CONFIRM) new_board();
            if (action == MODAL_OPEN_CONFIRM) open_modal(MODAL_OPEN);
            if (action == MODAL_QUIT_CONFIRM) app.running = false;
        }
    }
}

int main(int argc, char **argv)
{
    bool smoke = argc > 1 && strcmp(argv[1], "--smoke-test") == 0;
    bool help = argc == 2 && strcmp(argv[1], "--help") == 0;
    if (argc == 2 && strcmp(argv[1], "--version") == 0) {
        puts("PCBEE " PCBEE_VERSION);
        return 0;
    }
    bool invalid = argc > (smoke ? 3 : 2) ||
                   (argc > 1 && !smoke && !help && argv[1][0] == '-');
    if (help || invalid) {
        puts("Usage: pcbee [project.pcb]\n       pcbee --smoke-test [project.pcb]\n"
             "       pcbee --help | --version\nSee README.md for controls and build instructions.");
        return invalid ? 1 : 0;
    }
    const char *project = smoke ? (argc == 3 ? argv[2] : NULL) : (argc == 2 ? argv[1] : NULL);
    pcb_init(&app.board);
    if (project && strlen(project) >= sizeof(app.path)) {
        fputs("Project path is too long.\n", stderr);
        return 1;
    }
    default_path();
    if (project) {
        snprintf(app.path, sizeof(app.path), "%s", project);
        char error[256];
        if (!pcb_load(&app.board, app.path, error, sizeof(error))) {
            fprintf(stderr, "Cannot open %s: %s\n", app.path, error);
            return 1;
        }
    }
    app.saved = app.board;
    pcb_history_init(&app.history, &app.board);
    clear_selection();
    app.trace_width = 0.6f;
    app.running = true;
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT);
    InitWindow(1280, 820, "PCBEE - PCB layout editor");
    if (!IsWindowReady()) return 1;
    SetWindowMinSize(1060, 720);
    SetExitKey(KEY_NULL);
    SetTargetFPS(60);
    fit_board();
    int frames = 0;
    while (app.running) {
        if (WindowShouldClose()) {
            finish_interaction();
            if (dirty()) open_modal(MODAL_QUIT_CONFIRM);
            else app.running = false;
        }
        if (!app.running) break;
        if (IsWindowResized()) {
            Rectangle area = canvas();
            app.camera.offset = (Vector2){area.x + area.width / 2, area.y + area.height / 2};
        }
        if (app.modal == MODAL_NONE) {
            keyboard();
            if (app.modal == MODAL_NONE) update_canvas();
        }
        update_lesson();
        BeginDrawing();
        ClearBackground(background);
        draw_board();
        draw_toolbar();
        draw_sidebar();
        draw_lesson_panel();
        draw_status();
        draw_modal();
        EndDrawing();
        if (smoke && ++frames >= 3) app.running = false;
    }
    CloseWindow();
    if (smoke && frames < 3) return 1;
    if (smoke) puts("PCBEE desktop smoke test passed (3 rendered frames).");
    return 0;
}
