#include <stdio.h>
#include "instructions.h"
#include "regs.h"
#include "iomap.h"

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
    case OP_STORE:
      {
        uint_t addr = cpu_state.regs[rs1] + imm_S;
        uint_t val2 = cpu_state.regs[rs2];
        switch(funct3)
        {
          case 0: /* sb */
            iomap_manager.write(addr, 1, (uint8_t*)&val2);
            break;
          case 1: /* sw */
            iomap_manager.write(addr, 2, (uint8_t*)&val2);
            break;
          case 2: /* sh */
            iomap_manager.write(addr, 4, (uint8_t*)&val2);
            break;
          default:
            goto ERROR_PROCESS;
        }
      }
      break;
    case OP_LOAD:
      {
        uint_t addr = cpu_state.regs[rs1] + imm_S;
        uint_t val = 0;
        switch(funct3)
        {
          case 0x0: /* lb */
            {
              uint8_t ret = 0;
              iomap_manager.read(addr, sizeof(ret), &ret);
              val = (int8_t)ret;
            } 
            break;
          case 0x1: /* lh */
            {
              uint16_t ret = 0;
              iomap_manager.read(addr, sizeof(ret), (uint8_t*)&ret);
              val = (int16_t)ret;
            }
            break;
          case 0x2: /* lw */
            {
              uint32_t ret = 0;
              iomap_manager.read(addr, sizeof(ret), (uint8_t*)&ret);
              val = (int32_t)ret;
            }
            break;
          case 0x4: /* lbu */
            {
              uint8_t ret = 0;
              iomap_manager.read(addr, sizeof(ret), &ret);
              val = ret;
            }
            break;
          case 0x5: /* lhu */
            {
              uint16_t ret = 0;
              iomap_manager.read(addr, sizeof(ret), (uint8_t*)&ret);
              val = ret;
            }
            break;
          case 0x6: /* lwu */
            {
              uint32_t ret = 0;
              iomap_manager.read(addr, sizeof(ret), (uint8_t*)&ret);
              val = ret;
            }
            break;
          default:
            goto ERROR_PROCESS;
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




  }

ERROR_PROCESS:
  printf("Error instructions decode! \n");


JUMP:
  printf("jump instruction \n");
}
