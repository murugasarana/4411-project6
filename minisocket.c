/*
 *	Implementation of minisockets.
 */
#include "minisocket.h"
#include "miniheader.h"
#include "minithread.h"
#include "miniroute.h"
#include "alarm.h"
#include "queue.h"
#include "synch.h"

enum SOCKET_STATE {
	START, LISTENING, CONNECTING, CONNECTED, CLOSING, CLOSED
};

struct minisocket
{
    int remote_port;
    int local_port;
	int seq;
	int ack;
	int state;
	int timed_out;
	int tries;
	minisocket_error error;
    queue_t incoming_data;
    queue_t alarms;
	queue_t buffer;
    network_address_t src_addr;
    network_address_t dest_addr;
	semaphore_t data_available;
	semaphore_t buffer_has_stuff;
	semaphore_t unable_to_close;
	semaphore_t listening;
};

typedef struct stream_data {
	int length;
	int offset;
	char *data;
} *stream_data_t;

int MY_DEBUG = 0;

minisocket_t ports[SOCKET_SERVER_MAX + 1];
queue_t client_free_ports;

void print_debug(const char* format) {
	if (MY_DEBUG) {
		fprintf(stdout, format);
		fprintf(stdout, "\n");
	}
}

void print_status(minisocket_t socket) {
	if (MY_DEBUG) {
		printf("Socket seq: %d ack: %d state %d\n", socket->seq, socket->ack, socket->state);
	}
}

/* Get the next available client port number, 
   the value returned will not be available again until
   calling reclaim_port with that value. */
int get_next_client_port() {
	void* next;

	return queue_dequeue(client_free_ports, &next) == 0 ? (int)next : -1;
}

/* Set a client port as available */
int reclaim_port(int port) {
	return queue_append(client_free_ports, (void *)port);
}

int
send_data_packet( minisocket_t socket, int message_type, int data_len, char *data) {
    mini_header_reliable_t header;
    int sent;
    
    header = (mini_header_reliable_t)malloc(sizeof(struct mini_header_reliable));
    if(header == NULL) {
        socket->error = SOCKET_OUTOFMEMORY;
        free(header);
        return -1;
    }
    header->protocol = PROTOCOL_MINISTREAM;
    pack_address(header->source_address, socket->src_addr);
    pack_unsigned_short(header->source_port, socket->local_port);
    pack_address(header->destination_address, socket->dest_addr);
    pack_unsigned_short(header->destination_port, socket->remote_port);
    header->message_type = (char)message_type;
    pack_unsigned_int(header->seq_number, socket->seq);
    pack_unsigned_int(header->ack_number, socket->ack);
    
    sent = miniroute_send_pkt(socket->dest_addr, MINISTREAM_HEADER_SIZE,
                            (char *) header, data_len, data);
    
    if(sent == -1) {
        socket->error = SOCKET_SENDERROR;
    }  
    free(header);
    return sent;
}

int send_control_packet(minisocket_t socket, int message_type) {
	return send_data_packet(socket, message_type, 0, NULL);
}

typedef struct wake_arg {
	int alarm_id;
	minisocket_t socket;
	semaphore_t sema;
} *wake_arg_t;

int wake_from_timeout(wake_arg_t arg) {
	if (arg == NULL || arg->sema == NULL || arg->socket == NULL) {
		print_debug("!!! Null argument in wake_from_timeout !!!");
		return -1;
	}
	arg->socket->timed_out = 1;
	semaphore_V(arg->sema);
	return 0;
}

/* Wakes up any thread waiting on the socket */
void wake_from_packet(minisocket_t socket) {
	wake_arg_t arg;

	socket->timed_out = 0;
	queue_dequeue(socket->alarms, (void **)&arg);
	if (arg != NULL) {
		semaphore_V(arg->sema);
	}
}

int delete_by_id(wake_arg_t arg, int *id) {
	return arg->alarm_id == *id ? 0 : -1;
}

/* This function will block until the socket times out
   or a packet of interest comes in */
