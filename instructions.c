#include <stdio.h>
#include "instructions.h"
#include "regs.h"
#include "iomap.h"
#include <stdlib.h>
#include <sys/select.h>
#include "machine.h"
#include "console.h"
#include "softfp.h"

#define MAX_DELAY_TIME 10
#define C_QUADRANT(n) \
    case n+(0 << 2): case n+(1 << 2): case n+(2 << 2): case n+(3 << 2): \
    case n+(4 << 2): case n+(5 << 2): case n+(6 << 2): case n+(7 << 2): \
    case n+(8 << 2): case n+(9 << 2): case n+(10 << 2): case n+(11 << 2): \
    case n+(12 << 2): case n+(13 << 2): case n+(14 << 2): case n+(15 << 2): \
    case n+(16 << 2): case n+(17 << 2): case n+(18 << 2): case n+(19 << 2): \
    case n+(20 << 2): case n+(21 << 2): case n+(22 << 2): case n+(23 << 2): \
    case n+(24 << 2): case n+(25 << 2): case n+(26 << 2): case n+(27 << 2): \
    case n+(28 << 2): case n+(29 << 2): case n+(30 << 2): case n+(31 << 2): 

#if XLEN == 32
static inline uint32_t mulh(int32_t a, int32_t b)
{
  return ((int64_t)a * (int64_t)b) >> 32;
}

static inline uint32_t mulhsu(int32_t a, uint32_t b)
{
  return ((int64_t)a * (int64_t)b) >> 32;
}

static inline uint32_t mulhu(uint32_t a, uint32_t b)
{
  return ((int64_t)a * (int64_t)b) >> 32;
}

#else

static uint_t mulhu(uint_t a, uint_t b)
{
  uint32_t a0, a1, b0, b1, r2, r3;
  uint_t r00, r01, r10, r11, c;
  a0 = a;
  a1 = a >> 32;
  b0 = b;
  b1 = b >> 32;

  /* matrix */
  r00 = (uint_t)a0 * (uint_t)b0;
  r01 = (uint_t)a0 * (uint_t)b1;
  r10 = (uint_t)a1 * (uint_t)b0;
  r11 = (uint_t)a1 * (uint_t)b1;

  c = (r00 >> 32) + (uint32_t)r01 + (uint32_t)r10;
  c = (c >> 32) + (r01 >> 32) + (r10 >> 32) + (uint32_t)r11;
  r2 = c;
  r3 = (c >> 32) + (r11 >> 32);

  return ((uint_t)r3 << 32) | r2;
}

static inline uint_t mulh(int_t a, int_t b)
{
  uint_t r1;
  r1 = mulhu(a, b);

  if (a < 0)
    r1 -= b;
  if (b < 0)
    r1 -= a;
  return r1;
}

static inline uint_t mulhsu(int_t a, uint_t b)
{
  uint_t r1;
  r1 = mulhu(a, b);
  if (a < 0)
    r1 -= b;
  return r1;
}

#endif

static inline int_t m_div(int_t a, int_t b)
{
  if (b == 0) 
  {
    return -1;
  }
  else if (a == ((int_t)1 << (XLEN - 1)) && b == -1)
  {
    return a;
  }
  else
  {
    return a / b;
  }
}

static inline uint_t m_divu(uint_t a, uint_t b)
{
  if (b == 0)
    return -1;
  else
    return a / b;
}

static inline int_t rem(int_t a, int_t b)
{
  if (b == 0)
  {
    return a;
  }
  else if (a == ((int_t)1 << (XLEN - 1)) && b == -1)
  {
    return 0;
  }
  else
  {
    return a % b;
  }
}

static inline uint_t remu(uint_t a, uint_t b)
{
  if (b == 0)
    return a;
  else
    return a % b;
}

static inline int32_t m_div32(int32_t a, int32_t b)
{
  if (b == 0) 
  {
    return -1;
  }
  else if (a == ((int32_t)1 << (32 - 1)) && b == -1)
  {
    return a;
  }
  else
  {
    return a / b;
  }
}

static inline uint32_t m_divu32(uint32_t a, uint32_t b)
{
  if (b == 0)
    return -1;
  else
    return a / b;
}

static inline int32_t rem32(int32_t a, int32_t b)
{
  if (b == 0)
  {
    return a;
  }
  else if (a == ((int32_t)1 << (32 - 1)) && b == -1)
  {
    return 0;
  }
  else
  {
    return a % b;
  }
}

static inline uint32_t remu32(uint32_t a, uint32_t b)
{
  if (b == 0)
    return a;
  else
    return a % b;
}

#if FLEN > 0
static inline int32_t get_inst_rm(cpu_state_t *state, int32_t rm)
{
  if (rm == 7)
    return state->frm;
  if (rm >= 5)
    return -1;
  else
    return rm;
}
#endif

uint32_t fetch_inst(cpu_state_t *state, int *f)
{
  uint32_t inst = 0;
  int flag = 0;
  flag = iomap_manager.code_vaddr(state, state->pc, sizeof(inst), (uint8_t*)&inst);
  *f = flag;
  if (flag < 0)
  {
    raise_exception(state, state->pending_exception, state->pending_tval);
    return 0;
  }
  return inst;
}

#if DEBUG
static inline void dump_cpu_info(cpu_state_t *s, uint32_t inst)
{
  #define DUMP_PRINT(a) printf(#a ": 0x%lx, \n", s->a)
  printf("{\n");
  printf("inst: 0x%lx, \n", inst);
  DUMP_PRINT(pc);
  DUMP_PRINT(priv);
  DUMP_PRINT(mxl);
  DUMP_PRINT(fflags);
  DUMP_PRINT(frm);
  DUMP_PRINT(mstatus);
  printf("cur_xlen: 0x%lx, \n", s->xlen);
  DUMP_PRINT(mtvec);
  DUMP_PRINT(mepc);
  DUMP_PRINT(mcause);
  DUMP_PRINT(mtval);
  DUMP_PRINT(misa);
  DUMP_PRINT(mie);
  DUMP_PRINT(mip);
  DUMP_PRINT(medeleg);
  DUMP_PRINT(mideleg);
  DUMP_PRINT(mcounteren);
  DUMP_PRINT(stvec);
  DUMP_PRINT(sscratch);
  DUMP_PRINT(sepc);
  DUMP_PRINT(scause);
  DUMP_PRINT(stval);
  DUMP_PRINT(satp);
  DUMP_PRINT(scounteren);
  for (int i = 0; i < 32; i++)
  {
    printf("reg[%d]: 0x%lx, ", i, s->regs[i]);
  }
  printf("\n}\n");
  #undef DUMP_PRINT
}
#else
static inline void dump_cpu_info(cpu_state_t *s, uint32_t inst)
{
  return;
}
#endif
void decode_debug(cpu_state_t *state, uint32_t inst)
{
  uint32_t opcode = inst & 0x7F;
  uint32_t rd = (inst >> 7) & 0x1F;
  uint32_t funct3 = (inst >> 12) & 0x07;
  uint32_t rs1 = (inst >> 15) & 0x1F;
  uint32_t rs2 = (inst >> 20) & 0x1F;
  uint32_t funct7 = (inst >> 25) & 0x7F;
  int32_t  imm_I = (int32_t)inst >> 20;
  int32_t  imm_S = ((int32_t)(rd | ((inst >> 20) & 0xFE0)) << 20) >> 20;
  int32_t  imm_B = ((int32_t)(((inst >> 7) & 0x1E) | ((inst >> 20) & 0x7E0) | ((inst << 4) & 0x800) | ((inst >> 19) & 0x1000)) << 19) >> 19;
  int32_t  imm_U = (int32_t)(inst & 0xFFFFF000);
  int32_t  imm_J = ((int32_t)(((inst >> 20) & 0x7FE) | ((inst >> 9) & 0x800) | (inst & 0xFF000) | ((inst >> 11) & 0x100000)) << 11) >> 11;
  switch(opcode)
  {
    C_QUADRANT(0)
      funct3 = (inst >> 13) & 7;
      rd = ((inst >> 2) & 7) | 8;
      state->pc += 2;
      break;
    C_QUADRANT(1)
      funct3 = (inst >> 13) & 7;
      state->pc += 2;
      break;
    C_QUADRANT(2)
      funct3 = (inst >> 13) & 7;
      rs2 = (inst >> 2) & 0x1F;
      state->pc += 2;
      break;
    default:
      state->pc += 4;
  }
  printf("opcode: 0x%x, rd: 0x%x, funct3: 0x%x\n", opcode, rd, funct3);
}

