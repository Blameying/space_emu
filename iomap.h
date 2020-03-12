#ifndef __IOMAP_H__
#define __IOMAP_H__

#include "regs.h"

typedef struct address_handler
{
  uint_t start_address;
  uint_t size;
  uint_t (*read_bytes)(struct address_handler *handler, uint_t src, uint_t size, uint8_t *dst);
  uint_t (*write_bytes)(struct address_handler *handler, uint8_t *src, uint_t size, uint_t dst);
  void (*release)(void);
} address_handler_t;

typedef struct address_item
{
  char *name;
  uint_t start_address;
  uint_t size;
  address_handler_t *handler;
} address_item_t;

typedef struct iomap
{
  void (*register_address)(address_item_t *item);
  void (*release)(void);
  uint_t (*write)(uint_t address, uint_t size, uint8_t *src);
  uint_t (*read)(uint_t address, uint_t size, uint8_t *dst);
} iomap_t;

extern iomap_t iomap_manager;

#endif
