#include "iomap.h"
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "riscv_definations.h"

#define ADDRESS_ITEM_COUNT 1024

static address_item_t *address_items[ADDRESS_ITEM_COUNT];

static int check_overlap(address_item_t *item0, address_item_t *item1)
{
  uint_t address_1[2] = {item0->start_address, item0->start_address + item0->size - 1};
  uint_t address_2[2] = {item1->start_address, item1->start_address + item1->size - 1};
  if ((address_2[0] > address_1[1]) || (address_1[0] > address_2[1]))
  {
    return 0;
  }

  return 1;
}

static int check_in(address_item_t *item0, uint_t start, uint_t size)
{
  uint_t address_1[2] = {item0->start_address, item0->start_address + item0->size - 1};
  if (start >= address_1[0] && (start + size) <= address_1[1])
  {
    return 1;
  }
  return 0;
}

static void register_address_manager(cpu_state_t *state, address_item_t *item)
{
  int i = 0;
  for(; i < ADDRESS_ITEM_COUNT; i++)
  {
    if (address_items[i] != NULL) 
    {
      if (check_overlap(address_items[i], item))
      {
        printf("register address handler: %s failed, address overlap\n", item->name);
        return;
      }
    }
    else
    {
      item->cpu_state = state;
      /* init when register */
      if (item->init && item->init(item) == false)
      {
        printf("register address handler: %s failed, init failed\n", item->name);
        return;
      }

      address_items[i] = item;
      return;
    }
  }
}

static void release_address_manager()
{
  int i = 0;
  for(; i < ADDRESS_ITEM_COUNT; i++)
  {
    if (address_items[i] != NULL)
    {
      if (address_items[i]->release != NULL)
        address_items[i]->release(address_items[i]);
    }
    else
    {
      break;
    }
  }
}

static int_t write_bytes(uint_t address, uint_t size, uint8_t *src)
{
  int i = 0;
  for (; i < ADDRESS_ITEM_COUNT; i++)
  {
    if (address_items[i] == NULL)
      break;

    if (check_in(address_items[i], address, size))
    {
      return address_items[i]->write_bytes(address_items[i], src, size, address);
    }
  }

  return 0;
}

static int_t read_bytes(uint_t address, uint_t size, uint8_t *dst)
{
  int i = 0;
  for (; i < ADDRESS_ITEM_COUNT; i++)
  {
    if (address_items[i] == NULL)
    {
      break;
    }
    if (check_in(address_items[i], address, size))
    {
      return address_items[i]->read_bytes(address_items[i], address, size, dst);
    }
  }

  return 0;
}

typedef struct pte_format
{
  int mode;
  uint_t ppn;
  uint32_t pagesize;
  uint32_t ptesize;
  uint32_t ppn_width;
  uint32_t levels;
  uint_t vpn[4];
  uint_t va_offset;
  uint32_t access;
  uint_t pte_ppn_mask;
} pte_format_t;

