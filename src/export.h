#ifndef EXPORT_H
#define EXPORT_H

#include "pcb.h"

/* Writes seven fabrication layers as <prefix>-F_Cu.gbr etc. No electrical DRC. */
bool pcb_export_gerber(const PcbBoard *board, const char *prefix, char *error, size_t error_size);

#endif
