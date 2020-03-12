#ifndef __CPU_H__
#define __CPU_H__

#include "regs.h"

typedef struct cpu
{
  cpu_state_t *state;
} cpu_t;

#endif
