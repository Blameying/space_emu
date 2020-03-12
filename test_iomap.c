#include "iomap.h"
#include <string.h>
#include "test.h"

static int iomap_test()
{
  char *value = "iomap test";
  iomap_manager.write(512, strlen(value) + 1, (uint8_t*)value);
  char buffer[20];
  memset(buffer, 0, sizeof(buffer));
  iomap_manager.read(512, strlen(value) + 1, (uint8_t*)buffer);

  return strcmp(value, (char*)buffer);
}

test_entity_t iomap_test_entity = {
  .name = "iomap",
  .test_function = iomap_test,
  .next = NULL
};

