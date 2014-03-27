#ifdef CHANGED_3

#include "kernel/thread.h"
#include "lib/debug.h"
#include "drivers/device.h"
#include "drivers/gnd.h"
#include "net/network.h"

void nic_test_main(void) {
    void *buffer = 0x0;
    network_send(network_get_broadcast_address(),
                 1,
                 1,
                 1,
                 buffer);
    kprintf("Starting NIC test\n");
    while(1) {};
}

#endif