static int translate_action(cpu_state_t *state, pte_format_t *ft, uint_t *result, int access)
{
  int i = 0;
  uint_t base = ft->ppn;
  uint_t pte = 0;
  uint_t xwr = 0;
  
  for (i = ft->levels - 1; i >= 0; i--)
  {
    uint_t pte_addr = base * ft->pagesize + ft->vpn[i] * ft->ptesize;
    iomap_manager.read(pte_addr, ft->ptesize, (uint8_t*)&pte);
    
    if ((!(pte & PTE_V_MASK)) || (!(pte & PTE_R_MASK) && (pte & PTE_W_MASK)))
      return -1;

    if (!((pte & PTE_R_MASK) || (pte &  PTE_X_MASK)))
    {
      /* not leaf node */
      base = (pte & ft->pte_ppn_mask) >> 10;
      continue;
    }
 
    if (state->priv == PRIV_S)
    {
      if ((pte & PTE_U_MASK) && !(state->mstatus & MSTATUS_SUM))
        return -1;
    }
    else
    {
      if (!(pte & PTE_U_MASK))
        return -1;
    }
    xwr = pte & (PTE_R_MASK | PTE_W_MASK | PTE_X_MASK);
    if ((state->mstatus & MSTATUS_MXR))
      xwr |= (xwr >> 2);

    if ((xwr & ft->access) == 0)
    {
      return -1;
    }

    int need_write = !(pte & PTE_A_MASK) || ((!(pte & PTE_D_MASK)) && (access == PTE_W_MASK));
    pte |= PTE_A_MASK;
    if (ft->access == PTE_W_MASK)
      pte |= PTE_D_MASK;
    if (need_write)
      iomap_manager.write(pte_addr, ft->ptesize, (uint8_t*)&pte);
    
    /* superpage */
    if (i > 0)
    {
      *result = 0;
      int j = 0;
      for (j = 0; j < i; j++)
      {
        *result |= (ft->vpn[j] << (ft->ppn_width * j));
      }
      *result <<= 10;
      ft->pte_ppn_mask = (ft->pte_ppn_mask >> (10 + ft->ppn_width * i) << (10 + ft->pte_ppn_mask * i));
    }

    *result |= (((pte & ft->pte_ppn_mask) >> 10 << 12) | ft->va_offset);
    return 0;
  }
  return -1;
}


static int address_translate(cpu_state_t *state, uint_t inst, uint32_t access, uint_t *result)
{
#if XLEN == 32
   uint32_t mode = (state->satp >> 31) & 0x1;
   //uint32_t asid = (state->satp >> 22) & 0x1FF;
   uint_t ppn = state->satp & ((1 << 22) - 1);
#elif XLEN >= 64
   uint32_t mode = (state->satp >> 60) & 0xF;
   //uint32_t asid = (state->satp >> 44) & 0xFFFF;
   uint_t ppn = state->satp & (((uint64_t)1 << 44) - 1);
#endif
   switch (mode)
   {
     case 0: /* Bare */
       *result = inst;
       return 0;
     case 1: /* Sv32 */
       {
         pte_format_t ft;
         memset(&ft, 0, sizeof(pte_format_t));
         ft.mode = mode;
         ft.ppn = ppn;
         ft.pagesize = 4096;
         ft.ppn_width = 10;
         ft.ptesize = 4;
         ft.levels = 2;
         ft.vpn[0] = (inst >> 12) & 0x3FF;
         ft.vpn[1] = (inst >> 22) & 0x3FF;
         ft.va_offset = inst & 0xFFF;
         ft.access = access;
         ft.pte_ppn_mask = (((uint32_t)0 - 1) >> 10 << 10);
         return translate_action(state, &ft, result, access);
       }
       break;
     case 8: /* Sv39 */
       {
         pte_format_t ft;
         memset(&ft, 0, sizeof(pte_format_t));
         ft.mode = mode;
         ft.ppn = ppn;
         ft.pagesize = 4096;
         ft.ptesize = 8;
         ft.ppn_width = 9;
         ft.levels = 3;
         ft.vpn[0] = (inst >> 12) & 0x1FF;
         ft.vpn[1] = (inst >> 21) & 0x1FF;
         ft.vpn[2] = (inst >> 30) & 0x1FF;
         ft.va_offset = inst & 0xFFF;
         ft.access = access;
         ft.pte_ppn_mask = (((uint64_t)0 - 1) >> 10 << 20 >> 10);
         return translate_action(state, &ft, result, access);
       }
       break;
     case 9: /* Sv48 */
       {
         pte_format_t ft;
         memset(&ft, 0, sizeof(pte_format_t));
         ft.mode = mode;
         ft.ppn = ppn;
         ft.pagesize = 4096;
         ft.ptesize = 8;
         ft.ppn_width = 9;
         ft.levels = 4;
         ft.vpn[0] = (inst >> 12) & 0x1FF;
         ft.vpn[1] = (inst >> 21) & 0x1FF;
         ft.vpn[2] = (inst >> 30) & 0x1FF;
         ft.vpn[3] = (inst >> 39) & 0x1FF;
         ft.va_offset = inst & 0xFFF;
         ft.access = access;
         ft.pte_ppn_mask = (((uint64_t)0 - 1) >> 10 << 20 >> 10);
         return translate_action(state, &ft, result, access);
       }
       break;
     default:
       return -1;
   }
}