void waiT(minisocket_t socket) {
	int timeout, alarm_id;
	semaphore_t sema;
	wake_arg_t arg;
	interrupt_level_t level;

	level = set_interrupt_level(DISABLED);
	arg = (wake_arg_t)malloc(sizeof(struct wake_arg));
	sema = semaphore_create();
	arg->socket = socket;
	arg->sema = sema;
	timeout = BASE_TIMEOUT * (1 << socket->tries);
	alarm_id = register_alarm(timeout, (proc_t)wake_from_timeout, (arg_t)arg);
	arg->alarm_id = alarm_id;
	queue_append(socket->alarms, (void *)arg);

	semaphore_P(sema);

	deregister_alarm(arg->alarm_id);
	queue_delete_by_predicate(socket->alarms, (PFany)delete_by_id, &arg->alarm_id);
	semaphore_destroy(arg->sema);
	free(arg);
	set_interrupt_level(level);
}

void listen_wait(minisocket_t socket) {
	if (socket->listening == NULL) {
		socket->listening = semaphore_create();
	}
	semaphore_P(socket->listening);
	semaphore_destroy(socket->listening);
	socket->listening = NULL;
}

void listen_wake(minisocket_t socket) {
	socket->timed_out = 0;
	semaphore_V(socket->listening);
}

int minisocket_handle_incoming_packet(int port, network_interrupt_arg_t *packet) {
	minisocket_t socket;
	int message_type;
	mini_header_reliable_t header;
	stream_data_t item;
	interrupt_level_t level;
	
	level = set_interrupt_level(DISABLED);
	header = (mini_header_reliable_t)(packet->buffer + sizeof(struct routing_header));
	socket = ports[port];
    if(socket == NULL || packet == NULL) {
        return -1;
    }
	message_type = header->message_type;
	printf("Packet seq: %d ack: %d type: %d\n", unpack_unsigned_int(header->seq_number), unpack_unsigned_int(header->ack_number), message_type);
	print_status(socket);
	if (!(unpack_unsigned_int(header->seq_number) == socket->ack + 1 ||
		((message_type == MSG_ACK || message_type == MSG_SYNACK) && packet->size - MINISTREAM_HEADER_SIZE - sizeof(struct routing_header) == 0 && unpack_unsigned_int(header->ack_number) == socket->seq))) {

		print_debug("Received bad packet");
		send_control_packet(socket, MSG_ACK);
		return -1;
	}
	socket->ack = unpack_unsigned_int(header->seq_number);
	switch (socket->state) {
	case START:
		break;
	case LISTENING:
		switch (message_type) {
		case MSG_SYN:
			print_debug("Handler received SYN in Listening");
			// send synack, go to Connecting
			socket->remote_port = unpack_unsigned_short(&header->source_port);
            unpack_address(&header->source_address, socket->dest_addr);
			send_control_packet(socket, MSG_SYNACK);
			socket->state = CONNECTING;
			listen_wake(socket);
			break;
		}
		break;
	case CONNECTING:
		switch (message_type) {
		case MSG_SYNACK:
			print_debug("Handler received SYNACK in Connecting");
			// acknowledge and go to Connected
			send_control_packet(socket, MSG_ACK);
			socket->state = CONNECTED;
			wake_from_packet(socket);
			break;
		case MSG_ACK:
			print_debug("Handler received ACK in Connecting");
			socket->state = CONNECTED;
			wake_from_packet(socket);
			break;
		}
		break;
	case CONNECTED:
		switch (message_type) {
		case MSG_SYNACK:
			print_debug("Handler received SYNACK in Connected");
			send_control_packet(socket, MSG_ACK);
			break;
		case MSG_ACK:
			print_debug("Handler received ACK in Connected");
			if (packet->size - MINISTREAM_HEADER_SIZE > 0) { // there's stuff in there
				print_debug("Got some data");
				item = (stream_data_t)malloc(sizeof(struct stream_data));
				item->length = packet->size - MINISTREAM_HEADER_SIZE - sizeof(struct routing_header);
				item->offset = 0;
				item->data = (char*)malloc(sizeof(char) * item->length);
				memcpy(item->data, packet->buffer + MINISTREAM_HEADER_SIZE + sizeof(struct routing_header), item->length);
				queue_append(socket->buffer, item);
				if (queue_length(socket->buffer) == 1) { // it was previously empty
					semaphore_V(socket->buffer_has_stuff);
				}
				send_control_packet(socket, MSG_ACK);
			}
			wake_from_packet(socket);
			break;
		case MSG_FIN:
			print_debug("Received FIN in Connected");
			socket->state = CLOSING;
			socket->unable_to_close = semaphore_create();
			register_alarm(15000, (proc_t)semaphore_V, (arg_t)socket->unable_to_close);
			send_control_packet(socket, MSG_ACK);
			break;
		}
		break;
	case CLOSING:
		switch (message_type) {
		case MSG_FIN:
			send_control_packet(socket, MSG_ACK);
			break;
		case MSG_ACK:
			socket->state = CLOSED;
			break;
		}
		break;
	case CLOSED:
		break;
	}
	free(packet);
	set_interrupt_level(level);
	return 0;
}

