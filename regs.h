#ifndef __REGS_H__
#define __REGS_H__

#include <stdint.h>
#include "riscv_definations.h"

typedef struct cpu_state cpu_state_t;
typedef void set_irq_func(cpu_state_t* state, int irq_num, int flag);

#pragma pack(push)
#pragma pack(1)
typedef struct
{
  set_irq_func *set_irq;
  cpu_state_t *opaque;
  int irq_num;
} irq_signal_t;
#pragma pack(pop)

static inline void set_irq(irq_signal_t *irq, int level)
{
  irq->set_irq(irq->opaque, irq->irq_num, level);
}

static inline void irq_init(irq_signal_t *irq, set_irq_func *set_irq, cpu_state_t *opaque, int irq_num)
{
    irq->set_irq = set_irq;
    irq->opaque = opaque;
    irq->irq_num = irq_num;
}

#pragma pack(push)
#pragma pack(1)

struct cpu_state
{
  uint_t pc;
  uint_t regs[32];

#if FLEN > 0
  fp_uint fp_reg[32];
  uint32_t fflags; /* fcsr [0:4] */
  uint8_t  frm;    /* fcsr [5:7] */
#endif
  
  uint8_t xlen;
  uint8_t power_down_flag;
  uint8_t priv;
  uint8_t fs; /* mstatus [12:13] 2 bits FS field */
  uint8_t mxl;
  uint64_t cycles;
  /* clint */
  uint64_t rtc_start_time;
  uint64_t mtimecmp;
  /* plic */
  uint32_t plic_pending_irq;
  uint32_t plic_served_irq;
  irq_signal_t plic_irq[32];
  /* htif */
  uint64_t htif_tohost;
  uint64_t htif_fromhost;

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

  uint_t load_res;
};

#pragma pack(pop)

extern cpu_state_t cpu_state;

inline cpu_state_t *get_cur_cpu_state()
{
  return &cpu_state;
}

extern int get_base_from_xlen(int xlen);
extern int csr_read(cpu_state_t *state, uint_t *pval, uint32_t csr, bool will_write);
extern int csr_write(cpu_state_t *state, uint32_t csr, uint_t val);
extern void set_priv(cpu_state_t *state,int priv);
extern void raise_exception(cpu_state_t *state, uint32_t cause, uint_t tval);
extern void handle_sret(cpu_state_t *state);
extern void handle_mret(cpu_state_t *state);
extern int raise_interrupt(cpu_state_t *state);
extern int machine_get_sleep_duration(cpu_state_t *state, int delay);
static inline void set_mip(cpu_state_t* state, uint32_t mask)
{
  state->mip |= mask;
  if (state->power_down_flag && ((state->mip & state->mie) != 0))
    state->power_down_flag = false;
}

static inline void reset_mip(cpu_state_t *state, uint32_t mask)
{
  state->mip &= ~mask;
}

static inline uint32_t get_mip(cpu_state_t *state)
{
  return state->mip;
}
#endif
