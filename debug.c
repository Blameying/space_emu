#include "debug.h"
#include "iomap.h"
#include "riscv_definations.h"
#include <stdio.h>

static int debug_init(address_item_t *handler)
{
  return true;
}

static int_t debug_read(address_item_t *handler, uint_t src, uint_t size, uint8_t *dst)
{
  return 1;
}

static int_t debug_write(address_item_t *handler, uint8_t *src, uint_t size, uint_t dst)
{
  if (size <= PRINT_SIZE)
  {
    uint8_t *ptr = src;
    for(; size > 0; size--)
    {
      putchar(*ptr);
      ptr++;
    }
  }
  else
  {
    return -1; 
  }

  return size;
}

static void debug_release(address_item_t *handler)
{
  return;
}

address_item_t debug_item = 
{
  .name = "debug print",
  .start_address = PRINT_DEVICE,
  .size = PRINT_SIZE,
  .entity = NULL,
  .cpu_state = NULL,
  .init = debug_init,
  .read_bytes = debug_read,
  .write_bytes = debug_write,
  .release = debug_release
};


void debug_module_init(cpu_state_t *state)
{
  iomap_manager.register_address(state, &debug_item);
}

