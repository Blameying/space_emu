#include "iomap.h"
#include <stddef.h>
#include <stdlib.h>

#define ADDRESS_ITEM_COUNT 1024

static address_item_t *address_items[ADDRESS_ITEM_COUNT];

static int check_overlap(address_item_t *item0, address_item_t *item1)
{
  uint_t address_1[2] = {item0->start_address, item0->start_address + item0->size};
  uint_t address_2[2] = {item1->start_address, item1->start_address + item1->size};
  if ((address_2[0] > address_1[1]) || (address_1[0] > address_2[1]))
  {
    return 0;
  }

  return 1;
}

static int check_in(address_item_t *item0, uint_t start, uint_t size)
{
  uint_t address_1[2] = {item0->start_address, item0->start_address + item0->size};
  if (start >= address_1[0] && (start + size) <= address_1[1])
  {
    return 1;
  }
  return 0;
}

static void register_address_manager(address_item_t *item)
{
  int i = 0;
  for(; i < ADDRESS_ITEM_COUNT; i++)
  {
    if (address_items[i] != NULL) 
    {
      if (check_overlap(address_items[i], item))
        return;
    }
    else
    {
      address_items[i] = item;
      return;
    }
  }
}

static void release_address_manager()
{
  int i = 0;
  for(; i < ADDRESS_ITEM_COUNT; i++)
  {
    if (address_items[i] != NULL)
    {
      if (address_items[i]->handler != NULL && address_items[i]->handler->release != NULL)
        address_items[i]->handler->release();
    }
    else
    {
      break;
    }
  }
}

static uint_t write_bytes(uint_t address, uint_t size, uint8_t *src)
{
  int i = 0;
  for (; i < ADDRESS_ITEM_COUNT; i++)
  {
    if (address_items[i] != NULL && check_in(address_items[i], address, size))
    {
      return address_items[i]->handler->write_bytes(address_items[i]->handler, src, size, address);
    }
    if (address_items[i] == NULL)
      break;
  }

  return 0;
}

static uint_t read_bytes(uint_t address, uint_t size, uint8_t *dst)
{
  int i = 0;
  for (; i < ADDRESS_ITEM_COUNT; i++)
  {
    if (address_items[i] != NULL && check_in(address_items[i], address, size))
    {
      return address_items[i]->handler->read_bytes(address_items[i]->handler, address, size, dst);
    }
    if (address_items[i] == NULL)
    {
      break;
    }
  }

  return 0;
}

iomap_t iomap_manager = {
  .register_address = register_address_manager,
  .release = release_address_manager,
  .write = write_bytes,
  .read = read_bytes
};