void minisocket_free(minisocket_t socket) {
	int alarm_id;
	interrupt_level_t level;

	level = set_interrupt_level(level);
    if(socket->alarms!= NULL) {
        while (queue_length(socket->alarms) > 0) {
            queue_dequeue(socket->alarms, (void **)&alarm_id);
            deregister_alarm(alarm_id);
        }
    }
	queue_free(socket->incoming_data);
    queue_free(socket->alarms);
	queue_free(socket->buffer);
	semaphore_destroy(socket->data_available);
	semaphore_destroy(socket->buffer_has_stuff);
	free(socket);
	set_interrupt_level(level);
}

minisocket_t
create_new_socket(int remote_port, int local_port, int starting_state, minisocket_error *error) {
	minisocket_t socket;

	socket = (minisocket_t)malloc(sizeof(struct minisocket));
	if (socket == NULL) {
		*error = SOCKET_OUTOFMEMORY;
		return NULL;
	}
    
	socket->remote_port = remote_port;
    socket->local_port = local_port;
	socket->seq = 1;
	socket->ack = 0;
	socket->state = starting_state;
	socket->tries = 0;
	socket->timed_out = 0;
	socket->error = SOCKET_NOERROR;
	socket->incoming_data = queue_new();
    socket->alarms = queue_new();
	socket->buffer = queue_new();
	socket->buffer_has_stuff = semaphore_create();
	socket->data_available = semaphore_create();
	socket->unable_to_close = NULL;
	socket->listening = NULL;
	if (socket->incoming_data == NULL ||
		socket->data_available == NULL || 
		socket->alarms == NULL ||
		socket->buffer == NULL ||
		socket->buffer_has_stuff == NULL) {

		minisocket_free(socket);
		*error = SOCKET_OUTOFMEMORY;
		return NULL;
	}
	return socket;
}

/* Initializes the minisocket layer. */
void minisocket_initialize()
{
	int i;

	print_debug("Initializing minisockets..");
	memset(ports, (int)NULL, SOCKET_SERVER_MAX + 1);
	client_free_ports = queue_new();
	if (client_free_ports == NULL) {
		print_debug("Out of memory in minisocket_initialize");
		return;
	}
	for (i=0;i<=SOCKET_CLIENT_MAX;i++) {
		queue_append(client_free_ports, (void *)i);
	}
}

/* 
 * Listen for a connection from somebody else. When communication link is
 * created return a minisocket_t through which the communication can be made
 * from now on.
 *
 * The argument "port" is the port number on the local machine to which the
 * client will connect.
 *
 * Return value: the minisocket_t created, otherwise NULL with the errorcode
 * stored in the "error" variable.
 */
minisocket_t
minisocket_server_create(int port, minisocket_error *error)
{
	minisocket_t socket;
	interrupt_level_t level;

	*error = SOCKET_NOERROR;
	// validate port number
	if (port <= 0 || port > SOCKET_CLIENT_MAX) {
		*error = SOCKET_INVALIDPARAMS;
		return NULL;
	}
	// create the port
	level = set_interrupt_level(DISABLED);
	if (ports[port] != NULL) {
		*error = SOCKET_PORTINUSE;
		set_interrupt_level(level);
		return NULL;
	}
	socket = create_new_socket(-1, port, LISTENING, error);
	if (*error == SOCKET_OUTOFMEMORY) {
		set_interrupt_level(level);
		return NULL;
	}
	ports[port] = socket;
    set_interrupt_level(level);

    network_get_my_address(socket -> src_addr);

	// state machine
    while(socket->state != CONNECTED && socket->error == SOCKET_NOERROR) {
		if (socket->state == LISTENING) {
			listen_wait(socket);
		} else {
			wait(socket);
		}
		if (socket->timed_out) {
			socket->tries++;
			socket->timed_out = 0;
			print_debug("Server timed out");
			switch (socket->state) {
			case CONNECTING:
				send_control_packet(socket, MSG_SYNACK);
			}
		} else {
			socket->tries = 0;
		}
		if (socket->tries > MAX_TRIES) {
			print_debug("Server exceeded max tries");
			socket->state = LISTENING;
			socket->tries = 0;
			socket->timed_out = 0;
		}
	}

    if (socket->error != SOCKET_NOERROR) {
		level = set_interrupt_level(DISABLED);
		minisocket_free(socket);
		ports[port] = NULL;
		return NULL;
	}
    print_debug("Server:Connection Established with client");
    return socket;
}

