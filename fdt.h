#ifndef __FDT_H__
#define __FDT_H__

#include <stdint.h>
#include "regs.h"

extern int build_fdt(cpu_state_t *state, uint8_t *dst, uint64_t kernel_start, uint64_t kernel_size, const char *cmd_line);

#endif
