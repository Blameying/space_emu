#ifndef __PLIC_H__
#define __PLIC_H__
#include "regs.h"

extern void plic_module_init(cpu_state_t *state);
extern void plic_set_irq(cpu_state_t *state, int irq_num, int flag);
#endif
