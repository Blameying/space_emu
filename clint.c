#include "clint.h"
#include "riscv_definations.h"
#include "iomap.h"
#include "regs.h"
#include <stddef.h>
#include <time.h>
#include <stdio.h>

static uint64_t rtc_get_real_time()
{
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint64_t)ts.tv_sec * RTC_FREQ + \
    (ts.tv_nsec / (1000000000 / RTC_FREQ));
}

uint64_t rtc_get_time(cpu_state_t *state)
{
  return rtc_get_real_time() - state->rtc_start_time;
}

static int clint_init(address_item_t *handler)
{
  handler->cpu_state->rtc_start_time = rtc_get_real_time();
  return true;
}

static int_t clint_read_sub(address_item_t *handler, uint_t src, uint_t size, uint8_t *dst)
{
  if (handler == NULL || dst == NULL)
    return -1;
  uint32_t result = 0;
  if (size != 4)
  {
    perror("clint data size align error!");
    return -1;
  }
  uint_t offset = src - handler->start_address;
  switch(offset)
  {
    case 0xbff8:
      result = rtc_get_time(handler->cpu_state);
      break;
    case 0xbffc:
      result = rtc_get_time(handler->cpu_state) >> 32;
      break;
    case 0x4000:
      result = handler->cpu_state->mtimecmp;
      break;
    case 0x4004:
      result = handler->cpu_state->mtimecmp >> 32;
      break;
    default:
      result = 0;
      break;
  }
  uint8_t *ptr = (uint8_t*)&result;
  int i = 0;
  for (; i < 4; i++)
  {
    *dst++ = *ptr++;
  }
  return 4;
}

static int_t clint_read(address_item_t *handler, uint_t src, uint_t size, uint8_t *dst)
{
  int_t ret_size = 0;
  uint_t cp_size = size;
  while(cp_size >= 4)
  {
    ret_size += clint_read_sub(handler, src, 4, dst);
    src += 4;
    dst += 4;
    cp_size -= 4;
  }
  if (ret_size != size)
  {
    return -1;
  }

  return size;
}

static int_t clint_write_sub(address_item_t *handler, uint8_t *src, uint_t size, uint_t dst)
{
  if (handler == NULL || src == NULL)
    return -1;
  if (size != 4)
    return -1;
  uint_t offset = dst - handler->start_address;
  cpu_state_t *state = handler->cpu_state;
  uint32_t *value = (uint32_t*)src;
  switch(offset)
  {
    case 0x4000:
      state->mtimecmp = (state->mtimecmp & ~0xffffffff) | (*value);
      reset_mip(state, MIP_MTIP);
      break;
    case 0x4004:
      state->mtimecmp = (state->mtimecmp & 0xffffffff) | ((uint64_t)(*value) << 32);
      reset_mip(state, MIP_MTIP);
      break;
    default:
      break;
  }
  return 4;
}

static int_t clint_write(address_item_t *handler, uint8_t *src, uint_t size, uint_t dst)
{
  int_t ret_size = 0;
  uint_t cp_size = size;
  while(cp_size >= 4)
  {
    ret_size += clint_write_sub(handler, src, 4, dst);
    src += 4;
    dst += 4;
    cp_size -= 4;
  }
  if (ret_size != size)
  {
    return -1;
  }

  return size;
}

static void clint_release(address_item_t *handler)
{
  return;
}


address_item_t clint_item = {
  .name = "clint",
  .start_address = CLINT_BASE_ADDR,
  .size = CLINT_SIZE,
  .entity = NULL,
  .cpu_state = NULL,
  .init = clint_init,
  .write_bytes = clint_write,
  .read_bytes = clint_read,
  .release = clint_release
};


void clint_module_init(cpu_state_t *state)
{
  iomap_manager.register_address(state, &clint_item);
}


