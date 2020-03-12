#ifndef __REGS_H__
#define __REGS_H__

#include <stdint.h>

#define XLEN 32

#if XLEN == 32
typedef int32_t  int_t;
typedef uint32_t uint_t;
#else
typedef int64_t  int_t;
typedef uint64_t uint_t;
#endif

typedef struct
{
  uint_t regs[32];
  uint_t pc;
  
  uint8_t xlen;
  
  uint_t mstatus;
  uint_t mtvec;
  uint_t mscratch;
  uint_t mepc;
  uint_t mcause;
  uint_t mtval;
  uint_t mhartio;

  uint32_t misa;
  uint32_t mie;
  uint32_t mip;
  uint32_t medeleg;
  uint32_t mideleg;
  uint32_t mcounteren;

  uint_t stvec;
  uint_t sscratch;
  uint_t sepc;
  uint_t scause;
  uint_t stval;

  uint32_t priv;

#if XLEN == 32
  uint32_t satp;
#else 
  uint64_t satp;
#endif
  uint32_t scounteren;
} cpu_state_t;

extern cpu_state_t cpu_state;
#endif
