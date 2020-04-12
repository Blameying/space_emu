#ifndef __VIRTIO_INTERFACE_H__
#define __VIRTIO_INTERFACE_H__
#include "regs.h"
#include "iomap.h"

#define VIRTIO_PAGE_SIZE 4096
 /* MMIO addresses - from the Linux kernel */
#define VIRTIO_MMIO_MAGIC_VALUE		0x000
#define VIRTIO_MMIO_VERSION		0x004
#define VIRTIO_MMIO_DEVICE_ID		0x008
#define VIRTIO_MMIO_VENDOR_ID		0x00c
#define VIRTIO_MMIO_DEVICE_FEATURES	0x010
#define VIRTIO_MMIO_DEVICE_FEATURES_SEL	0x014
#define VIRTIO_MMIO_DRIVER_FEATURES	0x020
#define VIRTIO_MMIO_DRIVER_FEATURES_SEL	0x024
#define VIRTIO_MMIO_GUEST_PAGE_SIZE	0x028 /* version 1 only */
#define VIRTIO_MMIO_QUEUE_SEL		0x030
#define VIRTIO_MMIO_QUEUE_NUM_MAX	0x034
#define VIRTIO_MMIO_QUEUE_NUM		0x038
#define VIRTIO_MMIO_QUEUE_ALIGN		0x03c /* version 1 only */
#define VIRTIO_MMIO_QUEUE_PFN		0x040 /* version 1 only */
#define VIRTIO_MMIO_QUEUE_READY		0x044
#define VIRTIO_MMIO_QUEUE_NOTIFY	0x050
#define VIRTIO_MMIO_INTERRUPT_STATUS	0x060
#define VIRTIO_MMIO_INTERRUPT_ACK	0x064
#define VIRTIO_MMIO_STATUS		0x070
#define VIRTIO_MMIO_QUEUE_DESC_LOW	0x080
#define VIRTIO_MMIO_QUEUE_DESC_HIGH	0x084
#define VIRTIO_MMIO_QUEUE_AVAIL_LOW	0x090
#define VIRTIO_MMIO_QUEUE_AVAIL_HIGH	0x094
#define VIRTIO_MMIO_QUEUE_USED_LOW	0x0a0
#define VIRTIO_MMIO_QUEUE_USED_HIGH	0x0a4
#define VIRTIO_MMIO_CONFIG_GENERATION	0x0fc
#define VIRTIO_MMIO_CONFIG		0x100

/* PCI registers */
#define VIRTIO_PCI_DEVICE_FEATURE_SEL	0x000
#define VIRTIO_PCI_DEVICE_FEATURE	0x004
#define VIRTIO_PCI_GUEST_FEATURE_SEL	0x008
#define VIRTIO_PCI_GUEST_FEATURE	0x00c
#define VIRTIO_PCI_MSIX_CONFIG          0x010
#define VIRTIO_PCI_NUM_QUEUES           0x012
#define VIRTIO_PCI_DEVICE_STATUS        0x014
#define VIRTIO_PCI_CONFIG_GENERATION    0x015
#define VIRTIO_PCI_QUEUE_SEL		0x016
#define VIRTIO_PCI_QUEUE_SIZE	        0x018
#define VIRTIO_PCI_QUEUE_MSIX_VECTOR    0x01a
#define VIRTIO_PCI_QUEUE_ENABLE         0x01c
#define VIRTIO_PCI_QUEUE_NOTIFY_OFF     0x01e
#define VIRTIO_PCI_QUEUE_DESC_LOW	0x020
#define VIRTIO_PCI_QUEUE_DESC_HIGH	0x024
#define VIRTIO_PCI_QUEUE_AVAIL_LOW	0x028
#define VIRTIO_PCI_QUEUE_AVAIL_HIGH	0x02c
#define VIRTIO_PCI_QUEUE_USED_LOW	0x030
#define VIRTIO_PCI_QUEUE_USED_HIGH	0x034

#define VIRTIO_PCI_CFG_OFFSET          0x0000
#define VIRTIO_PCI_ISR_OFFSET          0x1000
#define VIRTIO_PCI_CONFIG_OFFSET       0x2000
#define VIRTIO_PCI_NOTIFY_OFFSET       0x3000

#define VIRTIO_PCI_CAP_LEN 16

#define MAX_QUEUE 8
#define MAX_CONFIG_SPACE_SIZE 256
#define MAX_QUEUE_NUM 16 

#define VRING_DESC_F_NEXT	1
#define VRING_DESC_F_WRITE	2
#define VRING_DESC_F_INDIRECT	4

#define VIRTIO_BLK_T_IN          0
#define VIRTIO_BLK_T_OUT         1
#define VIRTIO_BLK_T_FLUSH       4
#define VIRTIO_BLK_T_FLUSH_OUT   5

#define VIRTIO_BLK_S_OK     0
#define VIRTIO_BLK_S_IOERR  1
#define VIRTIO_BLK_S_UNSUPP 2

typedef struct virtual_io_device virtual_io_device_t;
typedef int virtual_io_device_recieve_func(virtual_io_device_t *device, int queue_index, int desc_index, int read_size, int write_size);


typedef struct
{
  uint64_t addr;
  uint32_t len;
  uint16_t flags;
  uint16_t next;
} virtual_io_desc_t;

typedef struct
{
  bool ready; /* 0 or 1 */
  uint32_t num;
  uint16_t last_avail_idx;
  uint64_t desc_addr;
  uint64_t avail_addr;
  uint64_t used_addr;
  bool manual_recv; /* if true, the device_recv() callback is not called */
} queue_state_t;


typedef struct
{
  uint32_t type;
  uint8_t *buf;
  int write_size;
  int queue_idx;
  int desc_idx;
} block_request_t;

typedef struct
{
  uint32_t type;
  uint32_t ioprio;
  uint64_t sector_num;
} block_request_header_t;


struct virtual_io_device
{
  irq_signal_t *irq;
  int debug;
  uint32_t int_status;
  uint32_t status;
  uint32_t device_features_select;
  uint32_t queue_select;
  queue_state_t queue[MAX_QUEUE];
  cpu_state_t *cpu_state;

  /* device specific */
  uint32_t device_id;
  uint32_t vendor_id;
  uint32_t device_features;
  virtual_io_device_recieve_func *device_recv;
  void (*config_write)(virtual_io_device_t *device);

  uint32_t config_space_size;
  uint8_t config_space[MAX_CONFIG_SPACE_SIZE];
};

typedef struct
{
  uint64_t addr;
  irq_signal_t *irq;
} virtual_io_bus_t;

extern int virtual_block_recv_request(virtual_io_device_t *device, int queue_idx,
    int desc_idx, int read_size, int write_size);
extern void virtio_init(cpu_state_t *state, address_item_t *handler,
    virtual_io_device_t *device,
    virtual_io_bus_t *bus, uint32_t device_id, 
    int config_space_size,
    virtual_io_device_recieve_func *device_recv);
#endif
