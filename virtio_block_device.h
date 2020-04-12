#ifndef __VIRTIO_BLOCK_H__
#define __VIRTIO_BLOCK_H__

#include <stdio.h>
#include <stdint.h>
#include "virtio_interface.h"
#include "riscv_definations.h"

#define SECTOR_SIZE 512
typedef struct block_device block_device_t;
typedef void block_device_complete_func(void *opaque, int ret);

typedef enum {
    BF_MODE_RO,
    BF_MODE_RW,
    BF_MODE_SNAPSHOT,
} block_device_mode_enum;

typedef struct block_device_file
{
  FILE *f;
  int64_t nb_sectors;
  block_device_mode_enum mode;
  uint8_t **sector_table;
} block_device_file_t;

typedef struct virtual_io_block_device
{
  virtual_io_device_t common;
  block_device_t *bs;

  bool req_in_progress;
  block_request_t req;
} virtual_io_block_device_t;

struct block_device
{
  int64_t (*get_sector_count)(block_device_t *bs);
  int (*read_async)(block_device_t *bs, uint64_t sector_num,
      uint8_t *buf, int size, block_device_complete_func *cb, void *opaque);
  int (*write_async)(block_device_t *bs, uint64_t sector_num,
      uint8_t *buf, int size, block_device_complete_func *cb, void *opaque);
  void *opaque;
};

extern void virtual_block_device_init(cpu_state_t *state, const char *filename, block_device_mode_enum mode, virtual_io_bus_t *bus);
#endif
