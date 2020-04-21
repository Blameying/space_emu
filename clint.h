#ifndef __CLINT__
#define __CLINT__
#include "regs.h"

extern void clint_module_init(cpu_state_t *state);
extern uint64_t rtc_get_time(cpu_state_t *state);
#endif
