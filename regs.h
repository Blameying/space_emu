#ifndef __REGS_H__
#define __REGS_H__

#include <stdint.h>
#include "riscv_definations.h"


typedef struct
{
  uint_t pc;
  uint_t regs[32];
  
  uint8_t xlen;
  uint8_t power_down_flag;
  uint8_t priv;
  uint64_t cycles;

  int pending_exception;
  uint_t pending_tval;
  
  uint_t mstatus;
  uint_t mtvec;
  uint_t mscratch;
  uint_t mepc;
  uint_t mcause;
  uint_t mtval;
  uint_t mhartid;

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

#if XLEN == 32
  uint32_t satp;
#else 
  uint64_t satp;
#endif
  uint32_t scounteren;
} cpu_state_t;

extern cpu_state_t cpu_state;
extern int csr_read(cpu_state_t *state, uint_t *pval, uint32_t csr, bool will_write);
extern int csr_write(cpu_state_t *state, uint32_t csr, uint_t val);
extern void set_priv(cpu_state_t *state,int priv)
extern void raise_exception(cpu_state_t *state, uint32_t cause, uint_t tval)
extern void handle_sret(cpu_state_t *state)
extern void handle_mret(cpu_state_t *state)
#endif
