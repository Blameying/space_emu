#include "regs.h"
#include "riscv_definations.h"
#include "memory.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <stdarg.h>
#include <byteswap.h>
#include <assert.h>
#include "fdt.h"
#include "cutils.h"


#define FDT_MAGIC 0xd00dfeed
#define FDT_VERSION 17

struct fdt_header
{
  uint32_t magic;
  uint32_t totalsize;
  uint32_t off_dt_struct;
  uint32_t off_dt_strings;
  uint32_t off_mem_rsvmap;
  uint32_t version;
  uint32_t last_comp_version;
  uint32_t boot_cpuid_phys;
  uint32_t size_dt_strings;
  uint32_t size_dt_struct;
};

struct fdt_reserve_entry
{
  uint64_t address;
  uint64_t size;
};

#define FDT_BEGIN_NODE 1
#define FDT_END_NODE 2
#define FDT_PROP 3
#define FDT_NOP 4
#define FDT_END 9

typedef struct
{
  uint32_t *tab;
  int tab_len;
  int tab_size;
  int open_node_count;

  char *string_table;
  int string_table_len;
  int string_table_size;
}fdt_state_t;

static fdt_state_t *fdt_init(void)
{
  fdt_state_t *fdt_s;
  fdt_s = malloc(sizeof(*fdt_s));
  if (!fdt_s)
  {
    printf("fdt state malloc error\n");
    abort();
  }
  memset(fdt_s, 0, sizeof(*fdt_s));
  return fdt_s;
}

static void fdt_alloc_len(fdt_state_t *fdt_s, int len)
{
  int new_size;
  if (len > fdt_s->tab_size)
  {
    new_size = max_int(len, fdt_s->tab_size * 3 / 2);
    fdt_s->tab = realloc(fdt_s->tab, new_size * sizeof(uint32_t));
    fdt_s->tab_size = new_size;
  }
}

/* small endian */
static void fdt_put32(fdt_state_t *fdt_s, uint32_t value)
{
  fdt_alloc_len(fdt_s, fdt_s->tab_len + 1);
  fdt_s->tab[fdt_s->tab_len++] = cpu_to_be32(value);
}

static void fdt_put_data(fdt_state_t *fdt_s, const uint8_t *data, int len)
{
  int len1;

  len1 = (len + 3) / 4;
  fdt_alloc_len(fdt_s, fdt_s->tab_len + len1);
  memcpy(fdt_s->tab + fdt_s->tab_len, data, len);
  memset((uint8_t*)(fdt_s->tab + fdt_s->tab_len) + len, 0, -len & 3);
  fdt_s->tab_len += len1;
}

static void fdt_begin_node(fdt_state_t *fdt_s, const char *name)
{
  fdt_put32(fdt_s, FDT_BEGIN_NODE);
  fdt_put_data(fdt_s, (uint8_t*)name, strlen(name) + 1);
  fdt_s->open_node_count++;
}

static void fdt_begin_node_num(fdt_state_t *fdt_s, const char *name, uint64_t n)
{
  char buf[256];
  snprintf(buf, sizeof(buf), "%s@%" PRIx64, name, n);
  fdt_begin_node(fdt_s, buf);
}

static void fdt_end_node(fdt_state_t *fdt_s)
{
  fdt_put32(fdt_s, FDT_END_NODE);
  fdt_s->open_node_count--;
}

static int fdt_get_string_offset(fdt_state_t *fdt_s, const char *name)
{
  int pos, new_size, name_size, new_len;

  pos = 0;
  while(pos < fdt_s->string_table_len)
  {
    if (!strcmp(fdt_s->string_table + pos, name))
      return pos;
    pos += strlen(fdt_s->string_table + pos) + 1;
  }

  name_size = strlen(name) + 1;
  new_len = fdt_s->string_table_len + name_size;
  if (new_len > fdt_s->string_table_size)
  {
    new_size = max_int(new_len, fdt_s->string_table_size * 3 / 2);
    fdt_s->string_table = realloc(fdt_s->string_table, new_size);
    fdt_s->string_table_size = new_size;
  }
  pos = fdt_s->string_table_len;
  memcpy(fdt_s->string_table + pos, name, name_size);
  fdt_s->string_table_len = new_len;

  return pos;
}

