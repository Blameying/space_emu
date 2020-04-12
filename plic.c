#include "plic.h"
#include "riscv_definations.h"
#include "iomap.h"
#include "regs.h"
#include <stddef.h>

#define PLIC_HART_BASE 0x200000
#define PLIC_HART_SIZE 0x1000

static void plic_update_mip(cpu_state_t *state)
{
  uint32_t mask = state->plic_pending_irq & ~state->plic_served_irq;
  if (mask)
  {
    set_mip(state, MIP_MEIP | MIP_SEIP);
  }
  else
  {
    reset_mip(state, MIP_MEIP | MIP_SEIP);
  }
}

void plic_set_irq(cpu_state_t *state, int irq_num, int flag)
{
  uint32_t mask = 1 << (irq_num - 1);
  if (flag)
    state->plic_pending_irq |= mask;
  else
    state->plic_pending_irq &= ~mask;
  plic_update_mip(state);
}

static int plic_init(address_item_t *handler)
{
  return 0;
}

static int_t plic_read(address_item_t *handler, uint_t src, uint_t size, uint8_t *dst)
{
  if (handler == NULL || dst == NULL)
  {
    return -1;
  }
  if (size != 4)
  {
    return -1;
  }

  uint_t offset = src - handler->start_address;
  uint32_t result = 0;
  cpu_state_t *state = handler->cpu_state;
  switch(offset)
  {
    case PLIC_HART_BASE:
      result = 0;
      break;
    case PLIC_HART_BASE + 4:
      {
        uint32_t mask = state->plic_pending_irq & ~state->plic_served_irq;
        if (mask != 0) {
          int i = 0;
          for (; i < 32; i++)
          {
            if ((mask >> i) & 1)
              break;
          }
          state->plic_served_irq |= 1 << i;
          plic_update_mip(state);
          result = i + 1;
        }
        else
        {
          result = 0;
        }
      }
      break;
    default:
      result = 0;
      break;
  }
  uint32_t *ptr = (uint32_t*)dst;
  *ptr = result;
  return 0;
}


static int_t plic_write(address_item_t *handler, uint8_t *src, uint_t size, uint_t dst)
{
  if (handler == NULL || src == NULL)
  {
    return -1;
  }
  if (size != 4)
  {
    return -1;
  }
  uint_t offset = dst - handler->start_address;
  uint32_t value = *(uint32_t*)dst;
  cpu_state_t *state = handler->cpu_state;
  switch(offset)
  {
    case PLIC_HART_BASE + 4:
      value--;
      if (value < 32)
      {
        state->plic_served_irq &= ~(1 << value);
        plic_update_mip(state);
      }
      break;
    default:
      break;
  }
  return 0;
}

static void plic_release(address_item_t *handler)
{
  return;
}


address_item_t plic_item = {
  .name = "plic",
  .start_address = PLIC_BASE_ADDR,
  .size = PLIC_SIZE,
  .entity = NULL,
  .cpu_state = NULL,
  .init = plic_init,
  .write_bytes = plic_write,
  .read_bytes = plic_read,
  .release = plic_release
};


void plic_module_init(cpu_state_t *state)
{
  iomap_manager.register_address(state, &plic_item);
}


