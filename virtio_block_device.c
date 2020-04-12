#include "virtio_block_device.h"
#include "virtio_interface.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int64_t bf_get_sector_count(block_device_t *bs)
{
  block_device_file_t *bf = bs->opaque;
  return bf->nb_sectors;
}

static int bf_read_async(block_device_t *bs, uint64_t sector_num,
    uint8_t *buf, int size,
    block_device_complete_func *cb, void *opaque)
{
  block_device_file_t *bf = bs->opaque;

  if (!bf->f)
    return -1;
  if (bf->mode == BF_MODE_SNAPSHOT)
  {
    int i;
    for (i = 0; i < size; i++)
    {
      if (!bf->sector_table[sector_num])
      {
        fseek(bf->f, sector_num * SECTOR_SIZE, SEEK_SET);
        fread(buf, 1, SECTOR_SIZE, bf->f);
      }
      else
      {
        memcpy(buf, bf->sector_table[sector_num], SECTOR_SIZE);
      }
      sector_num++;
      buf += SECTOR_SIZE;
    }
  }
  else
  {
    fseek(bf->f, sector_num * SECTOR_SIZE, SEEK_SET);
    fread(buf, 1, size * SECTOR_SIZE, bf->f);
  }
  return 0;
}

static int bf_write_async(block_device_t *bs, uint64_t sector_num, uint8_t *buf, int size, block_device_complete_func *cb, void *opaque)
{
  block_device_file_t *bf = bs->opaque;
  int ret;

  switch(bf->mode)
  {
    case BF_MODE_RO:
      ret = -1;
      break;
    case BF_MODE_RW:
      fseek(bf->f, sector_num * SECTOR_SIZE, SEEK_SET);
      fwrite(buf, 1, size * SECTOR_SIZE, bf->f);
      ret = 0;
      break;
    case BF_MODE_SNAPSHOT:
      {
        int i;
        if ((sector_num + size) > bf->nb_sectors)
          return -1;
        for (i = 0; i < size; i++)
        {
          if (!bf->sector_table[sector_num])
          {
            bf->sector_table[sector_num] = malloc(SECTOR_SIZE);
          }
          memcpy(bf->sector_table[sector_num], buf, SECTOR_SIZE);
          sector_num++;
          buf += SECTOR_SIZE;
        }
        ret = 0;
      }
      break;
    default:
      abort();
  }

  return ret;
}

static block_device_t *block_device_init(const char *filename, block_device_mode_enum mode)
{
  block_device_t *bs;
  block_device_file_t *bf;
  int64_t file_size;
  FILE *f;
  const char *mode_str;

  if (mode == BF_MODE_RW)
  {
    mode_str = "r+b";
  }
  else
  {
    mode_str = "rb";
  }

  f = fopen(filename, mode_str);
  if (!f)
  {
    perror(filename);
    exit(1);
  }

  fseek(f, 0, SEEK_END);
  file_size = ftell(f);

  bs = malloc(sizeof(*bs));
  memset(bs, 0, sizeof(*bs));

  bf = malloc(sizeof(*bf));
  memset(bf, 0, sizeof(*bf));

  bf->mode = mode;
  bf->nb_sectors = file_size / 512;
  bf->f = f;

  if (mode == BF_MODE_SNAPSHOT)
  {
    bf->sector_table = malloc(sizeof(bf->sector_table[0]) * bf->nb_sectors);
    memset(bf->sector_table, 0, sizeof(bf->sector_table[0]) * bf->nb_sectors);
  }

  bs->opaque = bf;
  bs->get_sector_count = bf_get_sector_count;
  bs->read_async = bf_read_async;
  bs->write_async = bf_write_async;

  return bs;
}

int block_init(address_item_t *handler)
{
  return true;
}

void block_release(address_item_t *handler)
{
  virtual_io_block_device_t *vbd = (virtual_io_block_device_t*)handler->entity;
  free(vbd);
}

address_item_t block_item = 
{
  .name = "harddisk",
  .init = block_init,
  .release = block_release
};

void virtual_block_device_init(cpu_state_t *state, const char *filename, block_device_mode_enum mode, virtual_io_bus_t *bus)
{
  virtual_io_block_device_t *vbd;
  uint64_t nb_sectors;

  vbd = malloc(sizeof(*vbd));
  memset(vbd, 0, sizeof(*vbd));
  vbd->bs = block_device_init(filename, mode);
  virtio_init(state, &block_item, &vbd->common, bus, 2, 8, virtual_block_recv_request);

  nb_sectors = vbd->bs->get_sector_count(vbd->bs);
  uint8_t *ptr = vbd->common.config_space;
  int i;
  for (i = 0; i < 8; i++)
  {
    ptr[i] = nb_sectors >> (i * 8);
  }

  return;
}
