#ifndef LEARN_H
#define LEARN_H

#include "pcb.h"

#define LEARN_PARTS 3
#define LEARN_STEPS 8

/* Fixed targets keep the first lesson focused on placement and connectivity. */
extern const PcbComponent learn_targets[LEARN_PARTS];
extern const char *const learn_net_names[3];
void learn_net_ends(int net, PcbPoint *a, PcbPoint *b);
bool learn_placed(const PcbBoard *board, int part);
/* Steps 1..3: placement, 4..6: routes, 7: final review. No board mutation. */
bool learn_check(const PcbBoard *board, int step, char *feedback, size_t size);

#endif