static int_t read_vaddr(cpu_state_t *state, uint_t vaddress, uint_t size, uint8_t *dst)
{
  uint_t phy_address = 0;
  int flag = 0;
  uint32_t page_mask = vaddress & PG_MASK;

  flag = address_translate(state, vaddress, PTE_R_MASK, &phy_address);
  if (flag < 0)
    return flag;

  if (phy_address == vaddress)
  {
    return iomap_manager.read(phy_address, size, dst);
  }

  if ((PG_MASK - page_mask + 1) >= size)
  {
    return iomap_manager.read(phy_address, size, dst);
  }
  else
  {
    uint32_t new_size = PG_MASK + 1 - page_mask; 
    if (iomap_manager.read(phy_address, new_size, dst) < 0)
      return -1;
    vaddress += new_size;
    dst += new_size;
    return read_vaddr(state, vaddress, size - new_size, dst);
  }
}

static int_t write_vaddr(cpu_state_t *state, uint_t vaddress, uint_t size, uint8_t *src)
{
  uint_t phy_address = 0;
  int flag = 0;
  uint32_t page_mask = vaddress & PG_MASK;

  flag = address_translate(state, vaddress, PTE_W_MASK, &phy_address);
  if (flag < 0)
    return flag;

  if (phy_address == vaddress)
  {
    return iomap_manager.write(phy_address, size, src);
  }

  if ((PG_MASK - page_mask + 1) >= size)
  {
    return iomap_manager.write(phy_address, size, src);
  }
  else
  {
    uint32_t new_size = PG_MASK + 1 - page_mask; 
    if (iomap_manager.write(phy_address, new_size, src) < 0)
      return -1;
    vaddress += new_size;
    src += new_size;
    return write_vaddr(state, vaddress, size - new_size, src);
  }
}

static int_t code_vaddr(cpu_state_t *state, uint_t vaddress, uint_t size, uint8_t *dst)
{
  uint_t phy_address = 0;
  int flag = 0;
  uint32_t page_mask = vaddress & PG_MASK;

  flag = address_translate(state, vaddress, PTE_X_MASK, &phy_address);
  if (flag < 0)
    return flag;

  if (phy_address == vaddress)
  {
    return iomap_manager.read(phy_address, size, dst);
  }

  if ((PG_MASK - page_mask + 1) >= size)
  {
    return iomap_manager.read(phy_address, size, dst);
  }
  else
  {
    uint32_t new_size = PG_MASK + 1 - page_mask; 
    if (iomap_manager.read(phy_address, new_size, dst) < 0)
      return -1;
    vaddress += new_size;
    dst += new_size;
    return code_vaddr(state, vaddress, size - new_size, dst);
  }
}

static address_item_t *get_address_item(cpu_state_t *state, uint_t address)
{
  uint_t phy_address = 0;
  int flag = 0;
  flag = address_translate(state, address, PTE_R_MASK, &phy_address);
  if (flag == 0 && phy_address == address)
  {
    int i = 0;
    for (; i < ADDRESS_ITEM_COUNT; i++)
    {
      if (address_items[i] == NULL)
        break;

      if (check_in(address_items[i], address, 1))
      {
        return address_items[i];
      }
    }
  }

  return NULL;
}

iomap_t iomap_manager = {
  .register_address = register_address_manager,
  .release = release_address_manager,
  .write = write_bytes,
  .read = read_bytes,
  .write_vaddr = write_vaddr,
  .read_vaddr = read_vaddr,
  .code_vaddr = code_vaddr,
  .get_address_item = get_address_item
};
