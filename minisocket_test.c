#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "minisocket.h"

void test_get_next_port() {
	int i, next;

	fprintf(stdout, "test_get_next_port\n");
	minisocket_initialize();
	// test getting every client port
	for (i = 0; i <= SOCKET_CLIENT_MAX; i++) {
		next =  get_next_client_port();
		assert(i == next);
	}
	for (i = 0; i<10;i++) {
		next = get_next_client_port();
		assert(-1 == next);
	}
	for (i=10;i<20;i++) {
		reclaim_port(i);
		next = get_next_client_port();
		assert(i == next);
	}
	for (i = 0; i<10;i++) {
		next = get_next_client_port();
		assert(-1 == next);
	}
}

int main() {
	fprintf(stdout, "Testing minisocket.h\n");
	test_get_next_port();
	fprintf(stdout, "Done!");
	return 0;
}