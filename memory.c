#include "regs.h"
#include "memory.h"
#include "iomap.h"
#include <string.h>
#include <stdio.h>

static uint8_t memory_entity[MEMORY_SIZE * (1 << 20)];

int check_valid(uint_t size)
{
  return size <= sizeof(memory_entity);
}

uint_t memory_write(address_handler_t *handler, uint8_t *src, uint_t size, uint_t dst)
{
  if (src == NULL || !check_valid((dst + size) - handler->start_address))
  {
    return 0;
  }
  uint_t pos = dst - handler->start_address;
  
  uint8_t *ptr = &memory_entity[pos];
  for(; size > 0; size--)
  {
    *ptr++ = *src++;
  }

  return size;
}

uint_t memory_read(address_handler_t *handler, uint_t src, uint_t size, uint8_t *dst)
{
  if (dst == NULL || !check_valid((src + size) - handler->start_address))
  {
    return 0;
  }

  uint_t pos = src - handler->start_address;
  uint8_t *ptr = &memory_entity[pos];
  for(; size > 0; size--)
  {
    *dst++ = *ptr++;
  }

  return size;
}

void memory_release()
{
  return;
}

static address_handler_t memory_handler = {
  .start_address = 0,
  .size = sizeof(memory_entity),
  .write_bytes = memory_write,
  .read_bytes = memory_read,
  .release = memory_release
};

static address_item_t memory_item = {
  .name = "ram",
  .start_address = 0,
  .size = sizeof(memory_entity),
  .handler = &memory_handler
};

void memory_init()
{
  memset(memory_entity, 0, sizeof(memory_entity));
  iomap_manager.register_address(&memory_item);
}

int test_memory()
{
  char *text = "hello world";
  memory_item.handler->write_bytes(&memory_handler, text, strlen(text) + 1, 1024);
  printf("memory write: %s\n", text);
  char buffer[20];
  memset(buffer, 0, sizeof(buffer));

  memory_item.handler->read_bytes(&memory_handler, 1024, strlen(text) + 1, (uint8_t*)buffer);
  printf("memory read: %s\n", buffer);
  if (strcmp(text, (uint8_t*)buffer))
  {
    return 1;
  }
  return 0;
}

test_entity_t memory_test = {
  .name = "memory io test",
  .test_function = test_memory,
  .next = NULL
};
