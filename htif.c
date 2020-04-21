#include "htif.h"
#include "riscv_definations.h"
#include "iomap.h"
#include "regs.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

static void htif_cmd_handler(cpu_state_t *state)
{
  uint32_t device, cmd;
  device = state->htif_tohost >> 56;
  cmd = (state->htif_tohost >> 48) & 0xff;
  if (state->htif_tohost == 1)
  {
    printf("\n Power Off.\n");
    exit(0);
  }
  else if (device == 1 && cmd == 1)
  {
    uint8_t buf[1];
    buf[0] = state->htif_tohost & 0xff;
    /* TODO: console out */
    (void)buf;
    state->htif_tohost =  0;
    state->htif_fromhost = ((uint64_t)device << 56) | ((uint64_t)cmd << 48);
  }
  else if (device == 1 && cmd == 0)
  {
    state->htif_tohost = 0;
  }
  else
  {
    printf("HTIF: unsupported tohost=0x%016lx\n", state->htif_tohost);
  }
}

static int htif_init(address_item_t *handler)
{
  return true;
}

static int_t htif_read(address_item_t *handler, uint_t src, uint_t size, uint8_t *dst)
{
  if (handler == NULL || dst == NULL)
    return -1;
  if (size != 4)
    return -1;
  uint_t offset = src - handler->start_address;
  cpu_state_t *state = handler->cpu_state;
  uint32_t result = 0;
  switch(offset)
  {
    case 0:
      result = state->htif_tohost;
      break;
    case 4:
      result = state->htif_tohost >> 32;
      break;
    case 8:
      result = state->htif_fromhost;
      break;
    case 12:
      result = state->htif_fromhost >> 32;
      break;
    default:
      result = 0;
      return -1;
  }
  uint32_t *ptr = (uint32_t*)dst;
  *ptr = result;
  return size;
}


static int_t htif_write(address_item_t *handler, uint8_t *src, uint_t size, uint_t dst)
{
  if (handler == NULL || src == NULL)
    return -1;
  if (size != 4)
    return -1;
  uint_t offset = dst - handler->start_address;
  cpu_state_t *state = handler->cpu_state;
  uint32_t value = *(uint32_t*)src;
  switch(offset)
  {
    case 0:
      state->htif_tohost = (state->htif_tohost & ~0xffffffff) | value;
      break;
    case 4:
      state->htif_tohost = (state->htif_tohost & 0xffffffff) | ((uint64_t)value << 32);
      htif_cmd_handler(state);
      break;
    case 8:
      state->htif_fromhost = (state->htif_fromhost & ~0xffffffff) | value;
      break;
    case 12:
      state->htif_fromhost = (state->htif_fromhost & 0xffffffff) | (uint64_t)value << 32;
      break;
    default:
      return -1;
  }

  return 4;
}

static void htif_release(address_item_t *handler)
{
  return;
}


address_item_t htif_item = {
  .name = "htif",
  .start_address = HTIF_BASE_ADDR,
  .size = HTIF_SIZE,
  .entity = NULL,
  .cpu_state = NULL,
  .init = htif_init,
  .write_bytes = htif_write,
  .read_bytes = htif_read,
  .release = htif_release
};


void htif_module_init(cpu_state_t *state)
{
  iomap_manager.register_address(state, &htif_item);
}


