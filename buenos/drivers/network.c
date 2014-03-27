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
#include "drivers/gbd.h"
#include "drivers/network.h"

device_t *nic_init(io_descriptor_t *desc) {
    desc = desc;
    return NULL;
}

#endif
