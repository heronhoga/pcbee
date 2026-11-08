#include "learn.h"
#include "i18n.h"

#include <math.h>
#include <stdio.h>

const PcbComponent learn_targets[LEARN_PARTS] = {
    {1, PCB_HEADER, {12.7f, 22.86f}, 90},
    {2, PCB_RESISTOR, {30.48f, 12.7f}, 0},
    {3, PCB_LED, {48.26f, 22.86f}, 270}
};
const char *const learn_net_names[3] = {"+5 V", "LED drive", "GND"};
/* Header 1..4, resistor 1..2, LED K/A. Unused header pads are separate nets. */
static const int pad_nets[8] = {0, 2, 3, 4, 0, 1, 2, 1};
static const int net_pads[3][2] = {{0, 4}, {5, 7}, {6, 1}};

static PcbPoint lesson_pad(int pad)
{
    int part = pad < 4 ? 0 : pad < 6 ? 1 : 2;
    return pcb_pad_position(&learn_targets[part], pad - (part == 0 ? 0 : part == 1 ? 4 : 6));
}

void learn_net_ends(int net, PcbPoint *a, PcbPoint *b)
{
    if (net < 0 || net >= 3) return;
    *a = lesson_pad(net_pads[net][0]);
    *b = lesson_pad(net_pads[net][1]);
}

bool learn_placed(const PcbBoard *board, int part)
{
    if (part < 0 || part >= LEARN_PARTS) return false;
    const PcbComponent *target = &learn_targets[part];
    int matches = 0;
    bool placed = false;
    for (int i = 0; i < board->component_count; ++i) {
        const PcbComponent *c = &board->components[i];
        if (c->type != target->type) continue;
        ++matches;
        placed = pcb_distance(c->position, target->position) < 0.01f &&
                 c->rotation == target->rotation;
    }
    return matches == 1 && placed;
}

static float cross(PcbPoint a, PcbPoint b, PcbPoint c)
{
    return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
}

static float segment_gap(PcbPoint a, PcbPoint b, PcbPoint c, PcbPoint d)
{
    float ab_c = cross(a, b, c), ab_d = cross(a, b, d);
    float cd_a = cross(c, d, a), cd_b = cross(c, d, b);
    if (((ab_c < 0 && ab_d > 0) || (ab_c > 0 && ab_d < 0)) &&
        ((cd_a < 0 && cd_b > 0) || (cd_a > 0 && cd_b < 0))) return 0;
    return fminf(fminf(pcb_segment_distance(a, c, d), pcb_segment_distance(b, c, d)),
                 fminf(pcb_segment_distance(c, a, b), pcb_segment_distance(d, a, b)));
}

static bool pad_touches(int pad, const PcbTrace *trace)
{
    PcbPoint p = lesson_pad(pad);
    if (pad != 0 && pad != 4 && pad != 6)
        return pcb_segment_distance(p, trace->a, trace->b) <= 0.9f + trace->width / 2 + 0.0001f;
    /* First pads are 1.8 mm squares. All lesson rotations are multiples of 90. */
    PcbPoint corners[4] = {{p.x - 0.9f, p.y - 0.9f}, {p.x + 0.9f, p.y - 0.9f},
                           {p.x + 0.9f, p.y + 0.9f}, {p.x - 0.9f, p.y + 0.9f}};
    if ((fabsf(trace->a.x - p.x) <= 0.9f && fabsf(trace->a.y - p.y) <= 0.9f) ||
        (fabsf(trace->b.x - p.x) <= 0.9f && fabsf(trace->b.y - p.y) <= 0.9f)) return true;
    for (int i = 0; i < 4; ++i)
        if (segment_gap(trace->a, trace->b, corners[i], corners[(i + 1) % 4]) <=
            trace->width / 2 + 0.0001f) return true;
    return false;
}

static int root(int *parents, int node)
{
    while (parents[node] != node) {
        parents[node] = parents[parents[node]];
        node = parents[node];
    }
    return node;
}

static void join(int *parents, int a, int b)
{
    parents[root(parents, a)] = root(parents, b);
}

bool learn_check(const PcbBoard *board, int step, char *feedback, size_t size)
{
    if (step < 1 || step >= LEARN_STEPS) {
        snprintf(feedback, size, "%s", tr("Answer the resistor question first."));
        return false;
    }
    int required = step < 3 ? step : 3;
    for (int i = 0; i < required; ++i) {
        if (!learn_placed(board, i)) {
            snprintf(feedback, size, tr("Place exactly one %s on its target at %.2f, %.2f mm, rotated %d degrees. Use Select to move it; R rotates."),
                     tr(pcb_footprint(learn_targets[i].type)->name), learn_targets[i].position.x,
                     learn_targets[i].position.y, learn_targets[i].rotation);
            return false;
        }
    }
    if (step <= 3) {
        snprintf(feedback, size, "%s", tr("Placement checked! The square pad marks pad 1. Continue when ready."));
        return true;
    }
    if (board->component_count != LEARN_PARTS) {
        snprintf(feedback, size, "%s", tr("Remove extra components. This lesson uses one header, one resistor, and one LED."));
        return false;
    }
    /* Each copper shape is a node; touching shapes share a connected set.
       This checker is only for the fixed lesson, not a general PCB DRC. */
    int parents[8 + PCB_MAX_TRACES];
    for (int i = 0; i < 8 + board->trace_count; ++i) parents[i] = i;
    for (int i = 0; i < board->trace_count; ++i) {
        const PcbTrace *t = &board->traces[i];
        for (int p = 0; p < 8; ++p)
            if (pad_touches(p, t)) join(parents, p, 8 + i);
        for (int j = 0; j < i; ++j) {
            const PcbTrace *u = &board->traces[j];
            if (segment_gap(t->a, t->b, u->a, u->b) <= (t->width + u->width) / 2 + 0.0001f)
                join(parents, 8 + i, 8 + j);
        }
    }
    for (int a = 0; a < 8; ++a) {
        for (int b = a + 1; b < 8; ++b) {
            if (pad_nets[a] != pad_nets[b] && root(parents, a) == root(parents, b)) {
                snprintf(feedback, size, "%s", tr("Unintended copper connection! Separate the three nets and keep header pads 3/4 isolated. Check crossings and pad edges."));
                return false;
            }
        }
    }
    int nets = step < 7 ? step - 3 : 3;
    for (int i = 0; i < nets; ++i) {
        if (root(parents, net_pads[i][0]) != root(parents, net_pads[i][1])) {
            snprintf(feedback, size, tr("%s is incomplete. Join its highlighted pads with continuous copper. Check gaps and finish each route with Enter."), tr(learn_net_names[i]));
            return false;
        }
    }
    if (step == 7) {
        for (int i = 0; i < board->trace_count; ++i) {
            bool attached = false;
            for (int p = 0; p < 3; ++p)
                attached = attached || root(parents, 8 + i) == root(parents, net_pads[p][0]);
            if (!attached) {
                snprintf(feedback, size, "%s", tr("Remove stray copper: every trace must belong to one of the three intended nets."));
                return false;
            }
        }
    }
    snprintf(feedback, size, "%s", tr(step == 7 ?
             "Lesson complete! All three connections are present and separate. Save your practice board. Manufacturing still needs a full CAD review." :
             "Connection checked! Copper is continuous and the lesson nets stay separate. Continue when ready."));
    return true;
}
