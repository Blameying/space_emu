#include "virtio_interface.h"
#include "virtio_block_device.h"
#include "riscv_definations.h"
#include "iomap.h"
#include "regs.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>


static void virtual_io_reset(virtual_io_device_t *device)
{
  int i;
  device->status = 0;
  device->queue_select = 0;
  device->device_features_select = 0;
  device->int_status = 0;
  for (i = 0; i < MAX_QUEUE; i++)
  {
    queue_state_t *qs = &device->queue[i];
    qs->ready = 0;
    qs->num = MAX_QUEUE_NUM;
    qs->desc_addr = 0;
    qs->avail_addr = 0;
    qs->used_addr = 0;
    qs->last_avail_idx = 0;
  }
}

static inline int min_int(int a, int b)
{
  return a < b? a:b;
}

static int virtio_memcpy_from_ram(virtual_io_device_t *device, uint8_t *buf, uint_t addr, int count)
{
  int len;
  while(count > 0)
  {
    len = min_int(count, VIRTIO_PAGE_SIZE - (addr & (VIRTIO_PAGE_SIZE - 1)));
    iomap_manager.read_vaddr(device->cpu_state, addr, len, buf);
    addr += len;
    buf += len;
    count -= len;
  }
  return 0;
}

static int virtio_memcpy_to_ram(virtual_io_device_t *device, uint_t addr, const uint8_t *buf, int count)
{
  int len;
  while(count > 0)
  {
    len = min_int(count, VIRTIO_PAGE_SIZE - (addr & (VIRTIO_PAGE_SIZE - 1)));
    iomap_manager.write_vaddr(device->cpu_state, addr, len, (uint8_t*)buf);
    addr += len;
    buf += len;
    count -= len;
  }
  return 0;
}

static int get_desc(virtual_io_device_t *device, virtual_io_desc_t *desc, int queue_idx, int desc_idx)
{
  queue_state_t *qs = &device->queue[queue_idx];
  return virtio_memcpy_from_ram(device, (void*)desc,
      qs->desc_addr + desc_idx * sizeof(virtual_io_desc_t),
      sizeof(virtual_io_desc_t));
}

static int virtual_memcpy_to_from_queue(virtual_io_device_t *device, uint8_t *buf,
    int queue_idx, int desc_idx, int offset,
    int count, bool to_queue)
{
  virtual_io_desc_t desc;
  int len, f_write_flag;

  if (count == 0)
    return 0;

  get_desc(device, &desc, queue_idx, desc_idx);

  if (to_queue)
  {
    f_write_flag = VRING_DESC_F_WRITE;
    for(;;)
    {
      if ((desc.flags & VRING_DESC_F_WRITE) == f_write_flag)
        break;
      if (!(desc.flags & VRING_DESC_F_NEXT))
        return -1;
      desc_idx = desc.next;
      get_desc(device, &desc, queue_idx, desc_idx);
    }
  }
  else
  {
    f_write_flag = 0;
  }

  for(;;)
  {
    if ((desc.flags & VRING_DESC_F_WRITE) != f_write_flag)
      return -1;
    if (offset < desc.len)
      break;
    if (!(desc.flags & VRING_DESC_F_NEXT))
      return -1;
    desc_idx = desc.next;
    offset = desc.len;
    get_desc(device, &desc, queue_idx, desc_idx);
  }

  for(;;)
  {
    len = min_int(count, desc.len - offset);
    if (to_queue)
      virtio_memcpy_to_ram(device, desc.addr + offset, buf, len);
    else
      virtio_memcpy_from_ram(device, buf, desc.addr + offset, len);
    count -= len;
    if (count == 0)
      break;
    offset += len;
    buf += len;
    if (offset == desc.len)
    {
      if (!(desc.flags & VRING_DESC_F_NEXT))
        return -1;
      desc_idx = desc.next;
      get_desc(device, &desc, queue_idx, desc_idx);
      if ((desc.flags & VRING_DESC_F_WRITE) != f_write_flag)
        return -1;
      offset = 0;
    }
  }

  return 0;
}

