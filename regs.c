#include "regs.h"
#include "cpu.h"
#include "riscv_definations.h"

cpu_state_t cpu_state;

#define SSTATUS_MASK0 (MSTATUS_UIE | MSTATUS_SIE |       \
                      MSTATUS_UPIE | MSTATUS_SPIE |     \
                      MSTATUS_SPP | \
                      MSTATUS_FS | MSTATUS_XS | \
                      MSTATUS_SUM | MSTATUS_MXR)

#define MSTATUS_MASK (MSTATUS_UIE | MSTATUS_SIE | MSTATUS_MIE |      \
                      MSTATUS_UPIE | MSTATUS_SPIE | MSTATUS_MPIE |    \
                      MSTATUS_SPP | MSTATUS_MPP | \
                      MSTATUS_FS | \
                      MSTATUS_MPRV | MSTATUS_SUM | MSTATUS_MXR)
/* cycle and insn counters */
#define COUNTEREN_MASK ((1 << 0) | (1 << 2))

#if XLEN >= 64
#define SSTATUS_MASK (SSTATUS_MASK0 | MSTATUS_UXL_MASK)
#else
#define SSTATUS_MASK SSTATUS_MASK0
#endif

int get_base_from_xlen(int xlen)
{
    if (xlen == 32)
        return 1;
    else if (xlen == 64)
        return 2;
    else
        return 3;
}

uint_t get_sstatus(cpu_state_t *state, uint_t mask)
{
  uint_t result = 0;
  result = state & SSTATUS_MASK & mask;
  bool sd = ((result & MSTATUS_FS) == MSTATUS_FS) |
       ((result & MSTATUS_XS) == MSTATUS_XS);
  if (sd)
    result |= (uint_t)1 << (state->xlen - 1);
  return result;
}

uint_t get_mstatus(cpu_state_t *state, uint_t mask)
{
  uint_t result = 0;
  result = state & mask;
  bool sd = ((result & MSTATUS_FS) == MSTATUS_FS) |
       ((result & MSTATUS_XS) == MSTATUS_XS);
  if (sd)
    result |= (uint_t)1 << (state->xlen - 1);
  return result;
}

void set_mstatus(cpu_state_t *state, uint_t value)
{
  uint_t diff = 0, mask = 0;
  diff = state->mstatus ^ value;
  if ((diff & (MSTATUS_MPRV | MSTATUS_SUM | MSTATUS_MXR)) != 0 ||
      ((state->mstatus & MSTATUS_MPRV) && (diff & MSTATUS_MPP) != 0))
  {
    //TODO: tlb flush
    void(diff);
  }
  mask = MSTATUS_MASK & ~MSTATUS_FS;
#if XLEN >= 64
  {
    int uxl, sxl;
    uxl = (val >> MSTATUS_UXL_SHIFT) & 3;
    if (uxl >= 1 && uxl <= get_base_from_xlen(XLEN))
        mask |= MSTATUS_UXL_MASK;
    sxl = (val >> MSTATUS_UXL_SHIFT) & 3;
    if (sxl >= 1 && sxl <= get_base_from_xlen(XLEN))
        mask |= MSTATUS_SXL_MASK;
  }
#endif
  state->mstatus = (state->mstatus & ~mask) | (value & mask) 
}


