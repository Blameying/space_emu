#include "regs.h"
#include "memory.h"
#include "iomap.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "riscv_definations.h"

static int check_valid(uint_t max, uint_t size)
{
  return (size <= max);
}

static int_t memory_write(address_item_t *handler, uint8_t *src, uint_t size, uint_t dst)
{
  if (src == NULL || !check_valid(handler->size, (dst + size) - handler->start_address))
  {
    return -1;
  }
  uint_t pos = dst - handler->start_address;
  
  uint8_t *ptr = &(handler->entity[pos]);
  for(; size > 0; size--)
  {
    *ptr++ = *src++;
  }

  return size;
}

static int_t memory_read(address_item_t *handler, uint_t src, uint_t size, uint8_t *dst)
{
  if (dst == NULL || !check_valid(handler->size, (src + size) - handler->start_address))
  {
    return -1;
  }

  uint_t pos = src - handler->start_address;
  uint8_t *ptr = &(handler->entity[pos]);
  for(; size > 0; size--)
  {
    *dst++ = *ptr++;
  }

  return size;
}

static int memory_init(address_item_t *handler)
{
  if (handler == NULL)
    return false;

  if (handler->entity != NULL)
    return true;

  handler->entity = malloc(handler->size);
  if (handler->entity == NULL)
    return false;

  memset(handler->entity, 0, handler->size);
  return true;
}

void memory_release(address_item_t *handler)
{
  free(handler->entity);
  return;
}

static address_item_t memory_item = {
  .name = "ram",
  .start_address = RAM_BASE_ADDR,
  .size = MEMORY_SIZE,
  .entity = NULL,
  .init = memory_init,
  .write_bytes = memory_write,
  .read_bytes = memory_read,
  .release = memory_release
};

static address_item_t low_memory_item = {
  .name = "low memory",
  .start_address = 0,
  .size = LOW_RAM_SIZE,
  .entity = NULL,
  .init = memory_init,
  .write_bytes = memory_write,
  .read_bytes = memory_read,
  .release = memory_release
};

void memory_module_init(cpu_state_t *state)
{
  iomap_manager.register_address(state, &low_memory_item);
  iomap_manager.register_address(state, &memory_item);
}