/*
 * Initiate the communication with a remote site. When communication is
 * established create a minisocket through which the communication can be made
 * from now on.
 *
 * The first argument is the network address of the remote machine. 
 *
 * The argument "port" is the port number on the remote machine to which the
 * connection is made. The port number of the local machine is one of the free
 * port numbers.
 *
 * Return value: the minisocket_t created, otherwise NULL with the errorcode
 * stored in the "error" variable.
 */
minisocket_t
minisocket_client_create(network_address_t addr, int port, minisocket_error *error) {
	minisocket_t socket;
	int local_port;
	interrupt_level_t level;
	
	*error = SOCKET_NOERROR;
	// validate port number
	if (port <= 0 || port > SOCKET_CLIENT_MAX) {
		*error = SOCKET_INVALIDPARAMS;
		return NULL;
	}
	level = set_interrupt_level(DISABLED);
	local_port = get_next_client_port()+SOCKET_CLIENT_MAX+1;
	if (local_port == -1) {
		*error = SOCKET_NOMOREPORTS;
		set_interrupt_level(level);
		return NULL;
	}
	socket = create_new_socket(port, local_port, START, error);
	if (*error == SOCKET_OUTOFMEMORY) {
		reclaim_port(local_port);
		set_interrupt_level(level);
		return NULL;
	}
	ports[local_port] = socket;
	set_interrupt_level(level);
	// begin state machine
    network_address_copy(addr, socket->dest_addr);
    network_get_my_address(socket->src_addr);
	// send syn, change to connecting
	print_debug("Client sending syn.");
	send_control_packet(socket, MSG_SYN);
	socket->state = CONNECTING;

    while (socket->state != CONNECTED && socket->error == SOCKET_NOERROR) {
		wait(socket);
		if (socket->timed_out) {
			socket->tries++;
			socket->timed_out = 0;
			print_debug("Client timed out");
			switch (socket->state) {
			case CONNECTING:
				print_debug("Client resending SYN");
				send_control_packet(socket, MSG_SYN);
				break;
			}
		} else {
			socket->tries = 0;
		}
		if (socket->tries > MAX_TRIES) {
			print_debug("Client exceeded max tries");
			socket->error = SOCKET_NOSERVER;
		}
    }
    
	if (socket->error != SOCKET_NOERROR) {
		level = set_interrupt_level(DISABLED);
		minisocket_free(socket);
		ports[local_port] = NULL;
		reclaim_port(port);
		set_interrupt_level(level);
		return NULL;
	}
    print_debug("Client: Connection established with server");
	return socket;
}


/* 
 * Send a message to the other end of the socket.
 *
 * The send call should block until the remote host has ACKnowledged receipt of
 * the message.  This does not necessarily imply that the application has called
 * 'minisocket_receive', only that the packet is buffered pending a future
 * receive.
 *
 * It is expected that the order of calls to 'minisocket_send' implies the order
 * in which the concatenated messages will be received.
 *
 * 'minisocket_send' should block until the whole message is reliably
 * transmitted or an error/timeout occurs
 *
 * Arguments: the socket on which the communication is made (socket), the
 *            message to be transmitted (msg) and its length (len).
 * Return value: returns the number of successfully transmitted bytes. Sets the
 *               error code and returns -1 if an error is encountered.
 */
