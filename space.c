#include "regs.h"
#include <string.h>
#include "memory.h"
#include "clint.h"
#include "htif.h"
#include "instructions.h"
#include "iomap.h"
#include "plic.h"
#include "fdt.h"
#include "virtio_interface.h"
#include "virtio_block_device.h"
#include <stdlib.h>
#include <stdio.h>

const char *bios_path = "./images/bbl64.bin";
const char *kernel_path = "./images/kernel-riscv64.bin";
const char *device = "./images/root-riscv64.bin";
const char *cmdline = "root=/dev/vda rw";

#define BIOS_INDEX 0
#define KERNEL_INDEX 1

typedef struct
{
  char *name;
  uint64_t size;
  uint8_t *buf;
} preload_file_t;

preload_file_t pfs[2] = {{"bios", 0, NULL}, {"kernel", 0, NULL}};

static void load_file(const char *path, preload_file_t *pre)
{
  FILE *f = fopen(path, "rb");
  uint64_t size = 0;
  uint8_t *buf = NULL;

  fseek(f, 0, SEEK_END);
  size = ftell(f);
  buf = malloc(size);
  if (!buf)
  {
    fclose(f);
    printf("load file failed, path: %s\n", path);
    exit(1);
  }
  fseek(f, 0, SEEK_SET);
  fread(buf, 1, size, f);
  fclose(f);

  pre->size = size;
  pre->buf = buf;
  return; 
}

void cpu_state_reset()
{
  memset(&cpu_state, 0, sizeof(cpu_state));
  cpu_state.pc = 0x1000;
  cpu_state.xlen = XLEN;
  cpu_state.mxl = get_base_from_xlen(XLEN);
  cpu_state.priv = PRIV_M;
  cpu_state.mstatus = ((uint64_t)get_base_from_xlen(XLEN) << MSTATUS_UXL_SHIFT) |
    ((uint64_t)get_base_from_xlen(XLEN) << MSTATUS_SXL_SHIFT);
  cpu_state.misa |= MCPUID_SUPER | MCPUID_USER | MCPUID_I | MCPUID_M |MCPUID_A;
}

static void copy_bios(cpu_state_t *state, const uint8_t *buf, int buf_len,
                 const uint8_t *kernel_buf, int kernel_buf_size, const char *cmd_line)
{
  uint32_t fdt_addr, kernel_align, kernel_base;
  uint32_t *q;

  if (buf_len > MEMORY_SIZE)
  {
    printf("bios is too big\n");
    exit(1);
  }

  iomap_manager.write(RAM_BASE_ADDR, buf_len, (uint8_t*)buf); 
  if (kernel_buf_size > 0)
  {
    if (XLEN == 32)
    {
      kernel_align = 4 << 20;
    }
    else
    {
      kernel_align = 2 << 20;
    }
    kernel_base = (buf_len + kernel_align - 1) & ~(kernel_align - 1);
    iomap_manager.write(RAM_BASE_ADDR + kernel_base, kernel_buf_size, (uint8_t*)kernel_buf);
  }
  else
  {
    kernel_base = 0;
  }

  fdt_addr = 0x1000 + 8 * 8;
  
  address_item_t *item = iomap_manager.get_address_item(state, fdt_addr);
  uint8_t *dst = NULL;
  if (item && item->entity)
  {
    dst = item->entity;
  }
  else
  {
    printf("build fdt failed, no memory selected\n");
    exit(1);
  }
  build_fdt(state, dst + fdt_addr, RAM_BASE_ADDR + kernel_base, kernel_buf_size, cmd_line);

  q = (uint32_t*)(dst + 0x1000);
  q[0] = 0x297 + 0x80000000 - 0x1000; /* auipc t0, jump_addr */
  q[1] = 0x597; /* auipc a1, dtb */
  q[2] = 0x58593 + ((fdt_addr - 4) << 20); /* addi a1, a1, dtb */
  q[3] = 0xf1402573; /* csrr a0, mhartid */
  q[4] = 0x00028067; /* jalr zero, t0, jump_addr */
}

int main()
{
  cpu_state_reset();  
  load_file(bios_path, &pfs[BIOS_INDEX]);
  load_file(kernel_path, &pfs[KERNEL_INDEX]);
  memory_module_init(&cpu_state);
  clint_module_init(&cpu_state);
  htif_module_init(&cpu_state);
  plic_module_init(&cpu_state);
  
  int i;
  for(i = 1; i < 32; i++)
  {
    irq_init(&(cpu_state.plic_irq[i]), plic_set_irq, &cpu_state, i);
  }
  virtual_io_bus_t *bus = NULL;
  uint32_t irq_num = VIRTIO_IRQ;
  bus = malloc(sizeof(*bus));
  if (!bus)
    return -1;
  memset(bus, 0, sizeof(*bus));
  bus->addr = VIRTIO_BASE_ADDR;
  bus->irq = &cpu_state.plic_irq[irq_num++];
  virtual_block_device_init(&cpu_state, device, BF_MODE_SNAPSHOT, bus);
  /* change addr and irq for next devie */
  bus->addr += VIRTIO_SIZE;
  bus->irq = &cpu_state.plic_irq[irq_num++];

  copy_bios(&cpu_state, pfs[BIOS_INDEX].buf, pfs[BIOS_INDEX].size, pfs[KERNEL_INDEX].buf, pfs[KERNEL_INDEX].size, cmdline);

  while(1)
  {
    machine_loop();
  }
  
  free(bus);
  return 0;
}
