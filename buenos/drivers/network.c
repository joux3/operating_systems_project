#ifdef CHANGED_3

#include "kernel/kmalloc.h"
#include "kernel/panic.h"
#include "kernel/assert.h"
#include "kernel/semaphore.h"
#include "kernel/spinlock.h"
#include "kernel/thread.h"
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

    ((nic_io_area_t*)dev->io_address)->command = NIC_COMMAND_CLEAR_RXIRQ;
    ((nic_io_area_t*)dev->io_address)->command = NIC_COMMAND_CLEAR_RIRQ;
    ((nic_io_area_t*)dev->io_address)->command = NIC_COMMAND_CLEAR_SIRQ;
    ((nic_io_area_t*)dev->io_address)->command = NIC_COMMAND_CLEAR_RXBUSY;
    ((nic_io_area_t*)dev->io_address)->command = NIC_COMMAND_CLEAR_RXIRQ;

    gnd->device = dev;
    gnd->send = nic_send;
    gnd->recv = nic_recv;
    gnd->frame_size = nic_frame_size;
    gnd->hwaddr = nic_hwaddr;
    
    spinlock_reset(&real_dev->slock);
    real_dev->send_sleepq = 0;
    real_dev->recv_sleepq = 0;
    real_dev->msg_recvd = 0;
    real_dev->recv_done_sleepq = 0;

    irq_mask = 1 << (desc->irq + 10);
    interrupt_register(irq_mask, nic_interrupt_handle, dev);

    return dev;
}


static void nic_interrupt_handle(device_t *device) {
    
    nic_real_device_t *real_dev = device->real_device;
    nic_io_area_t *io = (nic_io_area_t*)device->io_address;

    //DEBUG("nic_test", "NIC DRIVER INTERRUPT HANDLER: interrupt in NIC\n");
    TID_t tid = thread_get_current_thread();
    DEBUG("nic_test", "NIC DRIVER INTERRUPT HANDLER: thread id %d \n", tid );
    
    spinlock_acquire(&real_dev->slock);
    
    if (NIC_STATUS_SIRQ(io->status)) {
        //DEBUG("nic_test", "NIC DRIVER INTERRUPT HANDLER: interrupt SIRQ\n");
        io->command = NIC_COMMAND_CLEAR_SIRQ;
        sleepq_wake(&real_dev->send_sleepq);
    }
    if (NIC_STATUS_RXIRQ(io->status)) {
        //DEBUG("nic_test", "NIC DRIVER INTERRUPT HANDLER: interrupt RXIRQ\n");
        DEBUG("nic_test", "NIC DRIVER INTERRUPT HANDLER: waking %x\n", &real_dev->recv_sleepq);
        io->command = NIC_COMMAND_CLEAR_RXIRQ;
        real_dev->msg_recvd = 1;
        sleepq_wake(&real_dev->recv_sleepq);
    }
    if(NIC_STATUS_RIRQ(io->status)) {
        //DEBUG("nic_test", "NIC DRIVER INTERRUPT HANDLER: interrupt RIRQ\n");
        io->command = NIC_COMMAND_CLEAR_RIRQ;
        io->command = NIC_COMMAND_CLEAR_RXBUSY;
        sleepq_wake(&real_dev->recv_done_sleepq);
    }
    
    spinlock_release(&real_dev->slock);
}

static int nic_send(gnd_t *gnd, void *frame, network_address_t addr) {
    
    DEBUG("nic_test", "NIC DRIVER SEND: entering nic_send, sending %d to %x\n", frame, addr);
    TID_t tid = thread_get_current_thread();
    DEBUG("nic_test", "NIC SEND: thread id %d \n", tid );
    addr = addr; // Address is no longer needed, since we copy from
                 // buffer to buffer

    interrupt_status_t intr_status;
    nic_real_device_t *real_dev = gnd->device->real_device;
    nic_io_area_t *io = (nic_io_area_t*)gnd->device->io_address;

    intr_status = _interrupt_disable();
    spinlock_acquire(&real_dev->slock);

    while (NIC_STATUS_SBUSY(io->status)) {
        DEBUG("nic_test", "NIC DRIVER SEND: putting send to sleep in nic send\n");
        sleepq_add(&real_dev->send_sleepq);
        spinlock_release(&real_dev->slock);
        thread_switch();
        DEBUG("nic_test", "NIC DRIVER SEND: waking up in nic_send\n");
        spinlock_acquire(&real_dev->slock);
    }
    
    io->dmaaddr = (uint32_t)frame;
    io->command = NIC_COMMAND_DMA_SEND;
    if (NIC_STATUS_EBUSY(io->status) || NIC_STATUS_ERROR(io->status))
        KERNEL_PANIC("NIC DRIVER SEND: Failed at NIC send");

    spinlock_release(&real_dev->slock);
    _interrupt_set_state(intr_status);
   
    DEBUG("nic_test", "NIC DRIVER SEND: leaving nic_send\n");

    return 0;
}
static int nic_recv(gnd_t *gnd, void *frame) {

    DEBUG("nic_test", "NIC DRIVER RECV: entering nic_recv, buffer %x, hwaddr %x\n", frame, gnd->hwaddr(gnd));

    TID_t tid = thread_get_current_thread();
    DEBUG("nic_test", "NIC RECV: thread id %d \n", tid );
    interrupt_status_t intr_status;
    nic_real_device_t *real_dev = gnd->device->real_device;
    nic_io_area_t *io = (nic_io_area_t*)gnd->device->io_address;

    intr_status = _interrupt_disable();
    spinlock_acquire(&real_dev->slock);

    while (NIC_STATUS_RBUSY(io->status) || /*!NIC_STATUS_RXIRQ(io->status)*/ !real_dev->msg_recvd) {
        DEBUG("nic_test", "NIC DRIVER RECV: putting recv to sleep, waiting for %x\n", &real_dev->recv_sleepq);
        sleepq_add(&real_dev->recv_sleepq);
        spinlock_release(&real_dev->slock);
        thread_switch();
        DEBUG("nic_test", "NIC DRIVER RECV: recv waking, waited on was %x\n", &real_dev->recv_sleepq);
        spinlock_acquire(&real_dev->slock);
    }
    
//    io->command = NIC_COMMAND_CLEAR_RXIRQ;
    real_dev->msg_recvd = 0;
    io->dmaaddr = (uint32_t)frame;
    io->command = NIC_COMMAND_DMA_RECV;
    if (NIC_STATUS_EBUSY(io->status) || NIC_STATUS_ERROR(io->status))
        KERNEL_PANIC("NIC DRIVER RECV: Failed at NIC recv");

    DEBUG("nic_test", "NIC DRIVER RECV: putting recv to recv_done sleep queue\n");
    sleepq_add(&real_dev->recv_done_sleepq);

    spinlock_release(&real_dev->slock);
    _interrupt_set_state(intr_status);
    thread_switch();

    DEBUG("nic_test", "NIC DRIVER RECV: leaving nic_recv\n");
    
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
