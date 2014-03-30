#ifdef CHANGED_3

#include "kernel/thread.h"
#include "lib/debug.h"
#include "drivers/device.h"
#include "drivers/gnd.h"
#include "net/network.h"
#include "net/pop.h"
#include "kernel/assert.h"

void nic_test_main(void) {
    int data = 1;
    void *buffer = &data;
    network_address_t addr = 0x0f01beef;
    network_address_t broadcast = network_get_broadcast_address();
    network_address_t loopback = network_get_loopback_address();
    broadcast = loopback;
    loopback = broadcast;
    uint16_t sport = 1;
    uint16_t rport = 2;
    sock_t send_socket = socket_open(0x01, sport);
    sock_t recv_socket = socket_open(0x01, rport);
    KERNEL_ASSERT(send_socket >= 0 && recv_socket >= 0);
    kprintf("Starting NIC test\n");
    socket_sendto(send_socket, addr, rport, buffer, sizeof(int));
    socket_recvfrom(recv_socket, &addr, &sport, buffer, sizeof(int), &data);
    while(1) {};
}

#endif