int minisocket_send(minisocket_t socket, minimsg_t msg, int len, minisocket_error *error)
{
	int remaining, fragment_length;

	// verify that the socket is connected
	if (len < 0 || msg == NULL || socket == NULL || socket->state != CONNECTED) {
		*error = SOCKET_SENDERROR;
		return -1;
	}
	remaining = len;
	while (remaining > 0 && *error == SOCKET_NOERROR) {
		if (remaining + MINISTREAM_HEADER_SIZE + sizeof(struct routing_header) > MAX_NETWORK_PKT_SIZE) {
			fragment_length = MAX_NETWORK_PKT_SIZE - MINISTREAM_HEADER_SIZE - sizeof(struct routing_header);
		} else {
			fragment_length = remaining;
		}
		if (socket->tries == 0) {
			print_debug("Sending new data");
			socket->seq++;
		}
        send_data_packet(socket, MSG_ACK, fragment_length, msg + (len - remaining));
		wait(socket);
		if (socket->timed_out) {
			socket->tries++;
			socket->timed_out = 0;
			print_debug("Send timed out");
		} else {
			socket->tries = 0;
			socket->timed_out = 0;
			remaining -= fragment_length;
		}
		if (socket->tries > MAX_TRIES) {
			*error = SOCKET_SENDERROR;
		}
	}
    printf("Sent is %d\n", len-remaining);
	return len - remaining;
}

/*
 * Receive a message from the other end of the socket. Blocks until
 * some data is received (which can be smaller than max_len bytes).
 *
 * Arguments: the socket on which the communication is made (socket), the memory
 *            location where the received message is returned (msg) and its
 *            maximum length (max_len).
 * Return value: -1 in case of error and sets the error code, the number of
 *           bytes received otherwise
 */
int minisocket_receive(minisocket_t socket, minimsg_t msg, int max_len, minisocket_error *error)
{
	int received, size;
	stream_data_t item;
    interrupt_level_t level;

	received = 0;
	if (socket == NULL || socket->state != CONNECTED || max_len < 0 || msg == NULL) {
		*error = SOCKET_RECEIVEERROR;
		return -1;
	}
	
    level = set_interrupt_level(DISABLED);
    semaphore_P(socket->buffer_has_stuff);
	while (received < max_len &&  queue_length(socket->buffer) > 0) {
        if (queue_dequeue(socket->buffer, (void **)&item) == -1) {
            set_interrupt_level(level);
			*error = SOCKET_RECEIVEERROR;
			return -1;
		}
        size = max_len - received >= item->length - item->offset ? item->length - item->offset : max_len - received;
		memcpy(msg + received, item->data + item->offset, size);
		received += size;
		if (item->length - item->offset - size > 0) {
			item->offset = item->offset + size;
			if (queue_prepend(socket->buffer, (void *)item) == -1) {
                set_interrupt_level(level);
				*error = SOCKET_RECEIVEERROR;
				return -1;
			}
        } else {
			free(item->data);
			free(item);
		}
	}
	if (queue_length(socket->buffer) > 0) {
		semaphore_V(socket->buffer_has_stuff);
	}
    set_interrupt_level(level);
	return received;
}

/* Close a connection. If minisocket_close is issued, any send or receive should
 * fail.  As soon as the other side knows about the close, it should fail any
 * send or receive in progress. The minisocket is destroyed by minisocket_close
 * function.  The function should never fail.
 */
void minisocket_close(minisocket_t socket)
{
	interrupt_level_t level;
    int alarm_id;

	// Set state to closing, which should fail any send or receive
	level = set_interrupt_level(DISABLED);
	if (socket->unable_to_close != NULL) {
		semaphore_P(socket->unable_to_close);
	}
	if (socket == NULL || socket->state == CLOSING) {
		set_interrupt_level(level);
		return;
	}
	socket->state = CLOSING;
	set_interrupt_level(level);

	// send fin and wait for finack if possible
	while (socket->state != CLOSED && socket->tries <= MAX_TRIES) {
		send_control_packet(socket, MSG_FIN);
		wait(socket);
		if (socket->timed_out) {
			socket->tries++;
			socket->timed_out = 0;
		}
	}
	
	// destroy the socket
	level = set_interrupt_level(DISABLED);
	ports[socket->local_port] = NULL;
	if (socket->local_port <= SOCKET_CLIENT_MAX) {
		reclaim_port(socket->local_port);
	}
	if(socket->alarms!= NULL) {
        while (queue_length(socket->alarms) > 0) {
            queue_dequeue(socket->alarms, (void **)&alarm_id);
            deregister_alarm(alarm_id);
        }
    }
	queue_free(socket->incoming_data);
    queue_free(socket->alarms);
	queue_free(socket->buffer);
	semaphore_destroy(socket->data_available);
	semaphore_destroy(socket->buffer_has_stuff);
	free(socket);

    set_interrupt_level(level);
	print_debug("Socket closed");
}