int csr_read(cpu_state_t *state, uint_t *pval, uint32_t csr, bool will_write)
{
  uint_t val = 0;
  if (((csr & 0xC00) == 0xC00) && will_write)
  {
    // read-only regs, add will write to save time for next write method
    return -1;
  }
  if (state->priv < ((csr >> 8) & 3))
    // check current privilage
    return -1;

  switch (csr)
  {
    case 0xC00: /* ucycle */
    case 0xC02: /* uinstret */
      {
        uint32_t counteren;
        if (state->priv < PRIV_M)
        {
          if (state->priv < PRIV_S)
          {
            counteren = state->scounteren;
          }
          else
          {
            counteren = state->mcounteren;
          }
        }
        if (((counteren >> (csr & 0xF)) & 0x1) == 0)
          goto illegal_instruction;
      }
      val = (int64_t)state->cycles;
      break;
    case 0xC80: /* mcycleh */
    case 0xC82: /* minstreth */
      if (state->xlen != 32)
        goto illegal_instruction;
      {
        uint32_t counteren;
        if (state->priv < PRIV_M)
        {
          if (state->priv < PRIV_S)
          {
            counteren = state->scounteren;
          }
          else
          {
            counteren = state->mcounteren;
          }
        }
        if (((counteren >> (csr & 0xF)) & 0x1) == 0)
          goto illegal_instruction;
      }
      val = state->cycles >> 32;
      break;

    case 0x100: /* sstatus */
      val = get_sstatus(state, (uint_t)-1);
      break;
    case 0x104: /* sie */
      val = state->mie & state->mideleg;
      break;
    case 0x105: /* stvec */
      val = state->stvec;
      break;
    case 0x106: /* scounteren */
      val = state->scounteren;
      break;
    case 0x140: /* sscratch */
      val = state->sscratch;
      break;
    case 0x141: /* sepc */
      val = state->sepc;
      break;
    case 0x142: /* scause */
      val = state->scause;
      break;
    case 0x143: /* stval */
      val = state->stval;
      break;
    case 0x144: /* sip */
      val = s->mip & s->mideleg;
      break;
    case 0x180: /* satp */
      val = s->satp;
      break;
    case 0x300: /* mstatus */
      val = get_mstatus(state, (uint_t)-1);
      break;
    case 0x301: /* misa */
      val = state->misa;
      break;
    case 0x302: /* medeleg */
      val = state->medeleg;
      break;
    case 0x303: /* mideleg */
      val = state->mideleg;
      break;
    case 0x304: /* mie */
      val = state->mie;
      break;
    case 0x305: /* mtvec */
      val = state->mtvec;
      break;
    case 0x306: /* mcounteren */
      val = state->mcounteren;
      break;
    case 0x340: /* mscratch */
      val = state->mscratch;
      break;
    case 0x341: /* mepc */
      val = state->mepc;
      break;
    case 0x342: /* mcause */
      val = state->mcause;
      break;
    case 0x343: /* mtval */
      val = state->mtval;
      break;
    case 0x344: /* mip */
      val = state->mip;
      break;
    case 0xb00: /* mcycle */
    case 0xb02: /* minstret */
      val = (int64_t)state->cycles;
      break;
    case 0xb80: /* mcycleh */
    case 0xb82: /* minstreth */
      if (state->xlen != 32)
          goto illegal_instruction;
      val = state->cycles >> 32;
      break;
    case 0xf14: /* mhartid */
      val = state->mhartid;
      break;
    default:
      illegal_instruction:
      *pval = 0;
      return -1;
  }
  *pval = val;
  return 0;
}

int csr_write(cpu_state_t *state, uint32_t csr, uint_t val)
{
  uint_t mask = 0;
  switch(csr)
  {
    case 0x100: /* sstatus */
      set_mstatus(state, (state->mstatus & ~SSTATUS_MASK) | (val & SSTATUS_MASK));
      break;
    case 0x104: /* sie */
      mask = state->mideleg;
      state->mie = (s->mie & ~mask) | (val & mask);
      break;
    case 105:   /* stvec */
      state->stvec = val & ~3;
      break;
    case 0x106: /* scounteren */
      state->scounteren = val & COUNTEREN_MASK;
      break;
    case 0x140: /* sscratch */
      state->sscratch = val;
      break;
    case 0x141: /* sepc */
      state->sepc = val & ~1;
      break;
    case 0x142: /* scause */
      state->scause = val;
      break;
    case 0x143: /* stval */
      state->stval = val;
      break;
    case 0x144: /* sip */
      mask = s->mideleg;
      state->mip = (state->mip & ~mask) | (val & mask);
      break;
    case 0x180: /* satp */
      /* no ASID implemented */
#if XLEN == 32
      {
          int new_mode;
          new_mode = (val >> 31) & 1;
          state->satp = (val & (((uint_t)1 << 22) - 1)) |
              (new_mode << 31);
      }
#else
      {
          int mode, new_mode;
          mode = state->satp >> 60;
          new_mode = (val >> 60) & 0xf;
          if (new_mode == 0 || (new_mode >= 8 && new_mode <= 9))
              mode = new_mode;
          state->satp = (val & (((uint64_t)1 << 44) - 1)) |
              ((uint64_t)mode << 60);
      }
#endif
        //TODO: tlb flush
        return 2;
    case 0x300: /* mstatus */
        set_mstatus(state, val);
        break;
    case 0x301: /* misa */
#if XLEN >= 64
      {
        int new_mxl;
        new_mxl = (val >> (s->xlen - 2)) & 3;
        if (new_mxl >= 1 && new_mxl <= get_base_from_xlen(XLEN)) 
        {
          /* Note: misa is only modified in M level, so cur_xlen
             = 2^(mxl + 4) */
          if (state->mxl != new_mxl) 
          {
              state->mxl = new_mxl;
              state->xlen = 1 << (new_mxl + 4);
              return 1;
          }
        }
      }
#endif
      break;
    case 0x302: /* medeleg */
      mask = (1 << (CAUSE_STORE_PAGE_FAULT + 1)) - 1;
      state->medeleg = (state->medeleg & ~mask) | (val & mask);
      break;
    case 0x303: /* mideleg */
      mask = (MIP_SSIP | MIP_STIP | MIP_SEIP);
      s->mideleg = (s->mideleg & ~mask) | (val & mask);
      break;
    case 0x304: /* mie */
      mask = MIP_MSIP | MIP_MTIP | MIP_SSIP | MIP_STIP | MIP_SEIP;
      state->mie = (state->mie & ~mask) | (val & mask);
      break;
    case 0x305: /* mtvec */
      state->mtvec = val & ~3;
      break;
    case 0x306: /* mcounteren */
      state->mcounteren = val & COUNTEREN_MASK;
      break;
    case 0x340: /* mscratch */
      state->mscratch = val;
      break;
    case 0x341: /* mepc */
      state->mepc = val & ~1;
      break;
    case 0x342: /* mcause */
      state->mcause = val;
      break;
    case 0x343: /* mtval */
      state->mtval = val;
      break;
    case 0x344: /* mip */
      mask = MIP_SSIP | MIP_STIP;
      state->mip = (s->mip & ~mask) | (val & mask);
      break;
    default:
      return -1;
  }
  return 0;;
}