void decode_inst(uint32_t inst)
{
  uint32_t opcode = inst & 0x7F;
  uint32_t rd = (inst >> 7) & 0x1F;
  uint32_t funct3 = (inst >> 12) & 0x07;
  uint32_t rs1 = (inst >> 15) & 0x1F;
  uint32_t rs2 = (inst >> 20) & 0x1F;
  uint32_t funct7 = (inst >> 25) & 0x7F;
  int32_t  imm_I = (int32_t)inst >> 20;
  int32_t  imm_S = ((int32_t)(rd | ((inst >> 20) & 0xFE0)) << 20) >> 20;
  int32_t  imm_B = ((int32_t)(((inst >> 7) & 0x1E) | ((inst >> 20) & 0x7E0) | ((inst << 4) & 0x800) | ((inst >> 19) & 0x1000)) << 19) >> 19;
  int32_t  imm_U = (int32_t)(inst & 0xFFFFF000);
  int32_t  imm_J = ((int32_t)(((inst >> 20) & 0x7FE) | ((inst >> 9) & 0x800) | (inst & 0xFF000) | ((inst >> 11) & 0x100000)) << 11) >> 11;

#if FLEN > 0
  uint32_t rs3;
  int32_t  rm;
#endif

  dump_cpu_info(&cpu_state, inst);
  switch(opcode)
  {
    C_QUADRANT(0)
      funct3 = (inst >> 13) & 7;
      rd = ((inst >> 2) & 7) | 8;
      switch(funct3)
      {
        case 0: /* c.addi4spn */
          imm_I = ((inst >> 7) & 0x30) |
            ((inst >> 1) & 0x3C0) |
            ((inst >> 4) & 0x4) |
            ((inst >> 2) & 0x8);
          if (imm_I == 0)
            goto ERROR_PROCESS;
          cpu_state.regs[rd] = (int_t)(cpu_state.regs[2] + imm_I);
          break;
#if FLEN >= 64 
        case 1: /* c.fld */
          {
            uint64_t rval;
            if (cpu_state.fs == 0)
              goto ERROR_PROCESS;
            imm_I = ((inst >> 7) & 0x38) | ((inst << 1) & 0xC0);
            rs1 = ((inst >> 7) & 7) | 8;
            uint_t addr = (int_t)(cpu_state.regs[rs1] + imm_I);
            if (iomap_manager.read_vaddr(&cpu_state, addr, 8, (uint8_t*)&rval) < 0)
              goto MMU_EXCEPTION;
            cpu_state.fp_reg[rd] = rval | F64_HIGH;
            cpu_state.fs = 3;
          }
          break;
#endif 
        case 2: /* c.lw */
          {
            uint32_t rval;
            imm_I = ((inst >> 7) & 0x38) | ((inst >> 4) & 0x4) |
                    ((inst << 1) & 0x40);
            rs1 = ((inst >> 7) & 7) | 8; 
            uint_t addr = (int_t)(cpu_state.regs[rs1] + imm_I);
            if (iomap_manager.read_vaddr(&cpu_state, addr, 4, (uint8_t*)&rval) < 0)
              goto MMU_EXCEPTION;
            cpu_state.regs[rd] = (int32_t)rval;
          }
          break;
#if XLEN >= 64
        case 3: /* c.ld */
          {
            uint64_t rval;
            imm_I = ((inst >> 7) & 0x38) | ((inst << 1) & 0xC0);
            rs1 = ((inst >> 7) & 7) | 8;
            uint_t addr = (int_t)(cpu_state.regs[rs1] + imm_I);
            if (iomap_manager.read_vaddr(&cpu_state, addr, 8, (uint8_t*)&rval) < 0)
              goto MMU_EXCEPTION;
            cpu_state.regs[rd] = (int64_t)rval;

          }
          break;
#elif FLEN >= 32
        case 3: /* c.flw */
          {
            uint32_t rval;
            if (cpu_state.fs == 0)
              goto ERROR_PROCESS;
            imm_I = ((inst >> 7) & 0x38) | ((inst >> 4) & 0x4) |
                    ((inst << 1) & 0x40);
            rs1 = ((inst >> 7) & 7) | 8;
            uint_t addr = (int_t)(cpu_state.regs[rs1] + imm_I);
            if (iomap_manager.read_vaddr(&cpu_state, addr, 4, (uint8_t*)&rval) < 0)
              goto MMU_EXCEPTION;
            cpu_state.fp_reg[rd] = rval | F32_HIGH;
            cpu_state.fs = 3;
          }
          break;
#endif
#if FLEN >= 64
        case 5: /* c.fsd */
          {
            if (cpu_state.fs == 0)
              goto ERROR_PROCESS;
            imm_I = ((inst >> 7) & 0x38) | ((inst << 1) & 0xC0);
            rs1 = ((inst >> 7) & 7) | 8;
            uint_t addr = (int_t)(cpu_state.regs[rs1] + imm_I);
            if (iomap_manager.write_vaddr(&cpu_state, addr, 8, (uint8_t*)&cpu_state.fp_reg[rd]) < 0)
              goto MMU_EXCEPTION;
          }
          break;
#endif
        case 6: /* c.sw */
          {
            imm_I = ((inst >> 7) & 0x38) | ((inst >> 4) & 0x4) |
              ((inst << 1) & 0x40);
            rs1 = ((inst >> 7) & 7) | 8;
            uint_t addr = (int_t)(cpu_state.regs[rs1] + imm_I);
            uint_t val = cpu_state.regs[rd];
            if (iomap_manager.write_vaddr(&cpu_state, addr, 4, (uint8_t*)&val) < 0)
              goto MMU_EXCEPTION;
            break;
          }
          break;
#if XLEN >= 64
        case 7: /* c.sd */
          {
            imm_I = ((inst >> 7) & 0x38) | ((inst << 1) & 0xC0);
            rs1 = ((inst >> 7) & 7) | 8;
            uint_t addr = (int_t)(cpu_state.regs[rs1] + imm_I);
            uint_t val = cpu_state.regs[rd];
            if (iomap_manager.write_vaddr(&cpu_state, addr, 8, (uint8_t*)&val) < 0)
              goto MMU_EXCEPTION;
          }
          break;
#elif FLEN >= 32
        case 7: /* c.fsw */
          {
            if (cpu_state.fs == 0)
              goto ERROR_PROCESS;
            imm_I = ((inst >> 7) & 0x38) | ((inst >> 4) & 0x4) |
                    ((inst << 1) & 0x40);
            rs1 = ((inst >> 7) & 7) | 8;
            uint_t addr = (int_t)(cpu_state.regs[rs1] + imm_I);
            if (iomap_manager.write_vaddr(&cpu_state, addr, 4, (uint8_t*)&cpu_state.fp_reg[rd]) < 0)
              goto MMU_EXCEPTION;
          }
          break;
#endif
        default:
          goto ERROR_PROCESS;
      }
      goto C_NEXT_INSN;
    C_QUADRANT(1)
      funct3 = (inst >> 13) & 7;
      switch(funct3)
      {
        case 0: /* c.addi/c.nop */
          if (rd != 0)
          {
            imm_I = ((inst >> 7) & 0x20) | ((inst >> 2) & 0x1F);
            imm_I = (imm_I << 26) >> 26;
            cpu_state.regs[rd] = (int_t)(cpu_state.regs[rd] + imm_I);
          }
          break;
#if XLEN == 32
        case 1: /* c.jal */
          imm_I = ((inst >> 1) & 0x800) |
                  ((inst >> 7) & 0x10) |
                  ((inst >> 1) & 0x300) |
                  ((inst << 2) & 0x400) |
                  ((inst >> 1) & 0x40) |
                  ((inst << 1) & 0x80) |
                  ((inst >> 2) & 0xE) |
                  ((inst << 3) & 0x20);
          imm_I = (imm_I << 20) >> 20;
          cpu_state.regs[1] = cpu_state.pc + 2;
          cpu_state.pc = (int_t)(cpu_state.pc + imm_I);
          goto JUMP;
#else
        case 1: /* c.addiw */
          if (rd != 0)
          {
            imm_I = ((inst >> 7) & 0x20) |
                    ((inst >> 2) & 0x1F);
            imm_I = (imm_I << 26) >> 26;
            cpu_state.regs[rd] = (int32_t)(cpu_state.regs[rd] + imm_I);
          }
          break;
#endif
        case 2: /* c.li */
          if (rd != 0)
          {
            imm_I = ((inst >> 7) & 0x20) |
                    ((inst >> 2) & 0x1F);
            imm_I = (imm_I << 26) >> 26;
            cpu_state.regs[rd] = imm_I;
          }
          break;
        case 3:
          if (rd == 2)
          {
            /* c.addi16sp */
            imm_I = ((inst >> 3) & 0x200) |
                    ((inst >> 2) & 0x10) |
                    ((inst << 1) & 0x40) |
                    ((inst << 4) & 0x180) |
                    ((inst << 3) & 0x20);
            imm_I = (imm_I << 22) >> 22;
            if (imm_I == 0)
              goto ERROR_PROCESS;
            cpu_state.regs[2] = (int_t)(cpu_state.regs[2] + imm_I);
          }
          else if (rd != 0)
          {
            /* c.lui */
            imm_I = ((inst << 5) & 0x20000) |
                    ((inst << 10) & 0x1F000);
            imm_I = (imm_I << 14) >> 14;
            cpu_state.regs[rd] = imm_I;

          }
          break;
        case 4:
          funct3 = (inst >> 10) & 3;
          rd = ((inst >> 7) & 7) | 8;
          switch(funct3)
          {
            case 0: /* c.srli */
            case 1: /* c.srai */
              imm_I = ((inst >> 7) & 0x20) |
                      ((inst >> 2) & 0x1F);
#if XLEN == 32
              if (imm_I & 0x20)
                goto ERROR_PROCESS;
#endif
              if (funct3 == 0)
                cpu_state.regs[rd] = (int_t)((uint_t)cpu_state.regs[rd] >> imm_I);
              else
                cpu_state.regs[rd] = (int_t)cpu_state.regs[rd] >> imm_I;
              break;
            case 2: /* c.andi */
              imm_I = ((inst >> 7) & 0x20) |
                      ((inst >> 2) & 0x1F);
              imm_I = (imm_I << 26) >> 26;
              cpu_state.regs[rd] &= imm_I;
              break;
            case 3:
              rs2 = ((inst >> 2) & 7) | 8;
              funct3 = ((inst >> 5) & 3) | ((inst >> (12 - 2)) & 4);
              switch(funct3)
              {
                case 0: /* c.sub */
                  cpu_state.regs[rd] = (int_t)(cpu_state.regs[rd] - cpu_state.regs[rs2]);
                  break;
                case 1: /* c.xor */
                  cpu_state.regs[rd] = (cpu_state.regs[rd] ^ cpu_state.regs[rs2]);
                  break;
                case 2: /* c.or */
                  cpu_state.regs[rd] = cpu_state.regs[rd] | cpu_state.regs[rs2];
                  break;
                case 3: /* c.and */
                  cpu_state.regs[rd] = cpu_state.regs[rd] & cpu_state.regs[rs2];
                  break;
#if XLEN >= 64
                case 4: /* c.subw */
                  cpu_state.regs[rd] = (int32_t)(cpu_state.regs[rd] - cpu_state.regs[rs2]);
                    break;
                case 5: /* c.addw */
                  cpu_state.regs[rd] = (int32_t)(cpu_state.regs[rd] + cpu_state.regs[rs2]);
                  break;
#endif
                default:
                  goto ERROR_PROCESS;
              }
              break;
          }
          break;
        case 5: /* c.j */
          imm_I = ((inst >> 1) & 0x800) |
                  ((inst >> 7) & 0x10) |
                  ((inst >> 1) & 0x300) |
                  ((inst << 2) & 0x400) |
                  ((inst >> 1) & 0x40) |
                  ((inst << 1) & 0x80) |
                  ((inst >> 2) & 0x0E) |
                  ((inst << 3) & 0x20);
          imm_I = (imm_I << 20) >> 20;
          cpu_state.pc = (int_t)(cpu_state.pc + imm_I);
          goto JUMP;
        case 6: /* c.beqz */
          rs1 = ((inst >> 7) & 7) | 8;
          imm_I = ((inst >> 4) & 0x100) |
                  ((inst >> 7) & 0x18) |
                  ((inst << 1) & 0xC0) |
                  ((inst >> 2) & 0x06) |
                  ((inst << 3) & 0x20);
          imm_I = (imm_I << 23) >> 23;
          if (cpu_state.regs[rs1] == 0)
          {
            cpu_state.pc = (int_t)(cpu_state.pc + imm_I);
            goto JUMP;
          }
          break;
        case 7: /* c.bnez */
          rs1 = ((inst >> 7) & 7) | 8;
          imm_I = ((inst >> 4) & 0x100) |
                  ((inst >> 7) & 0x18) |
                  ((inst << 1) & 0xC0) |
                  ((inst >> 2) & 0x06) |
                  ((inst << 3) & 0x20);
          imm_I = (imm_I << 23) >> 23;
          if (cpu_state.regs[rs1] != 0)
          {
            cpu_state.pc = (int_t)(cpu_state.pc + imm_I);
            goto JUMP;
          }
          break;
        default:
          goto ERROR_PROCESS;
      }
      goto C_NEXT_INSN;
    C_QUADRANT(2)
      funct3 = (inst >> 13) & 7;
      rs2 = (inst >> 2) & 0x1F;
      switch(funct3)
      {
        case 0: /* c.slli */
          imm_I = ((inst >> 7) & 0x20) | rs2;
#if XLEN == 32
          if (imm_I & 0x20)
            goto ERROR_PROCESS;
#endif
          if (rd != 0)
            cpu_state.regs[rd] = (int_t)(cpu_state.regs[rd] << imm_I);
          break;
#if FLEN >= 64
        case 1: /* c.fldsp */
          {
            uint64_t rval;
            if (cpu_state.fs == 0)
              goto ERROR_PROCESS;
            imm_I = ((inst >> 7) & 0x20) |
                    (rs2 & (3 << 3)) |
                    ((inst << 4) & 0x1C0);
            uint_t addr = (int_t)(cpu_state.regs[2] + imm_I);
            if (iomap_manager.read_vaddr(&cpu_state, addr, 8, (uint8_t*)&rval) < 0)
              goto MMU_EXCEPTION;
            cpu_state.fp_reg[rd] = rval | F64_HIGH;
            cpu_state.fs = 3;
          }
          break;
#endif
        case 2: /* c.lwsp */
          {
            uint32_t rval;
            imm_I = ((inst >> 7) & 0x20) |
                    (rs2 & (7 << 2)) |
                    ((inst << 4) & 0xC0);
            uint_t addr = (int_t)(cpu_state.regs[2] + imm_I);
            if (iomap_manager.read_vaddr(&cpu_state, addr, 4, (uint8_t*)&rval) < 0)
              goto MMU_EXCEPTION;
            if (rd != 0)
              cpu_state.regs[rd] = (int32_t)rval;
          }
          break;
#if XLEN >= 64
        case 3: /* c.ldsp */
          {
            uint64_t rval;
            imm_I = ((inst >> 7) & 0x20) |
                    (rs2 & (3 << 3)) |
                    ((inst << 4) & 0x1C0);
            uint_t addr = (int_t)(cpu_state.regs[2] + imm_I);
            if (iomap_manager.read_vaddr(&cpu_state, addr, 8, (uint8_t*)&rval) < 0)
              goto MMU_EXCEPTION;
            if (rd != 0)
              cpu_state.regs[rd] = (int64_t)rval;
          }
          break;
#elif FLEN >= 32
        case 3: /* c.flwsp */
          {
            uint32_t rval;
            if (cpu_state.fs == 0)
              goto ERROR_PROCESS;
            imm_I = ((inst >> 7) & 0x20) |
                    (rs2 & (7 << 2)) |
                    ((inst << 4) & 0xC0);
            uint_t addr = (int_t)(cpu_state.regs[2] + imm_I);
            if (iomap_manager.read_vaddr(&cpu_state, addr, 4, (uint8_t*)&rval) < 0)
              goto MMU_EXCEPTION;
            cpu_state.fp_reg[rd] = rval | F32_HIGH;
            cpu_state.fs = 3;
          }
          break;
#endif
        case 4:
          if (((inst >> 12) & 1) == 0)
          {
            if (rs2 == 0)
            {
              /* c.jr */
              if (rd == 0)
                goto ERROR_PROCESS;
              cpu_state.pc = cpu_state.regs[rd] & ~1;
              goto JUMP;
            }
            else
            {
              /* c.mv */
              if (rd != 0)
                cpu_state.regs[rd] = cpu_state.regs[rs2];
            }
          }
          else
          {
            if (rs2 == 0)
            {
              if (rd == 0)
              {
                /* c.ebreak */
                cpu_state.pending_exception = CAUSE_BREAKPOINT;
                goto Exception;
              }
              else
              {
                /* c.jalr */
                uint_t val = cpu_state.pc + 2;
                cpu_state.pc = cpu_state.regs[rd] & ~1;
                cpu_state.regs[1] = val;
                goto JUMP;
              }
            }
            else
            {
              if (rd != 0)
                cpu_state.regs[rd] = (int_t)(cpu_state.regs[rd] + cpu_state.regs[rs2]);
            }
          }
          break;
#if FLEN >= 64
        case 5: /* c.fsdsp */
          {
            if (cpu_state.fs == 0)
              goto ERROR_PROCESS;
            imm_I = ((inst >> 7) & 0x38) |
              ((inst >> 1) & 0x1C0);
            uint_t addr = (int_t)(cpu_state.regs[2] + imm_I);
            if (iomap_manager.write_vaddr(&cpu_state, addr, 8, (uint8_t*)&cpu_state.fp_reg[rs2]) < 0)
              goto MMU_EXCEPTION;
          }
          break;
#endif
        case 6: /* c.swsp */
          {
            imm_I = ((inst >> 7) & 0x3C) |
              ((inst >> 1) & 0xC0);
            uint_t addr = (int_t)(cpu_state.regs[2] + imm_I);
            if (iomap_manager.write_vaddr(&cpu_state, addr, 4, (uint8_t*)&cpu_state.regs[rs2]) < 0)
              goto MMU_EXCEPTION;
          }
          break;
#if XLEN >= 64
        case 7: /* c.sdsp */
          {
            imm_I = ((inst >> 7) & 0x38) |
                    ((inst >> 1) & 0x1C0);
            uint_t addr = (int_t)(cpu_state.regs[2] + imm_I);
            if (iomap_manager.write_vaddr(&cpu_state, addr, 8, (uint8_t*)&cpu_state.regs[rs2]) < 0)
              goto MMU_EXCEPTION;
          }
          break;
#elif FLEN >= 32
        case 7: /* c.fswsp */
          {
            if (cpu_state.fs == 0)
              goto ERROR_PROCESS;
            imm_I = ((inst >> 7) & 0x3C) |
                    ((inst >> 1) & 0xC0);
            uint_t addr = (int_t)(cpu_state.regs[2] + imm_I);
            if (iomap_manager.write_vaddr(&cpu_state, addr, 4, (uint8_t*)&cpu_state.fp_reg[rs2]) < 0)
              goto MMU_EXCEPTION;
          }
          break;
#endif
        default:
          goto ERROR_PROCESS;
      }
      goto C_NEXT_INSN;
    case OP_REG:
      {
        uint_t val1 = cpu_state.regs[rs1];
        uint_t val2 = cpu_state.regs[rs2];
        switch(funct7)
        {
          case 0x0:
            switch(funct3)
            {
              case 0x0: /* add */
                val1 = (int_t)(val1 + val2);
                break;
              case 0x1: /* sll */
                val1 = (int_t)(val1 << (val2 & (XLEN - 1)));
                break;
              case 0x2: /* slt */
                val1 = (int_t)val1 < (int_t)val2;
                break;
              case 0x3: /* sltu */
                val1 = val1 < val2;
                break;
              case 0x4: /* xor */
                val1 = val1 ^ val2;
                break;
              case 0x5: /* srl */
                val1 = (int_t)(val1 >> (val2 & (XLEN -1)));
                break;
              case 0x6: /* or */
                val1 = val1 | val2;
                break;
              case 0x7: /* and */
                val1 = val1 & val2;
                break;
              default:
                goto ERROR_PROCESS;
            }
            break;
          case 0x1:
            switch(funct3)
            {
              case 0: /* mul */
                val1 = (int_t)val1 * (int_t)val2;
                break;
              case 1: /* mulh */
                val1 = (int_t)mulh(val1, val2);
                break;
              case 2: /* mulhsu */
                val1 = (int_t)mulhsu(val1, val2);
                break;
              case 3: /* mulhu */
                val1 = (int_t)mulhu(val1, val2);
                break;
              case 4: /* div */
                val1 = m_div(val1, val2);
                break;
              case 5: /* divu */
                val1 = m_divu (val1, val2);
                break;
              case 6: /* rem */
                val1 = rem(val1, val2);
                break;
              case 7: /* remu */
                val1 = remu(val1, val2);
                break;
              default:
                goto ERROR_PROCESS;
            }
            break;
          case 0x20:
            switch(funct3)
            {
              case 0x0: /* sub */
                val1 = (int_t)(val1 - val2);
                break;
              case 0x5: /* sra */
                val1 = (int_t)val1 >> (val2 & (XLEN - 1));
                break;
              default:
                goto ERROR_PROCESS;
            }
            break;
          default:
            goto ERROR_PROCESS;
        }
        if (rd != 0)
        {
          cpu_state.regs[rd] = val1;
        }
      }
      break;
    case OP_IMM:
      {
        uint_t val1 = cpu_state.regs[rs1];
        int_t val2 = imm_I;
        switch(funct3)
        {
          case 0: /* addi */
            val1 = (int_t)(val1 + val2);
            break;
          case 1: /* slli */
            if ((val2 & ~(XLEN - 1)) != 0)
            {
              goto ERROR_PROCESS;
            }
            val1 = (int_t)(val1 << (val2 & (XLEN - 1)));
            break;
          case 2: /* slti */
            val1 = (int_t)val1 < (int_t)val2;
            break;
          case 3: /* sltiu */
            val1 = val1 < (uint_t)val2;
            break;
          case 4: /* xori */
            val1 = val1 ^ val2;
            break;
          case 5:
            if ((val2 & ~((XLEN - 1) | 0x400)) != 0)
              goto ERROR_PROCESS;
            if (val2 & 0x400) /* srai */
            {
              val1 = (int_t)val1 >> (val2 & (XLEN -1));
            }
            else /* srli */
            {
              val1 = (int_t)((uint_t)val1 >> (val2 & (XLEN - 1)));
            }
            break;
          case 6: /* ori */
            val1 = val1 | val2;
            break;
          case 7: /* andi */
            val1 = val1 & val2;
            break;
          default:
            goto ERROR_PROCESS;
        }
        if (rd != 0)
        {
          cpu_state.regs[rd] = val1;
        }
      }
      break;
#if XLEN >= 64
    case OP_IMM32:
      {
        uint_t val = cpu_state.regs[rs1];
        switch(funct3)
        {
          case 0: /* addiw */
            val = (int32_t)(val + imm_I);
            break;
          case 1: /* slliw */
            if ((imm_I & ~31) != 0)
             goto ERROR_PROCESS;
            val = (int32_t)(val << (imm_I & 31));
            break;
          case 5: /* srliw/sraiw */
            if ((imm_I & ~(31 | 0x400)) != 0)
              goto ERROR_PROCESS;
            if (imm_I & 0x400)
              val = (int32_t)val >> (imm_I & 31);
            else
              val = (int32_t)((uint32_t)val >> (imm_I & 31));
            break;
          default:
            goto ERROR_PROCESS;
        }
        if (rd != 0)
          cpu_state.regs[rd] = val;
      }
      break;
    case OP_32:
      {
        uint_t val1 = cpu_state.regs[rs1];
        uint_t val2 = cpu_state.regs[rs2];
        switch(funct7)
        {
          case 0:
            switch(funct3)
            {
              case 0: /* addw */
                val1 = (int32_t)(val1 + val2);
                break;
              case 1: /* sllw */
                val1 = (int32_t)((uint32_t)val1 << (val2 & 31));
                break;
              case 5: /* srlw */
                val1 = (int32_t)((uint32_t)val1 >> (val2 & 31));
                break;
              default:
                goto ERROR_PROCESS;
            }
            break;
          case 0x1:
            switch(funct3)
            {
              case 0: /* mulw */
                val1 = (int32_t)((int32_t)val1 * (int32_t)val2);
                break;
              case 4: /* divw */
                val1 = m_div32(val1, val2);
                break;
              case 5: /* divuw */
                val1 = (int32_t)m_divu32(val1, val2);
                break;
              case 6: /* remw */
                val1 = rem32(val1, val2);
                break;
              case 7: /* remuw */
                val1 = (int32_t)remu32(val1, val2);
                break;
              default:
                goto ERROR_PROCESS;
            }
            break;
          case 0x20:
            switch(funct3)
            {
              case 0: /* subw */
                val1 = (int32_t)(val1 - val2);
                break;
              case 5: /* sraw */
                val1 = (int32_t)val1 >> (val2 & 31);
                break;
              default:
                goto ERROR_PROCESS;
            }
            break;
          default:
            goto ERROR_PROCESS;
        }
        if (rd != 0)
          cpu_state.regs[rd] = val1;
      }
      break;
#endif
    case OP_STORE:
      {
        uint_t addr = cpu_state.regs[rs1] + imm_S;
        uint_t val2 = cpu_state.regs[rs2];
        int write_flag = 0;
        switch(funct3)
        {
          case 0: /* sb */
            write_flag = iomap_manager.write_vaddr(&cpu_state, addr, 1, (uint8_t*)&val2);
            break;
          case 1: /* sh */
            write_flag = iomap_manager.write_vaddr(&cpu_state, addr, 2, (uint8_t*)&val2);
            break;
          case 2: /* sw */
            write_flag = iomap_manager.write_vaddr(&cpu_state, addr, 4, (uint8_t*)&val2);
            break;
#if XLEN >= 64
          case 3: /* sd */
            write_flag = iomap_manager.write_vaddr(&cpu_state, addr, 8, (uint8_t*)&val2);
            break;
#endif
          default:
            goto ERROR_PROCESS;
        }
        if (write_flag < 0)
        {
          printf("store data error, addr: %lx\n", (uint64_t)addr);
          goto MMU_EXCEPTION;
        }
      }
      break;
    case OP_LOAD:
      {
        uint_t addr = cpu_state.regs[rs1] + imm_I;
        uint_t val = 0;
        int read_flag = 0;
        switch(funct3)
        {
          case 0x0: /* lb */
            {
              uint8_t ret = 0;
              read_flag = iomap_manager.read_vaddr(&cpu_state, addr, sizeof(ret), &ret);
              val = (int8_t)ret;
            } 
            break;
          case 0x1: /* lh */
            {
              uint16_t ret = 0;
              read_flag = iomap_manager.read_vaddr(&cpu_state, addr, sizeof(ret), (uint8_t*)&ret);
              val = (int16_t)ret;
            }
            break;
          case 0x2: /* lw */
            {
              uint32_t ret = 0;
              read_flag = iomap_manager.read_vaddr(&cpu_state, addr, sizeof(ret), (uint8_t*)&ret);
              val = (int32_t)ret;
            }
            break;
          case 0x4: /* lbu */
            {
              uint8_t ret = 0;
              read_flag = iomap_manager.read_vaddr(&cpu_state, addr, sizeof(ret), &ret);
              val = ret;
            }
            break;
          case 0x5: /* lhu */
            {
              uint16_t ret = 0;
              read_flag = iomap_manager.read_vaddr(&cpu_state, addr, sizeof(ret), (uint8_t*)&ret);
              val = ret;
            }
            break;
#if XLEN >= 64
          case 0x3: /* ld */
            {
              uint64_t ret = 0;
              read_flag = iomap_manager.read_vaddr(&cpu_state, addr, sizeof(ret), (uint8_t*)&ret);
              val = (int64_t)ret;
            }
            break;
          case 0x6: /* lwu */
            {
              uint32_t ret = 0;
              read_flag = iomap_manager.read_vaddr(&cpu_state, addr, sizeof(ret), (uint8_t*)&ret);
              val = ret;
            }
            break;
#endif
          default:
            goto ERROR_PROCESS;
        }
        if (read_flag < 0)
        {
          printf("load data error, addr: %lx\n", (uint64_t)addr);
          goto MMU_EXCEPTION;
        }
        if (rd != 0)
        {
          cpu_state.regs[rd] = val;
        }
      }
      break;
#if FLEN > 0
    case OP_FP_LOAD:
      if (cpu_state.fs == 0)
        goto ERROR_PROCESS;
      {
        uint_t addr = cpu_state.regs[rs1] + imm_I;
        switch(funct3)
        {
          case 2: /* flw */
            {
              uint32_t fret = 0;
              if (iomap_manager.read_vaddr(&cpu_state, addr, 4, (uint8_t*)&fret)< 0)
                goto MMU_EXCEPTION;
              cpu_state.fp_reg[rd] = fret | F32_HIGH;
            }
            break;
#if FLEN >= 64
          case 3: /* fld */
            {
              uint64_t fret = 0;
              if (iomap_manager.read_vaddr(&cpu_state, addr, 8, (uint8_t*)&fret)< 0)
                goto MMU_EXCEPTION;
              cpu_state.fp_reg[rd] = fret | F64_HIGH;
            }
            break;
#endif
          default:
            goto ERROR_PROCESS;
        }
        cpu_state.fs = 3;
      }
      break;
    case OP_FP_STORE:
      if (cpu_state.fs == 0)
        goto ERROR_PROCESS;
      {
        uint_t addr = cpu_state.regs[rs1] + imm_S;
        switch(funct3)
        {
          case 2: /* fsw */
           if (iomap_manager.write_vaddr(&cpu_state, addr, 4, (uint8_t*)&(cpu_state.fp_reg[rs2])) < 0)
             goto MMU_EXCEPTION; 
           break;
#if FLEN >= 64
          case 3: /* fsd */
           if (iomap_manager.write_vaddr(&cpu_state, addr, 8, (uint8_t*)&(cpu_state.fp_reg[rs2])) < 0)
             goto MMU_EXCEPTION; 
           break;
#endif
          default:
            goto ERROR_PROCESS;
        }
      }
      break;
    case OP_FP_FMADD:
      if (cpu_state.fs == 0)
        goto ERROR_PROCESS; 
      funct3 = (inst >> 25) & 3;
      rs3 = inst >> 27;
      rm = (inst >> 12) & 7;
      rm = get_inst_rm(&cpu_state, rm);
      if (rm < 0)
        goto ERROR_PROCESS;
      switch (funct3)
      {
        case 0:
          cpu_state.fp_reg[rd] = fma_sf32(cpu_state.fp_reg[rs1], cpu_state.fp_reg[rs2],
                                          cpu_state.fp_reg[rs3], rm, &cpu_state.fflags) | F32_HIGH;
          break;
#if FLEN >= 64
        case 1:
          cpu_state.fp_reg[rd] = fma_sf64(cpu_state.fp_reg[rs1], cpu_state.fp_reg[rs2],
                                          cpu_state.fp_reg[rs3], rm, &cpu_state.fflags) | F64_HIGH;
          break;
#endif
        default:
          goto ERROR_PROCESS;
      }
      cpu_state.fs = 3;
      break;
    case OP_FP_FMSUB:
      if (cpu_state.fs == 0)
        goto ERROR_PROCESS;
      funct3 = (inst >> 25) & 3;
      rs3 = inst >> 27;
      rm = (inst >> 12) & 7;
      rm = get_inst_rm(&cpu_state, rm);
      if (rm < 0)
        goto ERROR_PROCESS;
      switch (funct3)
      {
        case 0:
          cpu_state.fp_reg[rd] = fma_sf32(cpu_state.fp_reg[rs1], cpu_state.fp_reg[rs2],
                                          cpu_state.fp_reg[rs3] ^ FSIGN_MASK32,
                                          rm, &cpu_state.fflags) | F32_HIGH;
          break;
#if FLEN >= 64
        case 1:
          cpu_state.fp_reg[rd] = fma_sf64(cpu_state.fp_reg[rs1], cpu_state.fp_reg[rs2],
                                          cpu_state.fp_reg[rs3] ^ FSIGN_MASK64,
                                          rm, &cpu_state.fflags) | F64_HIGH;
          break;
#endif
        default:
          goto ERROR_PROCESS;
      }
      cpu_state.fs = 3;
      break;
    case OP_FP_FNMSUB:
      if (cpu_state.fs == 0)
        goto ERROR_PROCESS;
      funct3 = (inst >> 25) & 3;
      rs3 = inst >> 27;
      rm = (inst >> 12) & 7;
      rm = get_inst_rm(&cpu_state, rm);
      if (rm < 0)
        goto ERROR_PROCESS;
      switch (funct3)
      {
        case 0:
          cpu_state.fp_reg[rd] = fma_sf32(cpu_state.fp_reg[rs1] ^ FSIGN_MASK32,
                                          cpu_state.fp_reg[rs2],
                                          cpu_state.fp_reg[rs3],
                                          rm, &cpu_state.fflags) | F32_HIGH;
          break;
#if FLEN >= 64
        case 1:
          cpu_state.fp_reg[rd] = fma_sf64(cpu_state.fp_reg[rs1] ^ FSIGN_MASK64,
                                          cpu_state.fp_reg[rs2],
                                          cpu_state.fp_reg[rs3],
                                          rm, &cpu_state.fflags) | F64_HIGH;
          break;
#endif
        default:
          goto ERROR_PROCESS;
      }
      cpu_state.fs = 3;
      break;
    case OP_FP_FNMADD:
      if (cpu_state.fs == 0)
        goto ERROR_PROCESS;
      funct3 = (inst >> 25) & 3;
      rs3 = inst >> 27;
      rm = (inst >> 12) & 7;
      rm = get_inst_rm(&cpu_state, rm);
      if (rm < 0)
        goto ERROR_PROCESS;
      switch (funct3)
      {
        case 0:
          cpu_state.fp_reg[rd] = fma_sf32(cpu_state.fp_reg[rs1] ^ FSIGN_MASK32,
                                          cpu_state.fp_reg[rs2],
                                          cpu_state.fp_reg[rs3] ^ FSIGN_MASK32,
                                          rm, &cpu_state.fflags) | F32_HIGH;
          break;
#if FLEN >= 64
        case 1:
          cpu_state.fp_reg[rd] = fma_sf64(cpu_state.fp_reg[rs1] ^ FSIGN_MASK64,
                                          cpu_state.fp_reg[rs2],
                                          cpu_state.fp_reg[rs3] ^FSIGN_MASK64,
                                          rm, &cpu_state.fflags) | F64_HIGH;
          break;
#endif
        default:
          goto ERROR_PROCESS;
      }
      cpu_state.fs = 3;
      break;
    case OP_FP_NORMAL:
      if (cpu_state.fs == 0)
        goto ERROR_PROCESS;
      rm = (inst >> 12) & 7;
      rm = get_inst_rm(&cpu_state, rm);
      switch(funct7)
      {
        case 0x0: /* fadd.s */
          if (rm < 0)
            goto ERROR_PROCESS;
          cpu_state.fp_reg[rd] = add_sf32(cpu_state.fp_reg[rs1],
                                          cpu_state.fp_reg[rs2],
                                          rm, &cpu_state.fflags) | F32_HIGH;
          cpu_state.fs = 3;
          break;
#if FLEN >= 64
        case 0x1: /* fadd.d */ 
          if (rm < 0)
            goto ERROR_PROCESS;
          cpu_state.fp_reg[rd] = add_sf64(cpu_state.fp_reg[rs1],
                                          cpu_state.fp_reg[rs2],
                                          rm, &cpu_state.fflags) | F64_HIGH;
          cpu_state.fs = 3;
          break;

#endif
        case 0x04: /* fsub.s */
          if (rm < 0)
            goto ERROR_PROCESS;
          cpu_state.fp_reg[rd] = sub_sf32(cpu_state.fp_reg[rs1],
                                          cpu_state.fp_reg[rs2],
                                          rm, &cpu_state.fflags) | F32_HIGH;
          cpu_state.fs = 3;
          break;
#if FLEN >= 64
        case 0x05: /* fsub.d */
          if (rm < 0)
            goto ERROR_PROCESS;
          cpu_state.fp_reg[rd] = sub_sf64(cpu_state.fp_reg[rs1],
                                          cpu_state.fp_reg[rs2],
                                          rm, &cpu_state.fflags) | F64_HIGH;
          cpu_state.fs = 3;
          break;
#endif
        case 0x08: /* fmul.s */
          if (rm < 0)
            goto ERROR_PROCESS;
          cpu_state.fp_reg[rd] = mul_sf32(cpu_state.fp_reg[rs1],
                                          cpu_state.fp_reg[rs2],
                                          rm, &cpu_state.fflags) | F32_HIGH; 
          cpu_state.fs = 3;
          break;
#if FLEN >= 64
        case 0x09: /*fmul.d */
          if (rm < 0)
            goto ERROR_PROCESS;
          cpu_state.fp_reg[rd] = mul_sf64(cpu_state.fp_reg[rs1],
                                          cpu_state.fp_reg[rs2],
                                          rm, &cpu_state.fflags) | F64_HIGH; 
          cpu_state.fs = 3;
          break;
#endif
        case 0x0C: /* fdiv.s */
          if (rm < 0)
            goto ERROR_PROCESS;
          cpu_state.fp_reg[rd] = div_sf32(cpu_state.fp_reg[rs1], 
                                          cpu_state.fp_reg[rs2],
                                          rm, &cpu_state.fflags) | F32_HIGH;
          cpu_state.fs = 3;
          break;
#if FLEN >= 64
        case 0x0D: /* fdiv.d */
          if (rm < 0)
            goto ERROR_PROCESS;
          cpu_state.fp_reg[rd] = div_sf64(cpu_state.fp_reg[rs1], 
                                          cpu_state.fp_reg[rs2],
                                          rm, &cpu_state.fflags) | F64_HIGH;
          cpu_state.fs = 3;
          break;
#endif
        case 0x2C: /* fsqrt.s */
          if (rm < 0 || rs2 != 0)
            goto ERROR_PROCESS;
          cpu_state.fp_reg[rd] = sqrt_sf32(cpu_state.fp_reg[rs1],
                                          rm, &cpu_state.fflags) | F32_HIGH;
          cpu_state.fs = 3;
          break;
#if FLEN >= 64
        case 0x2D: /* fsqrt.d */
          if (rm < 0 || rs2 != 0)
            goto ERROR_PROCESS;
          cpu_state.fp_reg[rd] = sqrt_sf64(cpu_state.fp_reg[rs1],
                                          rm, &cpu_state.fflags) | F64_HIGH;
          cpu_state.fs = 3;
          break;
#endif
        case 0x10:
          switch(rm)
          {
            case 0x0: /* fsgnj.s */
              cpu_state.fp_reg[rd] = (cpu_state.fp_reg[rs1] & ~FSIGN_MASK32) |
                                     (cpu_state.fp_reg[rs2] & FSIGN_MASK32);
              break;
            case 0x1: /* fsgnjn.s */
              cpu_state.fp_reg[rd] = (cpu_state.fp_reg[rs1] & ~FSIGN_MASK32) |
                                     ((cpu_state.fp_reg[rs2] & FSIGN_MASK32) ^ FSIGN_MASK32);
              break;
            case 0x2: /* fsgnjx.s */
              cpu_state.fp_reg[rd] = cpu_state.fp_reg[rs1] ^ (cpu_state.fp_reg[rs2] & FSIGN_MASK32);
              break;
            default:
              goto ERROR_PROCESS;
          }
          cpu_state.fs = 3;
          break;
#if FLEN >= 64
        case 0x11:
          switch(rm)
          {
            case 0x0: /* fsgnj.d */
              cpu_state.fp_reg[rd] = (cpu_state.fp_reg[rs1] & ~FSIGN_MASK64) |
                                     (cpu_state.fp_reg[rs2] & FSIGN_MASK64);
              break;
            case 0x1: /* fsgnjn.d */
              cpu_state.fp_reg[rd] = (cpu_state.fp_reg[rs1] & ~FSIGN_MASK64) |
                                     ((cpu_state.fp_reg[rs2] & FSIGN_MASK64) ^ FSIGN_MASK64);
              break;
            case 0x2: /* fsgnjx.s */
              cpu_state.fp_reg[rd] = cpu_state.fp_reg[rs1] ^ (cpu_state.fp_reg[rs2] & FSIGN_MASK64);
              break;
            default:
              goto ERROR_PROCESS;
          }
          cpu_state.fs = 3;
          break;
#endif
        case 0x14:
          switch(rm)
          {
            case 0: /* fmin.s */
              cpu_state.fp_reg[rd] = min_sf32(cpu_state.fp_reg[rs1],
                                              cpu_state.fp_reg[rs2],
                                              &cpu_state.fflags) | F32_HIGH;
              break;
            case 1: /* fmax.s */
              cpu_state.fp_reg[rd] = max_sf32(cpu_state.fp_reg[rs1],
                                              cpu_state.fp_reg[rs2],
                                              &cpu_state.fflags) | F32_HIGH;
              break;
            default:
              goto ERROR_PROCESS;
          }
          cpu_state.fs = 3;
          break;
#if FLEN >= 64
        case 0x15:
          switch(rm)
          {
            case 0: /* fmin.d */
              cpu_state.fp_reg[rd] = min_sf64(cpu_state.fp_reg[rs1],
                                              cpu_state.fp_reg[rs2],
                                              &cpu_state.fflags) | F64_HIGH;
              break;
            case 1: /* fmax.d */
              cpu_state.fp_reg[rd] = max_sf64(cpu_state.fp_reg[rs1],
                                              cpu_state.fp_reg[rs2],
                                              &cpu_state.fflags) | F64_HIGH;
              break;
            default:
              goto ERROR_PROCESS;
          }
          cpu_state.fs = 3;
          break;
        case 0x20:
          if (rm < 0)
            goto ERROR_PROCESS;
          switch(rs2)
          {
            case 1: /* fcvt.s.d */
              cpu_state.fp_reg[rd] = cvt_sf64_sf32(cpu_state.fp_reg[rs1], rm, &cpu_state.fflags) | F32_HIGH;
              break;
            default:
              goto ERROR_PROCESS;
          }
          break;
        case 0x21:
          if (rm < 0)
            goto ERROR_PROCESS;
          switch(rs2)
          {
            case 0: /* fcvt.d.s */
              cpu_state.fp_reg[rd] = cvt_sf32_sf64(cpu_state.fp_reg[rs1], &cpu_state.fflags) | F64_HIGH;
              break;
            default:
              goto ERROR_PROCESS;
          }
          break;
#endif
        case 0x60:
          {
            if (rm < 0)
              goto ERROR_PROCESS;
            uint_t val = 0;
            switch(rs2)
            {
              case 0: /* fcvt.w.s */
                val = (int32_t)cvt_sf32_i32(cpu_state.fp_reg[rs1], rm, &cpu_state.fflags);
                break;
              case 1: /* fcvt.wu.s */
                val = (int32_t)cvt_sf32_u32(cpu_state.fp_reg[rs1], rm, &cpu_state.fflags);
                break;
#if XLEN >= 64
              case 2: /* fcvt.l.s */
                val = (int64_t)cvt_sf32_i64(cpu_state.fp_reg[rs1], rm, &cpu_state.fflags);
                break;
              case 3: /* fcvt.lu.s */
                val = (int64_t)cvt_sf32_u64(cpu_state.fp_reg[rs1], rm, &cpu_state.fflags);
                break;
#endif
              default:
                goto ERROR_PROCESS;
            }
            if (rd != 0)
              cpu_state.regs[rd] = val;
          }
          break;
#if FLEN >= 64
        case 0x61:
          {
            if (rm < 0)
              goto ERROR_PROCESS;
            uint_t val = 0;
            switch(rs2)
            {
              case 0: /* fcvt.w.d */
                val = (int32_t)cvt_sf64_i32(cpu_state.fp_reg[rs1], rm, &cpu_state.fflags);
                break;
              case 1: /* fcvt.wu.d */
                val = (int32_t)cvt_sf64_u32(cpu_state.fp_reg[rs1], rm, &cpu_state.fflags);
                break;
#if XLEN >= 64
              case 2: /* fcvt.l.d */
                val = (int64_t)cvt_sf64_i64(cpu_state.fp_reg[rs1], rm, &cpu_state.fflags);
                break;
              case 3: /* fcvt.lu.d */
                val = (int64_t)cvt_sf64_u64(cpu_state.fp_reg[rs1], rm, &cpu_state.fflags);
                break;
#endif
              default:
                goto ERROR_PROCESS;
            }
            if (rd != 0)
              cpu_state.regs[rd] = val;
          }
          break;
#endif
        case 0x70:
          if (rs2 != 0)
            goto ERROR_PROCESS;
          {
            uint_t val = 0;
            switch(rm)
            {
#if FLEN <= XLEN
              case 0: /* fmv.x.w */
                val = (int32_t)cpu_state.fp_reg[rs1];
                break;
#endif
              case 1: /* fclass.s */
                val = fclass_sf32(cpu_state.fp_reg[rs1]);
                break;
              default:
                goto ERROR_PROCESS;
            }
            if (rd != 0)
              cpu_state.regs[rd] = val;
          }
          break;
#if FLEN >= 64
        case 0x71:
          if (rs2 != 0)
            goto ERROR_PROCESS;
          {
            uint_t val = 0;
            switch(rm)
            {
#if FLEN <= XLEN
              case 0: /* fmv.x.d */
                val = (int64_t)cpu_state.fp_reg[rs1];
                break;
#endif
              case 1: /* fclass.d */
                val = fclass_sf64(cpu_state.fp_reg[rs1]);
                break;
              default:
                goto ERROR_PROCESS;
            }
            if (rd != 0)
              cpu_state.regs[rd] = val;
          }
          break;
#endif
        case 0x50:
          {
            uint_t val = 0;
            switch(rm)
            {
              case 0: /* fle.s */
                val = le_sf32(cpu_state.fp_reg[rs1],
                              cpu_state.fp_reg[rs2],
                              &cpu_state.fflags);
                break;
              case 1: /* flt.s */
                val = lt_sf32(cpu_state.fp_reg[rs1],
                              cpu_state.fp_reg[rs2],
                              &cpu_state.fflags);
                break;
              case 2: /* feq.s */
                val = eq_quiet_sf32(cpu_state.fp_reg[rs1],
                                    cpu_state.fp_reg[rs2],
                                    &cpu_state.fflags);
                break;
              default:
                goto ERROR_PROCESS;
            }
            if (rd != 0)
              cpu_state.regs[rd] = val;
          }
          break;
#if FLEN >= 64
        case 0x51:
          {
            uint_t val = 0;
            switch(rm)
            {
              case 0: /* fle.d */
                val = le_sf64(cpu_state.fp_reg[rs1],
                              cpu_state.fp_reg[rs2],
                              &cpu_state.fflags);
                break;
              case 1: /* flt.d */
                val = lt_sf64(cpu_state.fp_reg[rs1],
                              cpu_state.fp_reg[rs2],
                              &cpu_state.fflags);
                break;
              case 2: /* feq.d */
                val = eq_quiet_sf64(cpu_state.fp_reg[rs1],
                                    cpu_state.fp_reg[rs2],
                                    &cpu_state.fflags);
                break;
              default:
                goto ERROR_PROCESS;
            }
            if (rd != 0)
              cpu_state.regs[rd] = val;
          }
          break;
#endif
        case 0x68:
          if (rm < 0)
            goto ERROR_PROCESS;
          switch(rs2)
          {
            case 0: /* fcvt.s.w */
              cpu_state.fp_reg[rd] = cvt_i32_sf32(cpu_state.regs[rs1],
                                                  rm, &cpu_state.fflags) | F32_HIGH;
              break;
            case 1: /* fcvt.s.wu */
              cpu_state.fp_reg[rd] = cvt_u32_sf32(cpu_state.regs[rs1],
                                                  rm, &cpu_state.fflags) | F32_HIGH;
              break;
#if XLEN >= 64
            case 2: /* fcvt.s.l */
              cpu_state.fp_reg[rd] = cvt_i64_sf32(cpu_state.regs[rs1],
                                                  rm, &cpu_state.fflags) | F32_HIGH;
              break;
            case 3: /* fcvt.s.lu */
              cpu_state.fp_reg[rd] = cvt_u64_sf32(cpu_state.regs[rs1],
                                                  rm, &cpu_state.fflags) | F32_HIGH;
              break;
#endif
            default:
              goto ERROR_PROCESS;
          }
          cpu_state.fs = 3;
          break;
#if FLEN >= 64
        case 0x69:
          if (rm < 0)
            goto ERROR_PROCESS;
          switch(rs2)
          {
            case 0: /* fcvt.d.w */
              cpu_state.fp_reg[rd] = cvt_i32_sf64(cpu_state.regs[rs1],
                                                  rm, &cpu_state.fflags) | F64_HIGH;
              break;
            case 1: /* fcvt.d.wu */
              cpu_state.fp_reg[rd] = cvt_u32_sf64(cpu_state.regs[rs1],
                                                  rm, &cpu_state.fflags) | F64_HIGH;
              break;
#if XLEN >= 64
            case 2: /* fcvt.d.l */
              cpu_state.fp_reg[rd] = cvt_i64_sf64(cpu_state.regs[rs1],
                                                  rm, &cpu_state.fflags) | F64_HIGH;
              break;
            case 3: /* fcvt.d.lu */
              cpu_state.fp_reg[rd] = cvt_u64_sf64(cpu_state.regs[rs1],
                                                  rm, &cpu_state.fflags) | F64_HIGH;
              break;
#endif
            default:
              goto ERROR_PROCESS;
          }
          cpu_state.fs = 3;
          break;
#endif
#if FLEN <= XLEN
        case 0x78: /* fmv.w.x */
          rm = (inst >> 12) & 7;
          if (rs2 != 0 || rm != 0)
            goto ERROR_PROCESS;
          cpu_state.fp_reg[rd] = (int32_t)cpu_state.regs[rs1];
          cpu_state.fs = 3;
          break;
#if FLEN >= 64
        case 0x79: /* fmv.d.x */
          rm = (inst >> 12) & 7;
          if (rs2 != 0 || rm != 0)
            goto ERROR_PROCESS;
          cpu_state.fp_reg[rd] = (int64_t)cpu_state.regs[rs1];
          cpu_state.fs = 3;
          break;
#endif
#endif
        default:
          goto ERROR_PROCESS;
      }
      break;
#endif
    case OP_AUIPC: /* auipc */
      if (rd != 0)
      {
        cpu_state.regs[rd] = (int_t)(cpu_state.pc + imm_U);
      }
      break;
    case OP_BRANCH:
      {
        int32_t condition = 0;
        switch(funct3)
        {
          case 0: /* beq */
          case 1: /* bne */
            condition = (cpu_state.regs[rs1] == cpu_state.regs[rs2]);
            break;
          case 4: /* blt */
          case 5: /* bge */
            condition = ((int_t)(cpu_state.regs[rs1]) < (int_t)(cpu_state.regs[rs2]));
            break;
          case 6: /* bltu */
          case 7: /* bgeu */
            condition = (cpu_state.regs[rs1] < cpu_state.regs[rs2]);
            break;
          default:
            goto ERROR_PROCESS;
        }
        condition ^= (funct3 & 1);
        if (condition)
        {
          cpu_state.pc = (int_t)(cpu_state.pc + imm_B);
          goto JUMP;
        }
      }
      break;
    case OP_JAL: /* jal */
      if (rd != 0)
      {
        cpu_state.regs[rd] = cpu_state.pc + 4;
      }
      cpu_state.pc = (int_t)(cpu_state.pc + imm_J);
      goto JUMP;
    case OP_JALR: /* jalr */
      {
        uint_t val = cpu_state.pc + 4;
        cpu_state.pc = (int_t)(cpu_state.regs[rs1] + imm_I) & ~1;
        if (rd != 0)
        {
          cpu_state.regs[rd] = val;
        }
        goto JUMP;
      }
    case OP_LUI: /* lui */
      if (rd != 0)
      {
        cpu_state.regs[rd] = (int32_t)(imm_U);
      }
      break;
    case OP_SYSTEM:
      imm_I = inst >> 20;
      switch(funct3)
      {
        case 0:
          switch(imm_I)
          {
            case 0: /* ecall */
              if (inst & 0x000FFF80)
                goto ERROR_PROCESS;
              cpu_state.pending_exception = CAUSE_USER_ECALL + cpu_state.priv;
              goto Exception;
            case 1: /* ebreak */
              if (inst & 0x000FFF80)
                goto ERROR_PROCESS;
              cpu_state.pending_exception = CAUSE_BREAKPOINT;
              goto Exception;
            case 0x102: /* sret */
              {
                if (inst & 0x000FFF80)
                  goto ERROR_PROCESS;
                if (cpu_state.priv < PRIV_S)
                  goto ERROR_PROCESS;
                handle_sret(&cpu_state);
                goto DONE_INTERP;
              }
              break;
            case 0x302: /* mret */
              {
                if (inst & 0x000FFF80)
                  goto ERROR_PROCESS;
                if (cpu_state.priv < PRIV_M)
                  goto ERROR_PROCESS;
                handle_mret(&cpu_state);
                goto DONE_INTERP;
              }
              break;
            case 0x105: /* wfi */
              {
                if (inst & 0x00007F80)
                  goto ERROR_PROCESS;
                if (cpu_state.priv == PRIV_U)
                  goto ERROR_PROCESS;
                if ((cpu_state.priv < PRIV_M) && (cpu_state.mstatus & MSTATUS_TW))
                  goto ERROR_PROCESS;
                if ((cpu_state.mip & cpu_state.mie) == 0)
                {
                  cpu_state.power_down_flag = true;
                  cpu_state.pc += 4;
                  goto DONE_INTERP;
                }
              }
              break;
            default:
              if ((imm_I >> 5) == 0x09)
              {
                /* sfence.vma */
                if (inst & 0x00007F80)
                  goto ERROR_PROCESS;
                if (cpu_state.priv == PRIV_U)
                  goto ERROR_PROCESS;
                // if (rs1 == 0)
                // {
                //   //TODO: tlb flush all 
                //   void(rs1);
                // }
                // else
                // {
                //   //TODO: tlb flush vaddr
                //   void(rs1);
                // }
                cpu_state.pc += 4;
                goto JUMP;
              }
              else
              {
                goto ERROR_PROCESS;
              }
          }
          break;
        case 1: /* csrrw */
          {
            funct3 &= 3;
            uint_t value = 0;
            int error = 0;
            if (csr_read(&cpu_state, &value, imm_I, true))
              goto ERROR_PROCESS;
            value = (int_t)value;
            error = csr_write(&cpu_state, imm_I, cpu_state.regs[rs1]);
            if (error < 0)
              goto ERROR_PROCESS;
            if (rd != 0)
              cpu_state.regs[rd] = value;
            if (error > 0)
            {
              cpu_state.pc += 4;
              if (error == 2)
                goto JUMP;
              else
                goto DONE_INTERP;
            }
          }
          break;
        case 2: /* csrrs */
        case 3: /* csrrc */
          {
            funct3 &= 3;
            uint_t value = 0;
            uint_t value2 = cpu_state.regs[rs1];
            int error = 0;
            if (csr_read(&cpu_state, &value, imm_I, (rs1 != 0)))
              goto ERROR_PROCESS;
            value = (int_t)(value);
            if (rs1 != 0)
            {
              if (funct3 == 2)
                value2 = value | value2;
              else
                value2 = value & ~value2;
              error = csr_write(&cpu_state, imm_I, value2);
              if (error < 0)
                goto ERROR_PROCESS;
            }
            else
            {
              error = 0;
            }
            if (rd != 0)
              cpu_state.regs[rd] = value;
            if (error > 0)
            {
              cpu_state.pc += 4;
              if (error == 2)
                goto JUMP;
              else
                goto DONE_INTERP;
            }
          }
          break;
        case 5: /* csrrwi */
          {
            funct3 &= 3;
            uint_t value = 0;
            int error = 0;
            if (csr_read(&cpu_state, &value, imm_I, true))
              goto ERROR_PROCESS;
            value = (int_t)value;
            error = csr_write(&cpu_state, imm_I, rs1);
            if (error < 0)
              goto ERROR_PROCESS;
            if (rd != 0)
              cpu_state.regs[rd] = value;
            if (error > 0)
            {
              cpu_state.pc += 4;
              if (error == 2)
                goto JUMP;
              else
                goto DONE_INTERP;
            }
          }
          break;
        case 6: /* csrrsi */
        case 7: /* csrrci */
          {
            funct3 &= 3;
            uint_t value = 0;
            uint_t value2 = rs1;
            int error = 0;
            if (csr_read(&cpu_state, &value, imm_I, (rs1 != 0)))
              goto ERROR_PROCESS;
            value = (int_t)(value);
            if (rs1 != 0)
            {
              if (funct3 == 2)
                value2 = value | value2;
              else
                value2 = value & ~value2;
              error = csr_write(&cpu_state, imm_I, value2);
              if (error < 0)
                goto ERROR_PROCESS;
            }
            else
            {
              error = 0;
            }
            if (rd != 0)
              cpu_state.regs[rd] = value;
            if (error > 0)
            {
              cpu_state.pc += 4;
              if (error == 2)
                goto JUMP;
              else
                goto DONE_INTERP;
            }
          }
          break;
        default:
          goto ERROR_PROCESS;
      }
      break;
    case OP_FENCE:
      switch(funct3)
      {
        case 0: /* fence */
          if (inst & 0xF00FFF80)
            goto ERROR_PROCESS;
          break;
        case 1: /* fence.i */
          if (inst != 0x0000100F)
            goto ERROR_PROCESS;
          break;
        default:
          goto ERROR_PROCESS;
      }
      break;
    case OP_AMO:
      {
        uint_t addr = cpu_state.regs[rs1];
        uint_t value = 0;
        funct7 = inst >> 27;
        switch(funct3)
        {
          case 2:
            {
              uint32_t rval;
              switch(funct7)
              {
                case 2: /* lr.w */
                  if (rs2 != 0)
                    goto ERROR_PROCESS;
                  if (iomap_manager.read_vaddr(&cpu_state, addr, 4, (uint8_t*)&rval) < 0)
                    goto MMU_EXCEPTION;
                  value = (int32_t)rval;
                  cpu_state.load_res = addr;
                  break;
                case 3: /* sc.w */
                  if (cpu_state.load_res == addr)
                  {
                    if (iomap_manager.write_vaddr(&cpu_state, addr, 4, (uint8_t*)&cpu_state.regs[rs2]) < 0)
                      goto MMU_EXCEPTION;
                    value = 0;
                    cpu_state.load_res = 0;
                  }
                  else
                  {
                    value = 1;
                  }
                  break;
                case 1: /* amoswap.w */
                case 0: /* amoadd.w */
                case 4: /* amoxor.w */
                case 0xc: /* amoand.w */
                case 0x8: /* amoor.w */
                case 0x10: /* amomin.w */
                case 0x14: /* amomax.w */
                case 0x18: /* amominu.w */
                case 0x1c: /* amomaxu.w */
                  {
                    uint_t val2;
                    if (iomap_manager.read_vaddr(&cpu_state, addr, 4, (uint8_t*)&rval) < 0)
                      goto MMU_EXCEPTION;
                    value = (int32_t)rval;
                    val2 = cpu_state.regs[rs2];
                    switch(funct7)
                    {
                      case 1: /* amoswap.w */
                        break;
                      case 0: /* amoadd.w */
                        val2 = (int32_t)(value + val2);
                        break;
                      case 4: /* amoxor.w */
                        val2 = (int32_t)(value ^ val2);
                        break;
                      case 0xc: /* amoand.w */
                        val2 = (int32_t)(value & val2);
                        break;
                      case 0x8: /* amoor.w */
                        val2 = (int32_t)(value | val2);
                        break;
                      case 0x10: /* amomin.w */
                        if ((int32_t)value < (int32_t)val2)
                          val2 = (int32_t)value;
                        break;
                      case 0x14: /* amomax.w */
                        if ((int32_t)value > (int32_t)val2)
                          val2 = (int32_t)value;
                        break;
                      case 0x18: /* amominu.w */
                        if ((uint32_t)value < (uint32_t)val2)
                          val2 = (int32_t)value;
                        break;
                      case 0x1c: /* amomaxu.w */
                        if ((uint32_t)value > (uint32_t)val2)
                          val2 = (int32_t)value;
                        break;
                      default:
                        goto ERROR_PROCESS;
                    }
                    if (iomap_manager.write_vaddr(&cpu_state, addr, 4, (uint8_t*)&val2) < 0)
                      goto MMU_EXCEPTION;
                  }
                  break;
                default:
                  goto ERROR_PROCESS;
              }
            }
            break;
          case 3:
            {
              uint64_t rval;
              switch(funct7)
              {
                case 2: /* lr.d */
                  if (rs2 != 0)
                    goto ERROR_PROCESS;
                  if (iomap_manager.read_vaddr(&cpu_state, addr, 8, (uint8_t*)&rval) < 0)
                    goto MMU_EXCEPTION;
                  value = (int64_t)rval;
                  cpu_state.load_res = addr;
                  break;
                case 3: /* sc.d */
                  if (cpu_state.load_res == addr)
                  {
                    if (iomap_manager.write_vaddr(&cpu_state, addr, 8, (uint8_t*)&cpu_state.regs[rs2]) < 0)
                      goto MMU_EXCEPTION;
                    value = 0;
                    cpu_state.load_res = 0;
                  }
                  else
                  {
                    value = 1;
                  }
                  break;
                case 1: /* amoswap.w */
                case 0: /* amoadd.w */
                case 4: /* amoxor.w */
                case 0xc: /* amoand.w */
                case 0x8: /* amoor.w */
                case 0x10: /* amomin.w */
                case 0x14: /* amomax.w */
                case 0x18: /* amominu.w */
                case 0x1c: /* amomaxu.w */
                  {
                    uint_t val2;
                    if (iomap_manager.read_vaddr(&cpu_state, addr, 8, (uint8_t*)&rval) < 0)
                      goto MMU_EXCEPTION;
                    value = (int64_t)rval;
                    val2 = cpu_state.regs[rs2];
                    switch(funct7)
                    {
                      case 1: /* amoswap.d */
                        break;
                      case 0: /* amoadd.d */
                        val2 = (int64_t)(value + val2);
                        break;
                      case 4: /* amoxor.d */
                        val2 = (int64_t)(value ^ val2);
                        break;
                      case 0xc: /* amoand.d */
                        val2 = (int64_t)(value & val2);
                        break;
                      case 0x8: /* amoor.d */
                        val2 = (int64_t)(value | val2);
                        break;
                      case 0x10: /* amomin.d */
                        if ((int64_t)value < (int64_t)val2)
                          val2 = (int64_t)value;
                        break;
                      case 0x14: /* amomax.d */
                        if ((int64_t)value > (int64_t)val2)
                          val2 = (int64_t)value;
                        break;
                      case 0x18: /* amominu.d */
                        if ((uint64_t)value < (uint64_t)val2)
                          val2 = (int64_t)value;
                        break;
                      case 0x1c: /* amomaxu.d */
                        if ((uint64_t)value > (uint64_t)val2)
                          val2 = (int64_t)value;
                        break;
                      default:
                        goto ERROR_PROCESS;
                    }
                    if (iomap_manager.write_vaddr(&cpu_state, addr, 8, (uint8_t*)&val2) < 0)
                      goto MMU_EXCEPTION;
                  }
                  break;
                default:
                  goto ERROR_PROCESS;
              }
            }
            break;
          default:
            goto ERROR_PROCESS;
        }
        if (rd != 0)
          cpu_state.regs[rd] = value;
      }
      break;
    default:
      goto ERROR_PROCESS;
  }
  cpu_state.pc += 4;
  return;
C_NEXT_INSN:
  cpu_state.pc += 2;
  return;

ERROR_PROCESS:
//  printf("Error instructions decode! 0x%x, cycles: %lu\n", inst, cpu_state.cycles);
  cpu_state.pending_exception = CAUSE_ILLEGAL_INSTRUCTION;
  cpu_state.pending_tval = inst;

MMU_EXCEPTION:
Exception:
  if (cpu_state.pending_exception >= 0)
  {
    raise_exception(&cpu_state, cpu_state.pending_exception, cpu_state.pending_tval);
    return;
  }
DONE_INTERP:
JUMP:
  return;
}

