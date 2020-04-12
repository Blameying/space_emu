#include <stdio.h>
#include "instructions.h"
#include "regs.h"
#include "iomap.h"

uint32_t fetch_inst(cpu_state_t *state)
{
  uint32_t inst = 0;
  int flag = 0;
  flag = iomap_manager.code_vaddr(state, state->pc, sizeof(inst), (uint8_t*)&inst);
  if (flag < 0)
  {
    state->pending_tval = state->pc;
    state->pending_exception = CAUSE_FAULT_FETCH;
    raise_exception(state, state->pending_exception, state->pending_tval);
    return 0;
  }
  return inst;
}

void decode_inst(uint32_t inst)
{
  uint32_t opcode = inst & 0x7F;
  uint32_t rd = (inst >> 7) & 0x1F;
  uint32_t funct3 = (inst >> 12) & 0x07;
  uint32_t rs1 = (inst >> 15) & 0x1F;
  uint32_t rs2 = (inst >> 20) & 0x1F;
  uint32_t funct7 = (inst >> 25) & 0x7F;
  int32_t  imm_I = (int32_t)(inst & 0xFFF00000) >> 20;
  int32_t  imm_S = (int32_t)(((inst >> 7) & 0x1F) | ((inst >> 20) & 0xFE0)) << 20 >> 20;
  int32_t  imm_B = (int32_t)(((inst >> 7) & 0x1E) | ((inst >> 20) & 0x7E0) | ((inst << 4) & 0x800) | ((inst >> 19) & 0x1000)) << 19 >> 19;
  int32_t  imm_U = (int32_t)(inst & 0xFFFFF000);
  int32_t  imm_J = (int32_t)(((inst >> 20) & 0x7FE) | ((inst >> 9) & 0x800) | (inst & 0xFF000) | ((inst >> 11) & 0x100000)) << 11 >> 11;
  
  switch(opcode)
  {
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
            val1 = (int_t)(val1) + val2;
            break;
          case 1: /* slli */
            if ((val2 & ~(XLEN - 1)) != 0)
            {
              goto ERROR_PROCESS;
            }
            val1 = (int_t)(val1 << (val2 & (XLEN - 1)));
            break;
          case 2: /* slti */
            val1 = (int_t)val1 < val2;
            break;
          case 3: /* sltiu */
            val1 = val1 < (uint_t)val2;
            break;
          case 4: /* xori */
            val1 = val1 ^ val2;
            break;
          case 5:
            if (funct7 == 0x0) /* srli */
            {
              val1 = (int_t)(val1 >> (val2 & (XLEN -1)));
            }
            else if (funct7 == 0x20) /* srai */
            {
              val1 = (int_t)val1 >> (val2 & (XLEN - 1));
            }
            else
            {
              goto ERROR_PROCESS;
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
        uint_t val = 0;
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
          printf("store data error, addr: %x\n", addr);
          cpu_state.pending_tval = addr;
          cpu_state.pending_exception = CAUSE_FAULT_STORE;
          goto Exception;
        }
      }
      break;
    case OP_LOAD:
      {
        uint_t addr = cpu_state.regs[rs1] + imm_S;
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
          printf("load data error, addr: %x\n", addr);
          cpu_state.pending_tval = addr;
          cpu_state.pending_exception = CAUSE_FAULT_LOAD;
          goto Exception;
        }
        if (rd != 0)
        {
          cpu_state.regs[rd] = val;
        }
      }
      break;
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
      cpu_state.pc = (int_t)(cpu_state.regs[rs1] + imm_J);
      goto JUMP;
    case OP_JALR: /* jalr */
      {
        uint_t val = cpu_state.pc + 4;
        cpu_state.pc = cpu_state.regs[rs1] + imm_I;
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
            default:
              goto ERROR_PROCESS;
          }
        case 1: /* csrrw */
          {
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
        case 0x102: /* sret */
          {
            if(inst & 0x000FFF80)
              goto ERROR_PROCESS;
            if (cpu_state.priv < PRIV_S)
              goto ERROR_PROCESS;
            handle_sret(&cpu_state);
            goto DONE_INTERP;
          }
          break;
        case 0x302: /* mret */
          {
            if(inst & 0x000FFF80)
              goto ERROR_PROCESS;
            if (cpu_state.priv < PRIV_M)
              goto ERROR_PROCESS;
            handle_mret(&cpu_state);
            goto DONE_INTERP;
          }
          break;
        case 0x105: /* wfi */
          if (inst & 0x00007F80)
            goto ERROR_PROCESS;
          if (cpu_state.priv == PRIV_U)
            goto ERROR_PROCESS;
          if ((cpu_state.mip & cpu_state.mie) == 0)
          {
            cpu_state.power_down_flag = true;
            cpu_state.pc += 4;
            goto DONE_INTERP;
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
            // JUMP INSN
          }
          else
          {
            goto ERROR_PROCESS;
          }
          break;
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
    default:
      goto ERROR_PROCESS;
  }
  cpu_state.pc += 4;
  return;

ERROR_PROCESS:
  printf("Error instructions decode! %x\n", inst);
  cpu_state.pending_exception = CAUSE_ILLEGAL_INSTRUCTION;
  cpu_state.pending_tval = inst;

Exception:
  if (cpu_state.pending_exception >= 0)
  {
    raise_exception(&cpu_state, cpu_state.pending_exception, cpu_state.pending_tval);
    return;
  }
DONE_INTERP:
JUMP:
  printf("jump instruction \n");
  return;
}

void machine_loop()
{
  // check interrupts
  if ((cpu_state.mip & cpu_state.mie) != 0)
  {
    if (raise_interrupt(&cpu_state))
    {
      return;
    }
  }

  uint32_t inst = fetch_inst(&cpu_state);
  decode_inst(inst);
}