static void fdt_prop(fdt_state_t *fdt_s, const char *prop_name, const void *data, int data_len)
{
  fdt_put32(fdt_s, FDT_PROP);
  fdt_put32(fdt_s, data_len);
  fdt_put32(fdt_s, fdt_get_string_offset(fdt_s, prop_name));
  fdt_put_data(fdt_s, data, data_len);
}

static void fdt_prop_tab_u32(fdt_state_t *fdt_s, const char *prop_name, uint32_t *tab, int tab_len)
{
  int i;
  fdt_put32(fdt_s, FDT_PROP);
  fdt_put32(fdt_s, tab_len * sizeof(uint32_t));
  fdt_put32(fdt_s, fdt_get_string_offset(fdt_s, prop_name));
  for(i = 0; i < tab_len; i++)
    fdt_put32(fdt_s, tab[i]);
}

static void fdt_prop_u32(fdt_state_t *fdt_s, const char *prop_name, uint32_t value)
{
  fdt_prop_tab_u32(fdt_s, prop_name, &value, 1);
}

static void fdt_prop_tab_u64(fdt_state_t *fdt_s, const char *prop_name, uint64_t value)
{
  uint32_t tab[2];
  tab[0] = value >> 32;
  tab[1] = value;
  fdt_prop_tab_u32(fdt_s, prop_name, tab, 2);
}

static void fdt_prop_tab_u64_2(fdt_state_t *fdt_s, const char *prop_name, uint64_t v0, uint64_t v1)
{
  uint32_t tab[4];
  tab[0] = v0 >> 32;
  tab[1] = v0;
  tab[2] = v1 >> 32;
  tab[3] = v1;
  fdt_prop_tab_u32(fdt_s, prop_name, tab, 4);
}

static void fdt_prop_str(fdt_state_t *fdt_s, const char *prop_name, const char *str)
{
  fdt_prop(fdt_s, prop_name, str, strlen(str) + 1);
}

static void fdt_prop_tab_str(fdt_state_t *fdt_s, const char *prop_name, ...)
{
  va_list ap;
  int size, str_size;
  char *ptr, *tab;

  va_start(ap, prop_name);
  size = 0;
  for(;;)
  {
    ptr = va_arg(ap, char*);
    if (!ptr)
      break;
    str_size = strlen(ptr) + 1;
    size += str_size;
  }
  va_end(ap);

  tab = malloc(size);
  memset(tab, 0, size);
  va_start(ap, prop_name);
  size = 0;
  for(;;)
  {
    ptr = va_arg(ap, char*);
    if (!ptr)
      break;
    str_size = strlen(ptr) + 1;
    memcpy(tab + size, ptr, str_size);
    size += str_size;
  }
  va_end(ap);

  fdt_prop(fdt_s, prop_name, tab, size);
  free(tab);
}

int fdt_output(fdt_state_t *fdt_s, uint8_t *dst)
{
  struct fdt_header *h;
  struct fdt_reserve_entry *re;
  int dt_struct_size;
  int dt_strings_size;
  int pos;

  assert(fdt_s->open_node_count == 0);
  fdt_put32(fdt_s, FDT_END);

  dt_struct_size = fdt_s->tab_len * sizeof(uint32_t);
  dt_strings_size = fdt_s->string_table_len;

  h = (struct fdt_header *)dst;
  h->magic = cpu_to_be32(FDT_MAGIC);
  h->version = cpu_to_be32(FDT_VERSION);
  h->last_comp_version = cpu_to_be32(16);
  h->boot_cpuid_phys = cpu_to_be32(0);
  h->size_dt_strings = cpu_to_be32(dt_strings_size);
  h->size_dt_struct = cpu_to_be32(dt_struct_size);

  pos = sizeof(struct fdt_header);

  h->off_dt_struct = cpu_to_be32(pos);
  memcpy(dst + pos, fdt_s->tab, dt_struct_size);
  pos += dt_struct_size;

  /* align to 8 */
  while((pos & 7) != 0)
  {
    dst[pos++] = 0;
  } 

  h->off_mem_rsvmap = cpu_to_be32(pos);
  re = (struct fdt_reserve_entry *)(dst + pos);
  re->address = 0;
  re->size = 0;
  pos += sizeof(struct fdt_reserve_entry);

  h->off_dt_strings = cpu_to_be32(pos);
  memcpy(dst + pos, fdt_s->string_table, dt_strings_size);
  pos += dt_strings_size;

  /* align to 8 */
  while((pos & 7) != 0)
  {
    dst[pos++] = 0;
  } 

  h->totalsize = cpu_to_be32(pos);

  return pos;
}

