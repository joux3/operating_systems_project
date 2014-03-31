#ifdef CHANGED_3

#include "kernel/thread.h"
#include "lib/debug.h"
#include "drivers/device.h"
#include "drivers/gnd.h"
#include "net/network.h"
#include "net/pop.h"
#include "kernel/assert.h"

#define POP_PROTOCOL 0x01

sock_t send_socket;
sock_t recv_socket;
uint16_t sport;
uint16_t rport;
network_address_t addr;

void send_thread(uint32_t param) {
    int send_data = 0;
    void *buffer = &send_data;
    int send_length = sizeof(int);
    param++;
    while (1) {
        kprintf("Sending from socket %d to addr %x\n", send_socket, addr);
        kprintf("Sending data %d\n", send_data++);
        socket_sendto(send_socket, addr, rport, buffer, send_length);
        thread_sleep(5000);
    }
}

void recv_thread(uint32_t param) {
    param++;
    int recv_data;
    int received_bytes;
    network_address_t sender_addr;
    void* buffer = &recv_data;
    int max_length = sizeof(int);
    while (1) {
        kprintf("receiving from socket %d\n", recv_socket);
        socket_recvfrom(recv_socket, &sender_addr, &sport, buffer, max_length, &received_bytes);
        kprintf("Reciefed the following things:\n");
        kprintf("Message from address %x\n", sender_addr);
        kprintf("Message content %d\n", recv_data);
        thread_sleep(5000);
    }
}

void nic_test_main(void) {
    addr = 0x0f01beef;
    sport = 1;
    rport = 2;
    send_socket = socket_open(POP_PROTOCOL, sport);
    recv_socket = socket_open(POP_PROTOCOL, rport);
    KERNEL_ASSERT(send_socket >= 0 && recv_socket >= 0);
    kprintf("Starting NIC test\n");
    TID_t thread;
    thread = thread_create(&send_thread, 1);
    thread_run(thread);
    thread = thread_create(&recv_thread, 1);;
    thread_run(thread);
    while(1) {};
}



#endif
