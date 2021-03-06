#ifndef __INSTRUCTIONS_H__
#define __INSTRUCTIONS_H__

#include <stdint.h>
#include "regs.h"

#define OP_REG 0x33
#define OP_IMM 0x13
#define OP_STORE 0x23
#define OP_LOAD 0x03
#define OP_AUIPC 0x17
#define OP_LUI 0x37
#define OP_JAL 0x6F
#define OP_SYSTEM 0x73
#define OP_JALR 0x67
#define OP_IMM32 0x1B
#define OP_32 0x3B
#define OP_BRANCH 0x63
#define OP_FENCE 0x0F
#define OP_AMO 0x2F
#define OP_FP_LOAD 0x07
#define OP_FP_STORE 0x27
#define OP_FP_FMADD 0x43
#define OP_FP_FMSUB 0x47
#define OP_FP_FNMSUB 0x4b
#define OP_FP_FNMADD 0x4f
#define OP_FP_NORMAL 0x53

extern uint32_t fetch_inst(cpu_state_t *state, int *f);
extern void decode_debug(cpu_state_t *state, uint32_t inst);
extern void machine_loop();
#endif
