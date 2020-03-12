#include "test.h"
#include "stddef.h"
#include "memory.h"
#include "test_iomap.h"
#include <stdio.h>

test_entity_t *root = NULL;

void register_test_entity(test_entity_t *entity)
{
  test_entity_t *ptr = root;
  if (ptr == NULL)
  {
    root = entity;
    return;
  }
  
  while(ptr->next != NULL)
  {
    ptr = ptr->next;
  }

  ptr->next = entity;
  entity->next = NULL;

  return;
}

void execute_test()
{
  test_entity_t *ptr = root;
  while(ptr != NULL)
  {
    if (ptr->name){
      printf("test_entity %s: ", ptr->name);
    }
    else
    {
      printf("test_entity anonymous: ");
    }
    if (ptr->test_function)
    {
      int result = ptr->test_function();
      if (result == 0)
      {
        printf("Pass \n\r");
      }
      else
      {
        printf("Failed, ret = %d \n\r", result);
      }
    }
    else
    {
      printf("has no test function \n\r");
    }
    ptr = ptr->next;
  }
}

int main()
{
  memory_init();
  register_test_entity(&memory_test);
  register_test_entity(&iomap_test_entity);
  printf("start test ............\n\r");
  execute_test();
  printf("\n\rfinished!\n\r");
  return 0;
}