void machine_loop()
{
  fd_set rfds, wfds, efds;
  int stdin_fd, fd_max, ret, delay;
  struct timeval tv;

  delay = machine_get_sleep_duration(&cpu_state, MAX_DELAY_TIME);

  FD_ZERO(&rfds);
  FD_ZERO(&wfds);
  FD_ZERO(&efds);
  fd_max = -1;
  if (virtio_console_can_write_data((virtual_io_device_t*)riscv_machine.console))
  {
    stdio_device_t *stdio_device = riscv_machine.console->cs->opaque;
    stdin_fd = stdio_device->stdin_fd;
    FD_SET(stdin_fd, &rfds);
    
    if (stdio_device->resize_pending)
    {
      int width, height;
      console_get_size(stdio_device, &width, &height);
      virtio_console_resize_event((virtual_io_device_t*)riscv_machine.console, width, height);
      stdio_device->resize_pending = false;
    }
  }

  tv.tv_sec = delay / 1000;
  tv.tv_usec = (delay % 1000) * 1000;
  ret = select(fd_max + 1, &rfds, &wfds, &efds, &tv);
  if (ret > 0)
  {
    if (riscv_machine.console && FD_ISSET(stdin_fd, &rfds))
    {
      uint8_t buf[128];
      int ret, len;
      len = virtio_console_get_write_len(&(riscv_machine.console->common));
      len = min_int(len, sizeof(buf));
      ret = riscv_machine.console->cs->read_data(riscv_machine.console->cs->opaque, buf, len);
      if (ret > 0)
      {
        virtio_console_write_data(&(riscv_machine.console->common), buf, ret);
      }
    }
  }
  // check interrupts
  if ((cpu_state.mip & cpu_state.mie) != 0)
  {
    if (raise_interrupt(&cpu_state))
    {
      return;
    }
  }

  cpu_state.pending_exception = -1;

  if (cpu_state.power_down_flag)
  {
    return;
  }

  int f = 0;
  uint32_t inst = fetch_inst(&cpu_state, &f);
  if (f < 0)
    return;
  decode_inst(inst);
  cpu_state.cycles++;
}
