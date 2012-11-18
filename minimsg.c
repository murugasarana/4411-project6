/*
 *	Implementation of minimsgs and miniports.
 */
#include "minimsg.h"
#include "miniheader.h"
#include "miniroute.h"
#include "network.h"
#include "queue.h"
#include "synch.h"
#include "interrupts.h"

int next_bound;
miniport_t unbound_ports[UNBOUND_MAX - UNBOUND_MIN + 1];
miniport_t bound_ports[BOUND_MAX - BOUND_MIN + 1];
semaphore_t next_bound_mutex;
semaphore_t bound_ports_mutex;
semaphore_t unbound_ports_mutex;

struct miniport
{
	enum port_type type;
	int port_number;

	union {
		struct {
			queue_t incoming_data;
            semaphore_t incoming_data_mutex;
			semaphore_t datagrams_ready;
		} unbound;

		struct {
			network_address_t remote_address;
			int remote_unbound_port;
		} bound;
	};
};

int miniport_get_port_number(miniport_t port) {
    return port -> port_number;
}

int miniport_unbound_enqueue(miniport_t port, network_interrupt_arg_t *data) {
	interrupt_level_t level;

    if (port->type == UNBOUND) {
		level = set_interrupt_level(DISABLED);
        queue_append(port->unbound.incoming_data, data);
        semaphore_V(port->unbound.datagrams_ready);
		set_interrupt_level(level);
        //fprintf(stdout, "hi\t");
        return 0;
	} else {
		return -1;
	}

}

int nextBound() {
	int i, bound, size;
    interrupt_level_t level;

	size = BOUND_MAX - BOUND_MIN + 1;
    level = set_interrupt_level(DISABLED);
    for (i = 0; i < size; i++) {
        if (bound_ports[(i + next_bound) % size] == NULL) {
            bound = i + next_bound;
            next_bound = (bound + 1) % size;
            set_interrupt_level(level);
            return (bound % size) + BOUND_MIN;
        }
    }
    set_interrupt_level(level);
    return -1;
}

/* performs any required initialization of the minimsg layer.
 */
void minimsg_initialize()
{
	next_bound = 0;
	memset(unbound_ports, (int)NULL, UNBOUND_MAX - UNBOUND_MIN);
	memset(bound_ports, (int)NULL, BOUND_MAX - BOUND_MIN);
}

/* Creates an unbound port for listening. Multiple requests to create the same
 * unbound port should return the same miniport reference. It is the responsibility
 * of the programmer to make sure he does not destroy unbound miniports while they
 * are still in use by other threads -- this would result in undefined behavior.
 * Unbound ports must range from 0 to 32767. If the programmer specifies a port number
 * outside this range, it is considered an error.
 */
miniport_t miniport_create_unbound(int port_number)
{
	miniport_t port;
    interrupt_level_t level;

    if (port_number < UNBOUND_MIN || port_number > UNBOUND_MAX) {
		return NULL;
	}
	
    level = set_interrupt_level(DISABLED);
    port = unbound_ports[port_number - UNBOUND_MIN];
    if (port == NULL) {
		port = (miniport_t) malloc(sizeof(struct miniport));
		if (port == NULL) {
            set_interrupt_level(level);
			return NULL;
		}
		port->type = UNBOUND;
		port->port_number = port_number;
		port->unbound.incoming_data = queue_new();
		if (port->unbound.incoming_data == NULL) {
			free(port);
            set_interrupt_level(level);
			return NULL;
		}
        port->unbound.incoming_data_mutex = semaphore_create();
        semaphore_initialize(port->unbound.incoming_data_mutex, 1);
        if(port->unbound.incoming_data_mutex == NULL) {
            queue_free(port->unbound.incoming_data);
			free(port);
            set_interrupt_level(level);
			return NULL;
        }
		port->unbound.datagrams_ready = semaphore_create();
		if (port->unbound.datagrams_ready == NULL) {
			queue_free(port->unbound.incoming_data);
            semaphore_destroy(port->unbound.incoming_data_mutex);
			free(port);
            set_interrupt_level(level);
			return NULL;
		}
        
		unbound_ports[port_number - UNBOUND_MIN] = port;
    }
    set_interrupt_level(level);
	return port;
}

/* Creates a bound port for use in sending packets. The two parameters, addr and
 * remote_unbound_port_number together specify the remote's listening endpoint.
 * This function should assign bound port numbers incrementally between the range
 * 32768 to 65535. Port numbers should not be reused even if they have been destroyed,
 * unless an overflow occurs (ie. going over the 65535 limit) in which case you should
 * wrap around to 32768 again, incrementally assigning port numbers that are not
 * currently in use.
 */
