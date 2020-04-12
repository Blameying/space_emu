#include "pci.h"
#include "iomap.h"
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#define LOG_E(fmt, ...) fprintf(stderr, fmt, __VA_ARGS__)

typedef struct
{
  uint32_t size;
  uint8_t type;
  bool enabled;
  void *opaque;
  pci_bar_set_function *bar_set;
} pci_io_region_t;

struct pci_device
{
  pci_bus_t *bus;
  uint8_t devfn;
  irq_signal_t irq[4];
  uint8_t config[256];
  uint8_t next_cap_offset;
  char *name;
  pci_io_region_t io_regions[PCI_NUM_REGIONS];
};

struct pci_bus
{
  int bus_num;
  pci_device_t *device[256];
  uint32_t irq_state[4][8];
  irq_signal_t irq[4];
};

static int bus_map_irq(pci_device_t *device, int irq_num)
{
  int slot_addend;
  slot_addend = (device->devfn >> 3) - 1;
  return (irq_num + slot_addend) & 3;
}

static void pci_device_set_irq(void *opaque, int irq_num, int level)
{
  pci_device_t *device = opaque;
  pci_bus_t *bus = device->bus;

  uint32_t mask = 0;
  int i = 0, irq_level = 0;

  irq_num = bus_map_irq(device, irq_num);
  mask = 1 << (device->devfn & 0x1f);
  if (level)
  {
    bus->irq_state[irq_num][device->devfn >> 5] |= mask;
  }
  else
  {
    bus->irq_state[irq_num][device->devfn >> 5] &= ~mask;
  }

  mask = 0;
  for (i = 0; i < 8; i++)
    mask |= bus->irq_state[irq_num][i];
  irq_level = (mask != 0);
  set_irq(&bus->irq[irq_num], irq_level);
}

static int devfn_alloc(pci_bus_t *bus)
{
  int devfn;
  for (devfn = 0; devfn < 256; devfn += 8)
  {
    if (!bus->device[devfn])
      return devfn;
  }
  return -1;
}

pci_device_t *pci_register_device(pci_bus_t *bus, const char *name, int devfn, uint16_t vendor_id, uint16_t device_id, uint8_t revision, uint16_t class_id)
{
  pci_device_t *device;
  int i;

  if (devfn < 0)
  {
    devfn = devfn_alloc(bus);
    if (devfn < 0)
      return NULL;
  }

  if (bus->device[devfn])
    return NULL;

  device = malloc(sizeof(pci_device_t));
  if (!device)
    return NULL;

  memset(device, 0, sizeof(pci_device_t));
  device->bus = bus;
  device->name = strdup(name);
  device->devfn = devfn;

  /* set vendor_id */
  uint8_t *ptr = device->config + 0x00;
  ptr[0] = vendor_id;
  ptr[1] = vendor_id >> 8;

  /* set device_id */
  ptr = device->config + 0x02;
  ptr[0] = device_id;
  ptr[1] = device_id >> 8;

  /* set revision */
  device->config[0x08] = revision;

  /* set class_id */
  ptr = device->config + 0x0a;
  ptr[0] = class_id;
  ptr[1] = class_id >> 8;

  /* header type */
  device->config[0x0e] = 0x00;
  device->next_cap_offset = 0x40;

  for (i = 0; i < 4; i++)
    irq_init(&device->irq[i], pci_device_set_irq, device, i);
  bus->device[devfn] = device;

  return device;
}

irq_signal_t *pci_device_get_irq(pci_device_t *device, uint32_t irq_num)
{
  if (irq_num >= 4)
  {
    LOG_E("pci device get irq failed, irq beyond valid range, irq: %u\n",irq_num);
    return NULL;
  }
  return &device->irq[irq_num];
}

static uint32_t pci_device_config_read(pci_device_t *device, uint32_t addr, int size)
{
  uint32_t value = 0;
  uint8_t *ptr = (uint8_t*)(device->config + addr);
  switch(size)
  {
    case 1:
      value = *ptr;
      break;
    case 2:
      if (addr <= 0xfe)
      {
        value = ptr[0] | (ptr[1] << 8);
      }
      else
        value = *ptr;
      break;
    case 4:
      value = ptr[0] | (ptr[1] << 8) | (ptr[2] << 16) | (ptr[3] << 24);
      break;
    default:
      abort();
  }
  return value;
}

