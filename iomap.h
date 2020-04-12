#ifndef __IOMAP_H__
#define __IOMAP_H__

#include "regs.h"

typedef struct address_item address_item_t;
typedef struct address_item
{
  char *name;
  uint_t start_address;
  uint_t size;
  uint8_t *entity;
  cpu_state_t *cpu_state;
  int (*init)(address_item_t *handler);
  int_t (*read_bytes)(address_item_t *handler, uint_t src, uint_t size, uint8_t *dst);
  int_t (*write_bytes)(address_item_t *handler, uint8_t *src, uint_t size, uint_t dst);
  void (*release)(address_item_t *handler);
} address_item_t;

typedef struct iomap
{
  void (*register_address)(cpu_state_t *state, address_item_t *item);
  void (*release)(void);
  int_t (*write)(uint_t address, uint_t size, uint8_t *src);
  int_t (*read)(uint_t address, uint_t size, uint8_t *dst);
  int_t (*write_vaddr)(cpu_state_t *state, uint_t vaddress, uint_t size, uint8_t *src);
  int_t (*read_vaddr)(cpu_state_t *state, uint_t vaddress, uint_t size, uint8_t *dst);
  int_t (*code_vaddr)(cpu_state_t *state, uint_t vaddress, uint_t size, uint8_t *dst);
  address_item_t *(*get_address_item)(cpu_state_t *state, uint_t address);
} iomap_t;

extern iomap_t iomap_manager;

#endif