void fdt_free(fdt_state_t *fdt_s)
{
  free(fdt_s->tab);
  free(fdt_s->string_table);
  free(fdt_s);
}

int build_fdt(cpu_state_t *state, uint8_t *dst, uint64_t kernel_start, uint64_t kernel_size, const char *cmd_line)
{
  fdt_state_t *fdt_s;
  int size, max_xlen, i, cur_phandler, intc_phandler, plic_handler;
  char isa_string[128], *q;
  uint32_t misa;
  uint32_t tab[4];

  fdt_s = fdt_init();
  cur_phandler = 1;

  fdt_begin_node(fdt_s, "");
  fdt_prop_u32(fdt_s, "#address-cells", 2);
  fdt_prop_u32(fdt_s, "#size-cells", 2);
  fdt_prop_str(fdt_s, "compatible", "ucbbar,riscvemu-bar_dev");
  fdt_prop_str(fdt_s, "model", "ucbbar,riscvemu-bare");

  /* cpu list */
  fdt_begin_node(fdt_s, "cpus");
  fdt_prop_u32(fdt_s, "#address-cells", 1);
  fdt_prop_u32(fdt_s, "#size-cells", 0);
  fdt_prop_u32(fdt_s, "timebase-frequency", RTC_FREQ);

  /* cpu */
  fdt_begin_node_num(fdt_s, "cpu", 0);
  fdt_prop_str(fdt_s, "device_type", "cpu");
  fdt_prop_u32(fdt_s, "reg", 0);
  fdt_prop_str(fdt_s, "status", "okay");
  fdt_prop_str(fdt_s, "compatible", "riscv");

  max_xlen = XLEN;
  misa = state->misa;
  q = isa_string;
  q += snprintf(isa_string, sizeof(isa_string), "rv%d", max_xlen);
  for (i = 0; i < 26; i++)
  {
    if (misa & (1 << i))
      *q++ = 'a' + i;
  }

  *q = '\0';
  fdt_prop_str(fdt_s, "riscv,isa", isa_string);

  fdt_prop_str(fdt_s, "mmu-type", max_xlen <= 32 ? "riscv,sv32":"riscv,sv48");
  fdt_prop_u32(fdt_s, "clock-frequency", 2000000000);

  fdt_begin_node(fdt_s, "interrupt-controller");
  fdt_prop_u32(fdt_s, "#interrput-cells", 1);
  fdt_prop(fdt_s, "interrput-controller", NULL, 0);
  fdt_prop_str(fdt_s, "compatible", "riscv,cpu-intc");
  intc_phandler = cur_phandler++;
  fdt_prop_u32(fdt_s, "phandle", intc_phandler);
  fdt_end_node(fdt_s); /* interrupt controller */

  fdt_end_node(fdt_s); /* cpu */
  fdt_end_node(fdt_s); /* cpus */

  fdt_begin_node_num(fdt_s, "memory", RAM_BASE_ADDR);
  fdt_prop_str(fdt_s, "device_type", "memory");
  tab[0] = (uint64_t)RAM_BASE_ADDR >> 32;
  tab[1] = RAM_BASE_ADDR;
  tab[2] = (uint64_t)MEMORY_SIZE >> 32;
  tab[3] = (uint64_t)MEMORY_SIZE;
  fdt_prop_tab_u32(fdt_s, "reg", tab, 4);
  fdt_end_node(fdt_s); /* memory */

  fdt_begin_node(fdt_s, "htif");
  fdt_prop_str(fdt_s, "compatible", "ucb,htif0");
  fdt_end_node(fdt_s);

  fdt_begin_node(fdt_s, "soc");
  fdt_prop_u32(fdt_s, "#address-cells", 2);
  fdt_prop_u32(fdt_s, "size-cells", 2);
  fdt_prop_tab_str(fdt_s, "compatible", "ucbbar,riscvemu-bar-soc", "simple-bus", NULL);
  fdt_prop(fdt_s, "ranges", NULL, 0);

  fdt_begin_node_num(fdt_s, "clint", CLINT_BASE_ADDR);
  fdt_prop_str(fdt_s, "compatible", "riscv,clint0");
  tab[0] = intc_phandler;
  tab[1] = 3;
  tab[2] = intc_phandler;
  tab[3] = 7;
  fdt_prop_tab_u32(fdt_s, "interrupts-extended", tab, 4);
  fdt_prop_tab_u64_2(fdt_s, "reg", CLINT_BASE_ADDR, CLINT_SIZE);
  fdt_end_node(fdt_s); /* clint */

  fdt_begin_node_num(fdt_s, "plic", PLIC_BASE_ADDR);
  fdt_prop_u32(fdt_s, "#interrupt-cells", 1);
  fdt_prop(fdt_s, "interrupt-controller", NULL, 0);
  fdt_prop_str(fdt_s, "compatible", "riscv,plic0");
  fdt_prop_u32(fdt_s, "riscv,ndev", 31);
  fdt_prop_tab_u64_2(fdt_s, "reg", PLIC_BASE_ADDR, PLIC_SIZE);
  tab[0] = intc_phandler;
  tab[1] = 9;
  tab[2] = intc_phandler;
  tab[3] = 11;
  fdt_prop_tab_u32(fdt_s, "interrputs-extended", tab, 4);
  plic_handler = cur_phandler++;
  fdt_prop_u32(fdt_s, "phandle", plic_handler);
  fdt_end_node(fdt_s); /* plic */

  for(i = 0; i < 2; i++)
  {
    fdt_begin_node_num(fdt_s, "virtio", VIRTIO_BASE_ADDR + i * VIRTIO_SIZE);
    fdt_prop_str(fdt_s, "compatible", "virtio,mmio");
    fdt_prop_tab_u64_2(fdt_s, "reg", VIRTIO_BASE_ADDR + i * VIRTIO_SIZE,
        VIRTIO_SIZE);
    tab[0] = plic_handler;
    tab[1] = VIRTIO_IRQ + i;
    fdt_prop_tab_u32(fdt_s, "interrupts-extended", tab, 2);
    fdt_end_node(fdt_s); /* virtio */
  }

  fdt_end_node(fdt_s); /* soc */

  fdt_begin_node(fdt_s, "chosen");
  fdt_prop_str(fdt_s, "bootargs", cmd_line ? cmd_line : "");
  if (kernel_size > 0)
  {
    fdt_prop_tab_u64(fdt_s, "riscv,kernel-start", kernel_start);
    fdt_prop_tab_u64(fdt_s, "riscv,kernel-end", kernel_start + kernel_size);
  }

  fdt_end_node(fdt_s); /* chosen */
  fdt_end_node(fdt_s); /* / */

#if 1
  {
    FILE *f;
    f = fopen("../tinyemu/riscvemu.dtb", "rb");
    fseek(f, 0, SEEK_END);
    size = ftell(f);
    fseek(f, 0, SEEK_SET);
    fread(dst, 1, size, f);
    fclose(f);
  }
#endif

  //size = fdt_output(fdt_s, dst);
#if 1
  {
    FILE *f;
    f = fopen("./riscvemu.dtb", "wb");
    fwrite(dst, 1, size, f);
    fclose(f);
  }
#endif
  fdt_free(fdt_s);

  return size;
}
