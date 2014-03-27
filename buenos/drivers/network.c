#ifdef CHANGED_3

#include "kernel/kmalloc.h"
#include "kernel/panic.h"
#include "kernel/assert.h"
#include "kernel/semaphore.h"
#include "kernel/spinlock.h"
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
    DEBUG("nic_test", "In nic init!\n");
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
    DEBUG("nic_test", "In nic send!\n");
    device = device;
}

static int nic_send(gnd_t *gnd, void *frame, network_address_t addr) {
    DEBUG("nic_test", "In nic send!\n");
    gnd = gnd;
    frame = frame;
    addr = addr;
    return 0;
}
static int nic_recv(gnd_t *gnd, void *frame) {
    DEBUG("nic_test", "In nic send!\n");
    gnd = gnd;
    frame = frame;
    return 0;
}
static uint32_t nic_frame_size(gnd_t *gnd) {
    DEBUG("nic_test", "In nic send!\n");
    gnd = gnd;
    return 0;
}
static network_address_t nic_hwaddr(gnd_t *gnd) {
    DEBUG("nic_test", "In nic send!\n");
    gnd = gnd;
    return 0;
}

#endif