static int virtual_memcpy_to_queue(virtual_io_device_t *device,
    int queue_idx, int desc_idx, int offset, const void *buf, int count)
{
  return virtual_memcpy_to_from_queue(device, (void*)buf, queue_idx, desc_idx, offset, count, true);
}

static int virtual_memcpy_from_queue(virtual_io_device_t *device, void *buf,
    int queue_idx, int desc_idx, int offset, int count)
{
  return virtual_memcpy_to_from_queue(device, buf, queue_idx, desc_idx, offset, count, false);
}

static int get_desc_rw_size(cpu_state_t *state, virtual_io_device_t *device,
    int *pread_size, int *pwrite_size,
    int queue_idx, int desc_idx)
{
  virtual_io_desc_t desc;
  int read_size, write_size;

  read_size = 0;
  write_size = 0;
  get_desc(device, &desc, queue_idx, desc_idx);

  for(;;)
  {
    if(desc.flags & VRING_DESC_F_WRITE)
      break;
    read_size = desc.len;
    if (!(desc.flags & VRING_DESC_F_NEXT))
      goto done;
    desc_idx = desc.next;
    get_desc(device, &desc, queue_idx, desc_idx);
  }

  for(;;)
  {
    if (!(desc.flags & VRING_DESC_F_WRITE))
      return -1;
    write_size += desc.len;
    if(!(desc.flags & VRING_DESC_F_NEXT))
      break;
    desc_idx = desc.next;
    get_desc(device, &desc, queue_idx, desc_idx);
  }

done:
  *pread_size = read_size;
  *pwrite_size = write_size;
  return 0;
}
  
static void queue_notify(virtual_io_device_t *device, int queue_idx)
{
  queue_state_t *qs = &device->queue[queue_idx];
  uint16_t avail_idx;
  int desc_idx, read_size, write_size;

  if (qs->manual_recv)
    return;

  int_t status = iomap_manager.read_vaddr(device->cpu_state, qs->avail_addr + 2, 2, (uint8_t*)&avail_idx);
  if (status < 0)
  {
    abort();
  }
  while(qs->last_avail_idx != avail_idx)
  {
    status = iomap_manager.read_vaddr(device->cpu_state, qs->avail_addr + 4 + (qs->last_avail_idx & (qs->num - 1)) * 2, 2, (uint8_t*)&desc_idx);
    if (status < 0)
      abort();
    if (!get_desc_rw_size(device->cpu_state, device, &read_size, &write_size, queue_idx, desc_idx))
    {
      if (device->device_recv(device, queue_idx, desc_idx, read_size, write_size) < 0)
        break;
    }
    qs->last_avail_idx++;
  }
}

static uint32_t virtual_config_read(virtual_io_device_t *device, uint32_t offset, int size)
{
  uint32_t value;
  switch(size)
  {
    case 1:
      if (offset < device->config_space_size)
      {
        value = device->config_space[offset];
      }
      else
      {
        value = 0;
      }
      break;
    case 2:
      if (offset < (device->config_space_size - 1))
      {
        uint8_t *ptr = (uint8_t*)&device->config_space[offset];
        value = ptr[0] | ptr[1] << 8;
      }
      else
      {
        value = 0;
      }
      break;
    case 4:
      if (offset < (device->config_space_size - 3))
      {
        uint8_t *ptr = (uint8_t*)(device->config_space + offset);
        value = ptr[0] | ptr[1] << 8 | ptr[2] << 16 | ptr[3] << 24;
      }
      else
      {
        value = 0;
      }
      break;
    default:
      abort();
  }
  return value;
}

