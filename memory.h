#ifndef __MEMORY_H__
#define __MEMORY_H__

#include "regs.h"
#define MEMORY_SIZE (256 * (1 << 20))

extern void memory_module_init(cpu_state_t *state);

#endif
