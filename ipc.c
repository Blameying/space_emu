#include <sys/ipc.h>
#include <sys/shm.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

const key_t SHM_ID = 718;
const size_t SHM_SIZE = 2048;

int main()
{
  int shm_id = shmget(SHM_ID, SHM_SIZE, 0666|IPC_CREAT);
  void* shm_addr = NULL;

  if (shm_id == -1)
  {
    fprintf(stderr, "shmget failed\n");
    exit(EXIT_FAILURE);
  }

  shm_addr = shmat(shm_id, 0, 0);
  if (shm_addr == (void*)-1)
  {
    fprintf(stderr, "shmat failed\n");
    exit(EXIT_FAILURE);
  }

  uint32_t *value = (uint32_t*)shm_addr;
  *value = 718;

  while(*value != 0)
  {
    sleep(1);
  }

  printf("Success!\n");
  if (shmdt(shm_addr) == -1)
  {
    fprintf(stderr, "shmdt failed!\n");
    exit(EXIT_FAILURE);
  }

  if (shmctl(shm_id, IPC_RMID, 0) == -1)
  {
    fprintf(stderr, "shm rm  failed!\n");
    exit(EXIT_FAILURE);
  }
  exit(EXIT_SUCCESS);
}