void set_priv(cpu_state_t *state,int priv)
{
  if (state->priv != priv)
  {
    //TODO: tlb flush
#if XLEN >= 64
    {
      int mxl;
      if (priv == PRIV_S)
        mxl = (state->mstatus >> MSTATUS_SXL_SHIFT) & 3;
      else if (priv == PRIV_U)
        mxl = (state->mstatus >> MSTATUS_UXL_SHIFT) & 3;
      else
        mxl = get_base_from_xlen(XLEN);
      state->xlen = 1 << (4 + mxl);
    }
#endif
    state->priv = priv;
  }
}

void raise_exception(cpu_state_t *state, uint32_t cause, uint_t tval)
{
  bool deleg;
  uint_t causel;

  if (state->priv <= PRIV_S)
  {
    if (cause & CAUSE_INTERRUPT)
      deleg = (state->mideleg >> (cause & (XLEN - 1))) & 1;
    else
      deleg = (state->medeleg >> cause) & 1;
  }
  else
  {
    deleg = 0;
  }
  causel = cause & 0x7FFFFFFF;
  if (cause & CAUSE_INTERRUPT)
    causel |= (uint_t)1 << (state->xlen - 1);

  if (deleg)
  {
    state->scause = causel;
    state->sepc = state->pc;
    state->stval = tval;
    state->mstatus = (state->mstatus & ~MSTATUS_SPIE) |
      (((state->mstatus >> state->priv) & 1) << MSTATUS_SPIE_SHIFT);
    state->mstatus = (state->mstatus & ~MSTATUS_SPP) |
      (state->priv << MSTATUS_SPP_SHIFT);
    state->mstatus &= ~MSTATUS_SIE;
    set_priv(state, PRIV_S);
    state->pc = state->stvec;
  }
  else
  {
    state->mcause = causel;
    state->mepc = state->pc;
    state->mtval = tval;
    state->mstatus = (state->mstatus & ~MSTATUS_MPIE) |
      (((state->mstatus >> state->priv) & 1) << MSTATUS_MPIE_SHIFT);
    state->mstatus = (state->mstatus & ~MSTATUS_MPP) |
      (state->priv << MSTATUS_MPP_SHIFT);
    state->mstatus &= ~MSTATUS_MIE;
    set_priv(state, PRIV_M);
    state->pc = state->mtvec;
  }
}

void handle_sret(cpu_state_t *state)
{
  int spp, spie;
  spp = (state->mstatus >> MSTATUS_SPP_SHIFT) & 1;
  spie = (state->mstatus >> MSTATUS_SPIE_SHIFT) & 1;
  state->mstatus = (state->mstatus & ~(1 << spp)) | (spie << spp);
  state->mstatus |= MSTATUS_SPIE;
  state->mstatus &= ~MSTATUS_SPP;
  set_priv(state, spp);
  state->pc = state->sepc;
}

void handle_mret(cpu_state_t *state)
{
  int mpp, mpie;
  mpp = (state->mstatus >> MSTATUS_MPP_SHIFT) & 1;
  mpie = (state->mstatus >> MSTATUS_MPIE_SHIFT) & 1;
  state->mstatus = (state->mstatus & ~(1 << mpp)) | (mpie << mpp);
  state->mstatus |= MSTATUS_MPIE;
  state->mstatus &= ~MSTATUS_MPP;
  set_priv(state, mpp);
  state->pc = state->mepc;
}


