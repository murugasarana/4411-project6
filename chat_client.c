#include "minithread.h"
#include "minimsg.h"
#include "read.h"
#include "read_private.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#define BUFFER_SIZE 256

miniport_t sender;
miniport_t receiver;

// Should be started when port is available
int receive(int* arg) {
    char buffer[BUFFER_SIZE];
    int length;
    miniport_t from;
    
    while(1) {
        length = BUFFER_SIZE;
        minimsg_receive(receiver, &from, buffer, &length);
        printf("RECEIVED: %s\n", buffer);
        miniport_destroy(from);
    }
    
    return 0;
}

// Transmit portion.
int transmit(int *arg) {
    char in;
    char buffer[BUFFER_SIZE];
    int source_port, port, length;
    network_address_t addr;
    
    // Initialize keyboard
    miniterm_initialize();
    printf("Local Port: ");
    miniterm_read(buffer, 6);
    sscanf(buffer, "%d", &source_port);
    
    printf("Specify destination address: ");
    miniterm_read(buffer, 21);
    network_translate_hostname(buffer, addr);
    
    printf("Specify destination port: ");
    miniterm_read(buffer, 6);
    sscanf(buffer, "%d", &port);
    
    //network_get_my_address(addr);
    
    // Create both ports
    receiver = miniport_create_unbound(source_port);
    sender = miniport_create_bound(addr, port);
    
    minithread_fork(receive, NULL);
    
    printf("Connected! You may begin typing.\n");
    
    while(1) {
        miniterm_read(buffer, 100);
        length = strlen(buffer) + 1;
        minimsg_send(receiver, sender, buffer, length);
    }
}

int main() {
    minithread_system_initialize(transmit, NULL);
}