miniport_t miniport_create_bound(network_address_t addr, int remote_unbound_port_number)
{
	miniport_t port;
    interrupt_level_t level;
	int port_number;
	if (remote_unbound_port_number < UNBOUND_MIN || remote_unbound_port_number > UNBOUND_MAX 
			|| addr == NULL) {
		return NULL;
	}
	port_number = nextBound();
	if (port_number == -1) {
		return NULL;
	}
	port = (miniport_t) malloc(sizeof(struct miniport));
	if (port == NULL) {
		return NULL;
	}
	port->type = BOUND;
	port->port_number = port_number;
	network_address_copy(addr, port->bound.remote_address);
	port->bound.remote_unbound_port = remote_unbound_port_number;
	
    level = set_interrupt_level(DISABLED);
    bound_ports[port_number - BOUND_MIN] = port;
    set_interrupt_level(level);
    
    return port;
}

/* Destroys a miniport and frees up its resources. If the miniport was in use at
 * the time it was destroyed, subsequent behavior is undefined.
 */
void miniport_destroy(miniport_t miniport)
{
	interrupt_level_t level;
    if (miniport != NULL) {
		level = set_interrupt_level(DISABLED);
		if (miniport->type == UNBOUND) {
			semaphore_destroy(miniport->unbound.datagrams_ready);
            queue_free(miniport->unbound.incoming_data);
            semaphore_destroy(miniport->unbound.incoming_data_mutex);
            unbound_ports[miniport -> port_number - UNBOUND_MIN] = NULL;
		} else {
            bound_ports[miniport -> port_number - BOUND_MIN] = NULL;
        }
		set_interrupt_level(level);
		free(miniport);
	}
}

/* Sends a message through a locally bound port (the bound port already has an associated
 * receiver address so it is sufficient to just supply the bound port number). In order
 * for the remote system to correctly create a bound port for replies back to the sending
 * system, it needs to know the sender's listening port (specified by local_unbound_port).
 * The msg parameter is a pointer to a data payload that the user wishes to send and does not
 * include a network header; your implementation of minimsg_send must construct the header
 * before calling network_send_pkt(). The return value of this function is the number of
 * data payload bytes sent not inclusive of the header.
 */
int minimsg_send(miniport_t local_unbound_port, miniport_t local_bound_port, minimsg_t msg, int len)
{
	int sent;
	mini_header_t header;
	network_address_t source_address;
	// validate arguments, fail if the packet is too big
	if (local_unbound_port == NULL ||
		local_bound_port == NULL || 
		msg == NULL ||
		len < 0 || 
		len > MINIMSG_MAX_MSG_SIZE) {

		return -1;
	}
	// form a miniheader
	header = (mini_header_t) malloc(HEADER_SIZE);
	if (header == NULL) {
		return -1;
	}
	// pack the header
	header->protocol = PROTOCOL_MINIDATAGRAM;
	pack_unsigned_short(header->source_port, local_unbound_port->port_number);
	network_get_my_address(source_address);
	pack_address(header->source_address, source_address);
	pack_unsigned_short(header->destination_port, local_bound_port->bound.remote_unbound_port);
	pack_address(header->destination_address, local_bound_port->bound.remote_address);
	// send
	sent = miniroute_send_pkt(local_bound_port->bound.remote_address, HEADER_SIZE, (char *) header, len, msg) - HEADER_SIZE;
	printf("miniroute_send_pkt %d\n", sent);
	free(header);
	return sent;
}

/* Receives a message through a locally unbound port. Threads that call this function are
 * blocked until a message arrives. Upon arrival of each message, the function must create
 * a new bound port that targets the sender's address and listening port, so that use of
 * this created bound port results in replying directly back to the sender. It is the
 * responsibility of this function to strip off and parse the header before returning the
 * data payload and data length via the respective msg and len parameter. The return value
 * of this function is the number of data payload bytes received not inclusive of the header.
 */
int minimsg_receive(miniport_t local_unbound_port, miniport_t* new_local_bound_port, minimsg_t msg, int *len)
{
	interrupt_level_t level;
    network_interrupt_arg_t *payload;
	int receiver_port;

	if (local_unbound_port == NULL) {
		return -1;
	}
    level = set_interrupt_level(DISABLED);
	semaphore_P(local_unbound_port->unbound.datagrams_ready);
    if(queue_dequeue(local_unbound_port->unbound.incoming_data, (void **)&payload)== 2) {
        fprintf(stdout, "error in minimsg_receive\n");
    }
    set_interrupt_level(level);
    *len = payload->size - HEADER_SIZE - sizeof(struct routing_header);
	memcpy(msg, payload->buffer + HEADER_SIZE + sizeof(struct routing_header), *len);

	receiver_port = unpack_unsigned_short(&payload->buffer[9]);
    *new_local_bound_port = miniport_create_bound(payload->addr, receiver_port);

	free(payload);
	return *len;
}
