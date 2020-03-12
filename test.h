#ifndef __TEST_H__
#define __TEST_H__

typedef struct test_entity
{
  char *name;
  int (*test_function)(void);
  struct test_entity *next;
} test_entity_t;
#endif
