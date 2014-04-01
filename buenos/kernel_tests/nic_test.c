#ifdef CHANGED_3

#include "kernel/thread.h"
#include "lib/debug.h"
#include "drivers/device.h"
#include "drivers/gnd.h"
#include "net/network.h"
#include "net/pop.h"
#include "kernel/assert.h"

#define POP_PROTOCOL    0x01
#define SOCKETS         1

sock_t send_sockets[SOCKETS];
sock_t recv_sockets[SOCKETS];
uint16_t sports[SOCKETS];
uint16_t rports[SOCKETS];
network_address_t addr;

void send_thread(uint32_t param) {
    int send_data = 0;
    void *buffer = &send_data;
    int send_length = sizeof(int);
    param++;
    int i, j = 0;
    while (j < 10) {
        for (i = 0; i < SOCKETS; i++) {
            kprintf("Sending from socket %d to addr %x\n", send_sockets[i], addr);
            //kprintf("Sending data %d\n", send_data++);
            socket_sendto(send_sockets[i], addr, rports[i], buffer, send_length);
            thread_switch();
            thread_sleep(500);
        }
    }
    kprintf("finished sending\n");
}

void recv_thread(uint32_t param) {
    param++;
    int recv_data;
    int received_bytes;
    uint16_t sport;
    network_address_t sender_addr;
    void* buffer = &recv_data;
    int max_length = sizeof(int);
    int i, j = 0;
    while (j < 10) {
        for (i = 0; i < SOCKETS; i++) {
            kprintf("receiving from socket %d\n", recv_sockets[i]);
            socket_recvfrom(recv_sockets[i], &sender_addr, &sport,
                            buffer, max_length, &received_bytes);
            //kprintf("Recieved the following things:\n");
            //kprintf("Message from address %x\n", sender_addr);
            //kprintf("Message content %d\n", recv_data);
            thread_switch();
            thread_sleep(500);
        }
    }
    kprintf("finished receiving\n");
}

void nic_test_main(void) {
    addr = 0x0f01beef;
    int i;
    for (i = 0; i < SOCKETS; i++) {
        sports[i] = i + 1;
        rports[i] = i + 1 + SOCKETS;
        kprintf("Creating sockets for ports %d and %d\n", sports[i], rports[i]);
        send_sockets[i] = socket_open(POP_PROTOCOL, sports[i]);
        recv_sockets[i] = socket_open(POP_PROTOCOL, rports[i]);
        kprintf("Created sockets %d and %d\n", send_sockets[i], recv_sockets[i]);
        KERNEL_ASSERT(send_sockets[i] >= 0 && recv_sockets[i] >= 0);
    }
    kprintf("Starting NIC test\n");
    TID_t thread;
    thread = thread_create(&send_thread, 1);
    thread_run(thread);
    thread = thread_create(&recv_thread, 1);;
    thread_run(thread);
    while(1) {};
}



#endif
