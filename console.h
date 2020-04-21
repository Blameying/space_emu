#ifndef __CONSOLE_H__
#define __CONSOLE_H__
#include "virtio_interface.h"

typedef struct 
{
  void *opaque;
  int_t (*write_data)(void *opaque, const uint8_t *buf, int len);
  int_t (*read_data)(void *opaque, uint8_t *buf, int len);
} character_device_t;

typedef struct
{
  int stdin_fd;
  int console_esc_state;
  bool resize_pending;
} stdio_device_t;

typedef struct virtio_console_device
{
  virtual_io_device_t common;
  character_device_t *cs;
} virtio_console_device_t;

extern void virtual_console_device_init(cpu_state_t *state, virtual_io_bus_t *bus);
extern void console_get_size(stdio_device_t *dev, int *pw, int *ph);
#endif
