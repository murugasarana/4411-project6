#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "minimsg.h"

miniport_t ports[BOUND_MAX - BOUND_MIN + 1];

void
test_basic_miniport_create_bound() {
    int i;
    int size = BOUND_MAX - BOUND_MIN + 1;
    network_address_t addr;
    
    minimsg_initialize();
    network_get_my_address(addr);
    for(i = 0;i<=size; i++ ) {
        ports[i] = miniport_create_bound(addr, 0);
        if(i >=size) {
            assert(NULL == ports[i]);
        } else {
            assert(i+BOUND_MIN == miniport_get_port_number(ports[i]));
        }
    }
}

void
test_miniport_create_bound_with_delete() {
    network_address_t addr;
    miniport_t port;
    int j;
    j = 10;
    network_get_my_address(addr);
    miniport_destroy(ports[j]);

    port = miniport_create_bound(addr, 0);
    assert(j+BOUND_MIN == miniport_get_port_number(port));
    
    miniport_destroy(ports[j]);
    miniport_destroy(ports[0]);
    
    port = miniport_create_bound(addr, 0);
    assert(BOUND_MIN == miniport_get_port_number(port));
    
}

void
test_miniport_create_unbound() {
    miniport_t outside_range_port;
    miniport_t dedup_port;
    int i;
    int size = UNBOUND_MAX - UNBOUND_MIN + 1;
    miniport_t ports[size];
    for(i = 0; i< size; i++) {
        ports[i] = miniport_create_unbound(i);
        assert(i+UNBOUND_MIN == miniport_get_port_number(ports[i]));
    }
    outside_range_port = miniport_create_unbound(UNBOUND_MAX + 1);
    assert(outside_range_port == NULL);

    outside_range_port = miniport_create_unbound(UNBOUND_MIN - 1);
    assert(outside_range_port == NULL);

    dedup_port = miniport_create_unbound(UNBOUND_MIN + 10);
    assert(dedup_port == ports[10]);
    
}

int main() {
    fprintf(stdout, "Testing minimsg\n");
    test_basic_miniport_create_bound();
    test_miniport_create_bound_with_delete();
    test_miniport_create_unbound();
    fprintf(stdout, "Done!");
    return 0;
}
