#ifdef CHANGED_3

#include "kernel/thread.h"
#include "lib/debug.h"
#include "drivers/device.h"
#include "drivers/gnd.h"

void nic_test_main(void) {
    gnd_t *nic_driver = (gnd_t*)device_get(YAMS_TYPECODE_NIC, 0)->generic_device;
    void *frame = nic_driver;
    network_address_t addr = 1;
    nic_driver->send(nic_driver, frame, addr);
    kprintf("Starting NIC test\n");
    while(1) {};
}

#endif
