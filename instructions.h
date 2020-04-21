#ifndef __INSTRUCTIONS_H__
#define __INSTRUCTIONS_H__

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

extern void machine_loop();
#endif