static void virtual_config_write(virtual_io_device_t *device, uint32_t offset , uint32_t value, int size)
{
  switch(size)
  {
    case 1:
      if (offset < device->config_space_size)
      {
        device->config_space[offset] = value;
        if(device->config_write)
          device->config_write(device);
      }
      break;
    case 2:
      if (offset < device->config_space_size - 1)
      {
        uint8_t *ptr = (uint8_t*)(device->config_space + offset);
        ptr[0] = value;
        ptr[1] = value >> 8;
        if (device->config_write)
          device->config_write(device);
      }
      break;
    case 4:
      if (offset < device->config_space_size - 3)
      {
        uint8_t *ptr = (uint8_t*)(device->config_space + offset);
        ptr[0] = value;
        ptr[1] = value >> 8;
        ptr[2] = value >> 16;
        ptr[3] = value >> 24;
        if (device->config_write)
          device->config_write(device);
      }
      break;
    default:
      abort();
  }
}

int_t virtual_mmio_read(address_item_t *handler, uint_t src, uint_t size, uint8_t *dst)
{
  if (handler == NULL || dst == NULL)
    return -1;
  virtual_io_device_t *device = (virtual_io_device_t*)handler->entity;
  uint32_t value = 0;
  uint32_t offset = src - handler->start_address;
  int i = 0;
  if (offset >= VIRTIO_MMIO_CONFIG)
    return virtual_config_read(device, offset - VIRTIO_MMIO_CONFIG, size);

  if (size == 4)
  {
    switch(offset)
    {
      case VIRTIO_MMIO_MAGIC_VALUE:
        value = 0x74726976;
        break;
      case VIRTIO_MMIO_VERSION:
        value = 2;
        break;
      case VIRTIO_MMIO_DEVICE_ID:
        value = device->device_id;
        break;
      case VIRTIO_MMIO_VENDOR_ID:
        value = device->vendor_id;
        break;
      case VIRTIO_MMIO_DEVICE_FEATURES:
        switch(device->device_features_select)
        {
          case 0:
            value = device->device_features;
            break;
          case 1:
            value = 1;
            break;
          default:
            value = 0;
            break;
        }
        break;
      case VIRTIO_MMIO_DEVICE_FEATURES_SEL:
        value = device->device_features_select;
        break;
      case VIRTIO_MMIO_QUEUE_SEL:
        value = device->queue_select;
        break;
      case VIRTIO_MMIO_QUEUE_NUM_MAX:
        value = MAX_QUEUE_NUM;
        break;
      case VIRTIO_MMIO_QUEUE_NUM:
        value = device->queue[device->queue_select].num;
        break;
      case VIRTIO_MMIO_QUEUE_DESC_LOW:
        value = device->queue[device->queue_select].desc_addr;
        break;
      case VIRTIO_MMIO_QUEUE_AVAIL_LOW:
        value = device->queue[device->queue_select].avail_addr;
        break;
      case VIRTIO_MMIO_QUEUE_USED_LOW:
        value = device->queue[device->queue_select].used_addr;
        break;
      case VIRTIO_MMIO_QUEUE_DESC_HIGH:
        value = device->queue[device->queue_select].desc_addr >> 32;
        break;
      case VIRTIO_MMIO_QUEUE_AVAIL_HIGH:
        value = device->queue[device->queue_select].avail_addr >> 32;
        break;
      case VIRTIO_MMIO_QUEUE_USED_HIGH:
        value = device->queue[device->queue_select].used_addr >> 32;
        break;
      case VIRTIO_MMIO_QUEUE_READY:
        value = device->queue[device->queue_select].ready;
        break;
      case VIRTIO_MMIO_INTERRUPT_STATUS:
        value = device->int_status;
        break;
      case VIRTIO_MMIO_STATUS:
        value = device->status;
        break;
      case VIRTIO_MMIO_CONFIG_GENERATION:
        value = 0;
        break;
      default:
        value = 0;
        break;
    }
  }
  else
  {
    value = 0;
  }

  uint8_t *ptr = (uint8_t*)&value;
  for (i = 0; i < size; i++)
  {
    *dst++ = *ptr++;
  }

  return size;
}

static void set_low32(uint64_t *addr, uint32_t value)
{
  *addr = (*addr & ~(uint64_t)0xffffffff) | value;
}

static void set_high32(uint64_t *addr, uint32_t value)
{
  *addr = (*addr & 0xffffffff) | ((uint64_t)value << 32);
}

