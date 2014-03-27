#ifdef CHANGED_3

#ifndef DRIVERS_NIC_H
#define DRIVERS_NIC_H

#include "lib/libc.h"
#include "kernel/spinlock.h"
#include "kernel/semaphore.h"
#include "drivers/device.h"
#include "drivers/yams.h"
#include "drivers/gbd.h"


#define NIC_COMMAND_DMA_RECV        0x01
#define NIC_COMMAND_DMA_SEND        0x02
#define NIC_COMMAND_CLEAR_RXIRQ     0x03
#define NIC_COMMAND_CLEAR_RIRQ      0x04
#define NIC_COMMAND_CLEAR_SIRQ      0x05
#define NIC_COMMAND_CLEAR_RXBUSY    0x06
#define NIC_COMMAND_ENTER_PROM      0x07
#define NIC_COMMAND_EXIT_PROM       0x08

#define NIC_STATUS_RXBUSY(status)  ((status) & 0x00000001)
#define NIC_STATUS_RBUSY(status)  ((status) & 0x000000002)
#define NIC_STATUS_SBUSY(status)  ((status) & 0x00000004)
#define NIC_STATUS_RXIRQ(status)  ((status) & 0x00000008)
#define NIC_STATUS_RIRQ(status)  ((status) & 0x0000000a)
#define NIC_STATUS_SIRQ(status)  ((status) & 0x00000010)
#define NIC_STATUS_PROM(status)  ((status) & 0x00000020)
#define NIC_STATUS_NOFRAME(status)  ((status) & 0x08000000)
#define NIC_STATUS_IADDR(status)  ((status) & 0x10000000)
#define NIC_STATUS_ICOMM(status)  ((status) & 0x20000000)
#define NIC_STATUS_EBUSY(status)  ((status) & 0x40000000)
#define NIC_STATUS_ERROR(status)  ((status) & 0xf8000000)

typedef struct {
    volatile uint32_t status;
    volatile uint32_t command;
    volatile uint32_t hwaddr;
    volatile uint32_t mtu;
    volatile uint32_t dmaaddr;
} nic_io_area_t;

typedef struct {
    spinlock_t slock;
    uint8_t send_sleepq;
    uint8_t recv_sleepq;
    uint8_t recv_done_sleepq;
} nic_real_device_t;

device_t *nic_init(io_descriptor_t *desc);

#endif // Include guard

#endif
