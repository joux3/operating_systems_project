#ifdef CHANGED_3

#include "kernel/thread.h"
#include "net/network.h"
#include "net/socket.h"
#include "kernel/assert.h"

#define POP_PROTOCOL    0x01
#define SOCKETS         5
#define TEST_RUNS       1000000

/* socket data from socket.c */
extern socket_descriptor_t open_sockets[CONFIG_MAX_OPEN_SOCKETS];
extern semaphore_t *open_sockets_sem;

sock_t send_sockets[SOCKETS];
sock_t recv_sockets[SOCKETS];
uint16_t sports[SOCKETS];
uint16_t rports[SOCKETS];
network_address_t addr;

void send_thread(uint32_t param) {
    kprintf("SENDING THREAD: Running send thread\n");
    int send_data = 0;
    void *buffer = &send_data;
    int send_length = sizeof(int);
    param++;
    int success;
    int i, j = 0;
    while (j++ < TEST_RUNS) {
        thread_sleep(500);
        for (i = 0; i < SOCKETS; i++) {
            kprintf("SENDING THREAD: Sending from socket %d to addr %x, destination port %d\n", send_sockets[i], addr, open_sockets[recv_sockets[i]].port);
            kprintf("SENDING THREAD: Sending data %d\n", ++send_data);
            success = socket_sendto(send_sockets[i], addr, open_sockets[recv_sockets[i]].port, buffer, send_length);
            if (success < 0)
                kprintf("SENDING_THREAD: send failed!\n");
            else
                kprintf("SENDING_THREAD: sending succeeded, sent %d bytes.\n", success);
            thread_switch();
            thread_sleep(500);
        }
    }
    kprintf("SENDING THREAD: finished sending\n");
}

void recv_thread(uint32_t param) {
    kprintf("RECEIVING THREAD: Running receive thread\n");
    param++;
    int recv_data;
    int received_bytes;
    uint16_t sport;
    network_address_t sender_addr;
    void* buffer = &recv_data;
    int max_length = sizeof(int);
    int success;
    int i, j = 0;
    while (j++ < TEST_RUNS) {
        for (i = 0; i < SOCKETS; i++) {
            kprintf("RECEIVEING THREAD: receiving from socket %d, port %d\n", recv_sockets[i], open_sockets[recv_sockets[i]].port);
            success = socket_recvfrom(recv_sockets[i], &sender_addr, &sport,
                            buffer, max_length, &received_bytes);
            if (success < 0)
                kprintf("RECEIVING THREAD: failed at receiving!\n");
            else
                kprintf("RECEIVING THREAD: succeeded at receiving, received %d bytes\n", success);
            kprintf("RECEIVEING THREAD: Recieved the following things:\n");
            kprintf("RECEIVEING THREAD: Message from address %x\n", sender_addr);
            kprintf("RECEIVEING THREAD: Message content %d\n", recv_data);
        }
    }
    kprintf("RECEIVEING THREAD: finished receiving\n");
}

int nic_test_main(void) {
    addr = 0x0f01beef;
    int i;
    for (i = 0; i < SOCKETS; i++) {
        sports[i] = i + 1;
        rports[i] = i + 1 + SOCKETS;
        kprintf("Creating sockets for ports %d and %d\n", sports[i], rports[i]);
        send_sockets[i] = socket_open(POP_PROTOCOL, sports[i]);
        recv_sockets[i] = socket_open(POP_PROTOCOL, rports[i]);
        kprintf("Created sockets %d and %d, bound to ports %d and %d\n", send_sockets[i], recv_sockets[i],
                open_sockets[send_sockets[i]].port, open_sockets[recv_sockets[i]].port);
        KERNEL_ASSERT(send_sockets[i] >= 0 && recv_sockets[i] >= 0);
    }
    kprintf("Starting NIC test\n");
    TID_t thread;
    thread = thread_create(&send_thread, 1);
    thread_run(thread);
    thread = thread_create(&recv_thread, 1);
    thread_run(thread);
    while(1) {}
    return 0;
}



#endif