void pci_register_bar(pci_device_t *device, uint32_t bar_num, uint32_t size, int type, void *opaque, pci_bar_set_function *bar_set)
{
  pci_io_region_t *region;
  uint32_t value = 0;
  uint32_t config_addr = 0;

  assert(bar_num < PCI_NUM_REGIONS);
  assert((size & (size - 1)) == 0);
  assert(size >= 4);
  region = &device->io_regions[bar_num];
  assert(region->size == 0);
  region->size = size;
  region->type = type;
  region->enabled = false;
  region->opaque = opaque;
  region->bar_set = bar_set;

  if (bar_num == PCI_ROM_SLOT)
  {
    config_addr = 0x30;
  }
  else
  {
    value |= region->type;
    config_addr = 0x10 + 4 * bar_num;
  }

  uint8_t *ptr = (uint8_t*)&device->config[config_addr];
  ptr[0] = value;
  ptr[1] = value >> 8;
  ptr[2] = value >> 16;
  ptr[3] = value >> 24;
}

static void pci_update_mappings(pci_device_t *device)
{
  int cmd = 0;
  int i = 0;
  int offset = 0;
  uint32_t new_addr = 0;
  bool new_enabled = false;
  pci_io_region_t *region = NULL;

  uint8_t *ptr = &device->config[PCI_COMMAND];
  cmd = ptr[0] | ptr[1] << 8;

  for (i = 0; i < PCI_NUM_REGIONS; i++)
  {
    region = &device->io_regions[i];
    if (i == PCI_ROM_SLOT)
    {
      offset = 0x30;
    }
    else
    {
      offset = 0x10 + i * 4;
    }
    ptr = &device->config[offset];
    new_addr = ptr[0] | ptr[1] << 8 | ptr[2] << 16 | ptr[3] << 24;
    new_enabled = false;
    if (region->size != 0)
    {
      if ((region->type & PCI_ADDRESS_SPACE_IO) && (cmd & PCI_COMMAND_IO))
      {
        new_enabled = true;
      }
      else
      {
        if (cmd & PCI_COMMAND_MEMORY)
        {
          if (i == PCI_ROM_SLOT)
          {
            new_enabled = (new_addr & 1);
          }
          else
          {
            new_enabled = true;
          }
        }
      }
    }
    if (new_enabled)
    {
      ptr = &device->config[offset];
      new_addr = ptr[0] | ptr[1] << 8 | ptr[2] << 16 | ptr[3] << 24;
      region->bar_set(region->opaque, i, new_addr, true);
      region->enabled = true;
    }
    else if (region->enabled)
    {
      region->bar_set(region->opaque, i, 0, false);
      region->enabled = false;
    }
  }
}

static int pci_write_bar(pci_device_t *device, uint32_t addr, uint32_t value)
{
  pci_io_region_t *region = NULL;
  int reg = 0;

  if (addr == 0x30)
  {
    reg = PCI_ROM_SLOT;
  }
  else
  {
    reg = (addr - 0x10) >> 2;
  }

  region = &device->io_regions[reg];
  if (region->size == 0)
    return -1;
  if (reg == PCI_ROM_SLOT)
    value = value & ((~(region->size - 1)) | 1);
  else
    value = (value & ~(region->size - 1)) | region->type;

  uint8_t *ptr = (uint8_t*)(device->config + addr);
  ptr[0] = value;
  ptr[1] = value >> 8;
  ptr[2] = value >> 16;
  ptr[3] = value >> 24;

  pci_update_mappings(device);
  return 0;
}

