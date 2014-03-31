#ifdef CHANGED_3

#include "kernel/kmalloc.h"
#include "kernel/panic.h"
#include "kernel/assert.h"
#include "kernel/semaphore.h"
#include "kernel/spinlock.h"
#include "kernel/sleepq.h"
#include "kernel/interrupt.h"
#include "lib/libc.h"
#include "drivers/device.h"
#include "drivers/yams.h"
#include "drivers/gnd.h"
#include "drivers/network.h"
#include "lib/debug.h"

static void nic_interrupt_handle(device_t *device);
static int nic_send(gnd_t *gnd, void *frame, network_address_t addr);
static int nic_recv(gnd_t *gnd, void *frame);
static uint32_t nic_frame_size(gnd_t *gnd);
static network_address_t nic_hwaddr(gnd_t *gnd);

device_t *nic_init(io_descriptor_t *desc) {
    //DEBUG("nic_test", "In nic init!\n");
    device_t *dev;
    gnd_t *gnd;
    nic_real_device_t *real_dev;
    uint32_t irq_mask;

    dev = kmalloc(sizeof(device_t));
    gnd = kmalloc(sizeof(gnd_t));
    real_dev = kmalloc(sizeof(nic_real_device_t));
    if (dev == NULL || gnd == NULL || real_dev == NULL)
        KERNEL_PANIC("Could not allocate memory to NIC driver.");

    dev->generic_device = gnd;
    dev->real_device = real_dev;
    dev->descriptor =  desc;
    dev->io_address = desc->io_area_base;
    dev->type = desc->type;

    gnd->device = dev;
    gnd->send = nic_send;
    gnd->recv = nic_recv;
    gnd->frame_size = nic_frame_size;
    gnd->hwaddr = nic_hwaddr;

    irq_mask = 1 << (desc->irq + 10);
    interrupt_register(irq_mask, nic_interrupt_handle, dev);

    return dev;
}


static void nic_interrupt_handle(device_t *device) {
    
    nic_real_device_t *real_dev = device->real_device;
    nic_io_area_t *io = (nic_io_area_t*)device->io_address;
    
    spinlock_acquire(&real_dev->slock);
    
    if (NIC_STATUS_SIRQ(io->status)) {
        io->command = NIC_COMMAND_CLEAR_SIRQ;
        sleepq_wake(&real_dev->send_sleepq);
    }
    if (NIC_STATUS_RXIRQ(io->status)) {
        sleepq_wake(&real_dev->recv_sleepq);
    }
    if(NIC_STATUS_RIRQ(io->status)) {
        io->command = NIC_COMMAND_CLEAR_RIRQ;
        io->command = NIC_COMMAND_CLEAR_RXBUSY;
        sleepq_wake(&real_dev->recv_done_sleepq);
    }
    
    spinlock_release(&real_dev->slock);
}

static int nic_send(gnd_t *gnd, void *frame, network_address_t addr) {
    
    addr = addr; // Address is no longer needed, since we copy from
                 // buffer to buffer

    interrupt_status_t intr_status;
    nic_real_device_t *real_dev = gnd->device->real_device;
    nic_io_area_t *io = (nic_io_area_t*)gnd->device->io_address;

    intr_status = _interrupt_disable();
    spinlock_acquire(&real_dev->slock);

    while (NIC_STATUS_SBUSY(io->status)) {
        sleepq_add(&real_dev->send_sleepq);
        spinlock_release(&real_dev->slock);
        thread_switch();
        spinlock_acquire(&real_dev->slock);
    }
    
    io->dmaaddr = (uint32_t)frame;
    io->command = NIC_COMMAND_DMA_SEND;
    if (NIC_STATUS_EBUSY(io->status) || NIC_STATUS_ERROR(io->status))
        KERNEL_PANIC("Failed at NIC send");

    spinlock_release(&real_dev->slock);
    _interrupt_set_state(intr_status);
    
    return 0;
}
static int nic_recv(gnd_t *gnd, void *frame) {

    interrupt_status_t intr_status;
    nic_real_device_t *real_dev = gnd->device->real_device;
    nic_io_area_t *io = (nic_io_area_t*)gnd->device->io_address;

    intr_status = _interrupt_disable();
    spinlock_acquire(&real_dev->slock);

    while (NIC_STATUS_RBUSY(io->status) || !NIC_STATUS_RXIRQ(io->status)) {
        sleepq_add(&real_dev->recv_sleepq);
        spinlock_release(&real_dev->slock);
        thread_switch();
        spinlock_acquire(&real_dev->slock);
    }
    
    io->command = NIC_COMMAND_CLEAR_RXIRQ;
    io->dmaaddr = (uint32_t)frame;
    io->command = NIC_COMMAND_DMA_RECV;
    if (NIC_STATUS_EBUSY(io->status) || NIC_STATUS_ERROR(io->status))
        KERNEL_PANIC("Failed at NIC recv");

    sleepq_add(&real_dev->recv_done_sleepq);

    spinlock_release(&real_dev->slock);
    _interrupt_set_state(intr_status);
    thread_switch();
    
    return 0;
}

static uint32_t nic_frame_size(gnd_t *gnd) {
    
    interrupt_status_t intr_status;
    nic_real_device_t *real_dev = gnd->device->real_device;
    nic_io_area_t *io = (nic_io_area_t*)gnd->device->io_address;

    intr_status = _interrupt_disable();
    spinlock_acquire(&real_dev->slock);

    uint32_t frame_size = io->mtu;
    
    spinlock_release(&real_dev->slock);
    _interrupt_set_state(intr_status);
    
    return frame_size;
}

static network_address_t nic_hwaddr(gnd_t *gnd) {
    
    interrupt_status_t intr_status;
    nic_real_device_t *real_dev = gnd->device->real_device;
    nic_io_area_t *io = (nic_io_area_t*)gnd->device->io_address;

    intr_status = _interrupt_disable();
    spinlock_acquire(&real_dev->slock);

    uint32_t hwaddr = io->hwaddr;
    
    spinlock_release(&real_dev->slock);
    _interrupt_set_state(intr_status);
    
    return hwaddr;
}

#endif