int_t virtual_mmio_write(address_item_t *handler, uint8_t *src, uint_t size, uint_t dst)
{
  if (handler == NULL || src == NULL)
    return -1;
  virtual_io_device_t *device = (virtual_io_device_t*)(handler->entity);
  uint32_t offset = dst - handler->start_address;
  uint8_t *ptr = src;
  uint32_t value = 0;
  int i = 0;
  for (i = 0; i < size; i++)
  {
    value |= ptr[i] << (i * 8); 
  }
  if (offset >= VIRTIO_MMIO_CONFIG)
  {
    virtual_config_write(device, offset - VIRTIO_MMIO_CONFIG, value, size);
  }

  if (size == 4)
  {
    switch(offset)
    {
      case VIRTIO_MMIO_DEVICE_FEATURES_SEL:
        device->device_features_select = value;
        break;
      case VIRTIO_MMIO_QUEUE_SEL:
        if (value < MAX_QUEUE)
          device->queue_select = value;
        break;
      case VIRTIO_MMIO_QUEUE_NUM:
        if ((value & (value - 1)) == 0 && value > 0)
        {
          device->queue[device->queue_select].num = value;
        }
        break;
      case VIRTIO_MMIO_QUEUE_DESC_LOW:
        set_low32(&device->queue[device->queue_select].desc_addr, value);
        break;
      case VIRTIO_MMIO_QUEUE_AVAIL_LOW:
        set_low32(&device->queue[device->queue_select].avail_addr, value);
        break;
      case VIRTIO_MMIO_QUEUE_USED_LOW:
        set_low32(&device->queue[device->queue_select].used_addr, value);
        break;
      case VIRTIO_MMIO_QUEUE_DESC_HIGH:
        set_high32(&device->queue[device->queue_select].desc_addr, value);
        break;
      case VIRTIO_MMIO_QUEUE_AVAIL_HIGH:
        set_high32(&device->queue[device->queue_select].avail_addr, value);
        break;
      case VIRTIO_MMIO_QUEUE_USED_HIGH:
        set_high32(&device->queue[device->queue_select].used_addr, value);
        break;
      case VIRTIO_MMIO_STATUS:
        device->status = value;
        if (value == 0)
        {
          set_irq(device->irq, 0);
          virtual_io_reset(device);
        }
        break;
      case VIRTIO_MMIO_QUEUE_READY:
        device->queue[device->queue_select].ready = value & 1;
        break;
      case VIRTIO_MMIO_QUEUE_NOTIFY:
        if (value < MAX_QUEUE)
          queue_notify(device, value);
        break;
      case VIRTIO_MMIO_INTERRUPT_ACK:
        device->int_status &= ~value;
        if (device->int_status == 0)
        {
          set_irq(device->irq, 0);
        }
        break;
    }
  }
}

static void virtual_consume_desc(virtual_io_device_t *device, int queue_idx, int desc_idx, int desc_len)
{
  queue_state_t *qs = &device->queue[queue_idx];
  uint64_t addr;
  uint32_t index = 0;

  addr = qs->used_addr + 2;
  iomap_manager.read_vaddr(device->cpu_state, addr, 2, (uint8_t*)&index);
  index += 1;
  iomap_manager.write_vaddr(device->cpu_state, addr, 2, (uint8_t*)&index);
  index -= 1;

  addr = qs->used_addr + 4 + (index & (qs->num - 1)) * 8;
  iomap_manager.write_vaddr(device->cpu_state, addr, 4, (uint8_t*)&desc_idx);
  iomap_manager.write_vaddr(device->cpu_state, addr + 4, 4, (uint8_t*)&desc_len);

  device->int_status |= 1;
  set_irq(device->irq, 1);

  return;
}