static void pci_device_config_write8(pci_device_t *device, uint32_t addr, uint32_t data)
{
  int can_write;
  if (addr == PCI_STATUS || addr == (PCI_STATUS + 1))
  {
    device->config[addr] &= ~data;
    return;
  }

  switch(device->config[0x0e])
  {
    case 0x00:
    case 0x80:
      switch(addr)
      {
        case 0x00:
        case 0x01:
        case 0x02:
        case 0x03:
        case 0x08:
        case 0x09:
        case 0x0a:
        case 0x0b:
        case 0x0e:
        case 0x10 ... 0x27: /* base */
        case 0x30 ... 0x33: /* rom */
        case 0x3d:
          can_write = 0;
          break;
        default:
          can_write = 1;
          break;
      }
      break;
    default:
    case 0x01:
      switch(addr)
      {
        case 0x00:
        case 0x01:
        case 0x02:
        case 0x03:
        case 0x08:
        case 0x09:
        case 0x0a:
        case 0x0b:
        case 0x0e:
        case 0x38 ... 0x3b: /* rom */
        case 0x3d:
            can_write = 0;
            break;
        default:
            can_write = 1;
            break;
      }
      break;
  }
  if (can_write)
    device->config[addr] = data;
}

static void pci_device_config_write(pci_device_t *device, uint32_t addr, uint32_t data, int size)
{
  int size = 0;
  int i = 0;
  uint32_t addr1 = 0;

  if (size == 4 && ((addr >= 0x10 && addr < 0x10 + 4 * 6) ||
      addr == 0x30))
  {
    if (pci_write_bar(device, addr, data) == 0)
      return;
  }

  if (size & (size - 1))
  {
    return;
  }

  for (i = 0; i < size; i++)
  {
    addr1 = addr + i;
    if (addr1 <= 0xff)
    {
      pci_device_config_write8(device, addr1, (data >> (i * 8)) & 0xff);
    }
  }

  if (PCI_COMMAND >= addr && PCI_COMMAND < (addr + size))
  {
    pci_update_mappings(device);
  }
}

static void pci_data_write(pci_bus_t *bus, uint32_t addr, uint32_t data, int size)
{
  pci_device_t *device;
  int bus_num = 0, devfn = 0, config_addr = 0;

  bus_num = (addr >> 16) & 0xff;
  if (bus_num != bus->bus_num)
    return;

  devfn = (addr >> 8) & 0xff;
  device->bus->device[devfn];
  if (!device)
    return;
  config_addr = addr & 0xff;
  pci_device_config_write(device, config_addr, data, size);
}

static uint32_t pci_data_read(pci_bus_t *bus, uint32_t addr, int size)
{
  pci_device_t *device;
  int bus_num = 0, devfn = 0, config_addr = 0;

  bus_num = (addr >> 16) & 0xff;
  if (bus_num != bus->bus_num)
  {
    switch(size)
    {
      case 0:
        return 0xff;
      case 2:
        return 0xffff;
      case 4:
        return 0xffffffff;
      default:
        LOG_E("pci data read error, size is not the power of 2, size: %d\n", size);
        exit(1);
    }
  }
  devfn = (addr >> 8) & 0xff;
  device = bus->device[devfn];
  if (!device)
  {
    switch(size)
    {
      case 0:
        return 0xff;
      case 2:
        return 0xffff;
      case 4:
        return 0xffffffff;
      default:
        LOG_E("pci data read error, size is not the power of 2, size: %d\n", size);
        exit(1);
    }
  }
  config_addr = addr & 0xff;
  return pci_device_config_read(device, config_addr, size);
}

void pci_device_set_config8(pci_device_t *device, uint8_t addr, uint8_t val)
{
  device->config[addr] = val;
}

void pci_device_set_config16(pci_device_t *device, uint8_t addr, uint16_t val)
{
  uint8_t *ptr = &device->config[addr];
  ptr[0] = val;
  ptr[1] = val >> 8;
}

int pci_device_get_devfn(pci_device_t *device)
{
  return device->devfn;
}

int pci_add_capability(pci_device_t *device, const uint8_t *buf, int size)
{
  int offset = device->next_cap_offset;
  if ((offset + size) > 256)
    return -1;
  device->next_cap_offset += size;
  device->config[PCI_STATUS] |= PCI_STATUS_CAP_LIST;
  memcpy(device->config + offset, buf, size);
  device->config[offset + 1] = device->config[PCI_STATUS_CAP_LIST];
  device->config[PCI_CAPABILITY_LIST] = offset;
  return offset;
}

