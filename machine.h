#ifndef __MACHINE_H__
#define __MACHINE_H__
#include "regs.h"
#include "virtio_block_device.h"
#include "console.h"

typedef struct
{
  cpu_state_t *cpu_state;
  virtio_console_device_t *console;
  virtual_io_block_device_t *block;
} machine_t;

extern machine_t riscv_machine;
#endif