static void virtual_block_req_end(virtual_io_device_t *device, int ret)
{
  virtual_io_block_device_t *vbd = (virtual_io_block_device_t*)device;
  int write_size;
  int queue_idx = vbd->req.queue_idx;
  int desc_idx = vbd->req.desc_idx;
  uint8_t *buf, buf1[1];

  switch(vbd->req.type)
  {
    case VIRTIO_BLK_T_IN:
      write_size = vbd->req.write_size;
      buf = vbd->req.buf;
      if (ret < 0)
      {
        buf[write_size - 1] = VIRTIO_BLK_S_IOERR;
      }
      else
      {
        buf[write_size - 1] = VIRTIO_BLK_S_OK;
      }
      virtual_memcpy_to_queue(device, queue_idx, desc_idx, 0, buf, write_size);
      free(buf);
      virtual_consume_desc(device, queue_idx, desc_idx, write_size);
      break;
    case VIRTIO_BLK_T_OUT:
      if (ret < 0)
        buf1[0] = VIRTIO_BLK_S_IOERR;
      else
        buf1[0] = VIRTIO_BLK_S_OK;
      virtual_memcpy_to_queue(device, queue_idx, desc_idx, 0, buf1, sizeof(buf1));
      virtual_consume_desc(device, queue_idx, desc_idx, 1);
      break;
    default:
      abort();
  }
}
static void virtual_io_block_req_cb(void *opaque, int ret)
{
  virtual_io_device_t *device = (virtual_io_device_t*)opaque;
  virtual_io_block_device_t *vbd = (virtual_io_block_device_t*)device;

  virtual_block_req_end(device, ret);

  vbd->req_in_progress = false;

  queue_notify(device, vbd->req.queue_idx);
}

int virtual_block_recv_request(virtual_io_device_t *device, int queue_idx,
    int desc_idx, int read_size, int write_size)
{
  virtual_io_block_device_t *vbd = (virtual_io_block_device_t*)device;
  block_device_t *bs = vbd->bs;
  block_request_header_t header;
  uint8_t *buf = NULL;
  int len, ret;

  if (vbd->req_in_progress)
    return -1;
  if (virtual_memcpy_from_queue(device, &header, queue_idx, desc_idx, 0, sizeof(header)) < 0)
    return 0;
  vbd->req.type = header.type;
  vbd->req.queue_idx = queue_idx;
  vbd->req.desc_idx = desc_idx;
  switch(header.type)
  {
    case VIRTIO_BLK_T_IN:
      vbd->req.buf = malloc(write_size);
      vbd->req.write_size = write_size;
      ret = bs->read_async(bs, header.sector_num, vbd->req.buf, (write_size - 1) / SECTOR_SIZE, virtual_io_block_req_cb, device);
      if (ret > 0)
      {
        vbd->req_in_progress = true;
      }
      else
      {
        virtual_block_req_end(device, ret);
      }
      break;
    case VIRTIO_BLK_T_OUT:
      assert(write_size >= 1);
      len = read_size - sizeof(header);
      buf = malloc(len);
      assert(buf != NULL);
      virtual_memcpy_from_queue(device, buf, queue_idx, desc_idx, sizeof(header), len);
      ret = bs->write_async(bs, header.sector_num, buf, len / SECTOR_SIZE, virtual_io_block_req_cb, device);
      free(buf);
      if (ret > 0)
      {
        vbd->req_in_progress = true;
      }
      else
      {
        virtual_block_req_end(device, ret);
      }
      break;
    default:
      break;
  }

  return 0;
}

void virtio_init(cpu_state_t *state, address_item_t *handler,
    virtual_io_device_t *device,
    virtual_io_bus_t *bus, uint32_t device_id, 
    int config_space_size,
    virtual_io_device_recieve_func *device_recv)
{
  memset(device, 0, sizeof(*device));
  device->irq = bus->irq;
  device->device_id = device_id;
  device->vendor_id = 0xffff;
  device->config_space_size = config_space_size;
  device->cpu_state = state;
  device->device_recv = device_recv;
  handler->entity = (void*)device;
  handler->start_address = bus->addr;
  handler->size = VIRTIO_PAGE_SIZE;
  handler->read_bytes = virtual_mmio_read;
  handler->write_bytes = virtual_mmio_write;
  iomap_manager.register_address(state, handler);
  virtual_io_reset(device);
}


