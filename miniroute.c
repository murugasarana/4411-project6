#include "miniroute.h"
#include "miniheader.h"
#include "minithread.h"
#include "minimsg.h"
#include "network.h"
#include "synch.h"
#include "alarm.h"
#include "miniroute.h"
#include "hashtable.h"

typedef struct route_cache_entry
{
    network_address_t destination;
    int path_len;
    network_address_t path[MAX_ROUTE_LENGTH];
    
    semaphore_t routing_sem;
    //no. of threads waiting for a route to be found
    int waiting_count;
    
    // flag is 0 if routing is in progress, 1 if routing is completed successfully
    // 2 if routing has failed and threads must return NULL
    int routing_flag;
    int alarm_id;
    /* Cannot guarantee that the thread woken is the original initiator of the
     * route flood, we use this count variable to determine behavior
     */
    int retry_count;
    int routing_id;
} *route_cache_entry_t;

network_address_t local_address;
hashtable_t routing_cache;

unsigned short
network_address_comparator(network_address_t a, network_address_t *b) {
    return network_address_same(a, *b);
}

unsigned short
hash_wrapper(network_address_t *addr) {
    return hash_address(*addr);
}

/* Performs any initialization of the miniroute layer, if required. */
void
miniroute_initialize() {
    routing_cache = hashtable_init(SIZE_OF_ROUTE_CACHE, hash_wrapper, network_address_comparator);
    network_get_my_address(local_address);
}

// unpacks a path into an array supplied by caller
void
unpack_path(char buf[MAX_ROUTE_LENGTH][8], network_address_t *path) {
    int i;
    
    for (i = 0 ; i < MAX_ROUTE_LENGTH; i++) {
        unpack_address(buf[i], path[i]);
    }
}

// reverse a path
void
reverse_path(network_address_t copy_path[MAX_ROUTE_LENGTH], network_address_t *original_path, unsigned short len) {
    int i;
    
    for (i = 0; i <= len; i++) {
        network_address_copy(original_path[len - i], copy_path[i]);
    }
}

// packs a path into an array supplied by caller
void
pack_path(char buf[MAX_ROUTE_LENGTH][8], network_address_t *path) {
    int i;
    
    for (i = 0; i < MAX_ROUTE_LENGTH; i++) {
        pack_address(buf[i], path[i]);
    }
}

// returns 1 if provided path contains self, 0 otherwise
int
path_contains_self(network_address_t *path) {
    int i;
    
    for (i = 0; i < MAX_ROUTE_LENGTH; i++) {
        if (network_address_same(local_address, path[i])) return 1;
    }
    
    return 0;
}

// returns index where local address is located, -1 if not in path
int
find_self(network_address_t *path) {
    int i;
    
    for (i = 0; i < MAX_ROUTE_LENGTH; i++) {
        if (network_address_same(local_address, path[i])) return i;
    }
    
    return -1;
}

// evicts cache entry stored in arg
// Does not check for threads blocked on semaphore before deallocation
int
cache_evict(void *arg) {
    route_cache_entry_t route = (route_cache_entry_t) arg;
    
    semaphore_destroy(route->routing_sem);
    free(hashtable_remove(routing_cache, (route->destination)));
    free(route);
    
    return 0;
}

/*
 Called by network handler
 */
void
miniroute_helper(network_interrupt_arg_t *packet) {
    routing_header_t receivedheader, replyheader;
    char routing_type;
    network_address_t destination;
    unsigned int id;
    unsigned int ttl;
    unsigned int path_len;
    char junk;
    network_address_t path[MAX_ROUTE_LENGTH];
    network_address_t reversepath[MAX_ROUTE_LENGTH];
    
    route_cache_entry_t cache_entry;
    interrupt_level_t level;
    
    network_address_t replypath[MAX_ROUTE_LENGTH];
    memset(replypath, 0, MAX_ROUTE_LENGTH * 8);
    
    memset(reversepath, 0, MAX_ROUTE_LENGTH * 8);
    
    receivedheader = (routing_header_t) packet->buffer;
    routing_type = receivedheader->routing_packet_type;
    unpack_address(receivedheader->destination, destination);
    id = unpack_unsigned_int(receivedheader->id);
    ttl = unpack_unsigned_int(receivedheader->ttl);
    path_len = unpack_unsigned_int(receivedheader->path_len);
    unpack_path(receivedheader->path, path);
    
    if (routing_type == ROUTING_ROUTE_DISCOVERY) {
        ttl--;
        if (network_address_same(destination, local_address)) {
            // pack up and send reply
            path_len++;
            network_address_copy(local_address, path[path_len]);
            replyheader = (routing_header_t) malloc(sizeof(struct routing_header));
            replyheader->routing_packet_type = ROUTING_ROUTE_REPLY;
            // Destination is the message initiator
            pack_address(replyheader->destination, path[0]);
            pack_unsigned_int(replyheader->id, id);
            pack_unsigned_int(replyheader->ttl, MAX_ROUTE_LENGTH);
            pack_unsigned_int(replyheader->path_len, path_len);
            // Reverse path
            reverse_path(replypath, path, path_len);
            pack_path(replyheader->path, replypath);
            
            network_send_pkt(replypath[1], sizeof(struct routing_header), (char *)replyheader, 0, &junk);
            free(replyheader);
            free(packet);
            return;
        } else {
            if (ttl == 0 || ttl == -1) {
                free(packet);
                return;
                // ttl is 0, drop packet
            } else {
                // Loop detected, drop packet
                if (path_contains_self(path)) {
                    free(packet);
                    return;
                }
                // pack up and broadcast
                path_len++;
                network_address_copy(local_address, path[path_len]);
                replyheader = (routing_header_t) malloc(sizeof(struct routing_header));
                replyheader->routing_packet_type = ROUTING_ROUTE_DISCOVERY;
                pack_address(replyheader->destination, destination);
                pack_unsigned_int(replyheader->id, id);
                pack_unsigned_int(replyheader->ttl, ttl);
                pack_unsigned_int(replyheader->path_len, path_len);
                pack_path(replyheader->path, path);
                
                network_bcast_pkt(sizeof(struct routing_header), (char *)replyheader, 0, &junk);
                free(replyheader);
                free(packet);
                return;
            }
        }
    }
    
    if (routing_type == ROUTING_ROUTE_REPLY) {
        if (network_address_same(destination, local_address)) {
            level = set_interrupt_level(DISABLED);
            // check cache, wake up threads if necessary
            if (hashtable_get(routing_cache, path[0], &cache_entry)) {
                // cache entry not present, discard packet
                free(packet);
                set_interrupt_level(level);
                return;
            } else {
                // cache entry present
                // cache entry indicates this packet is not needed, simply return
                if (cache_entry->routing_flag || (cache_entry->retry_count > 2) || (cache_entry->routing_id != id)) {
                    free(packet);
                    set_interrupt_level(level);
                    return;
                }
                // update cache, wake up threads
                deregister_alarm(cache_entry->alarm_id);
                cache_entry->path_len = path_len;
                
                unpack_path(receivedheader->path, path);
                reverse_path(cache_entry->path, path, path_len);
                cache_entry->routing_flag = 1;
                cache_entry->alarm_id = register_alarm(3000, cache_evict, cache_entry);
                
                if (cache_entry->waiting_count > 0) {
                    semaphore_V(cache_entry->routing_sem);
                }
                free(packet);
                return;
            }
            set_interrupt_level(level);
        } else {
            // Continue forwarding
            ttl--;
            if (ttl <= 0) {
                free(packet);
                return;
            }
            replyheader = (routing_header_t) malloc(sizeof(struct routing_header));
            replyheader->routing_packet_type = routing_type;
            pack_address(replyheader->destination, destination);
            pack_unsigned_int(replyheader->id, id);
            pack_unsigned_int(replyheader->ttl, ttl);
            pack_unsigned_int(replyheader->path_len, path_len);
            pack_path(replyheader->path, path);
            
            network_send_pkt(path[find_self(path) + 1], sizeof(struct routing_header), (char *)replyheader, 0, &junk);
            
            free(replyheader);
            free(packet);
            return;
        }
    }
}

// only called if routing_type of packet is ROUTING_DATA
// returns 1 if packet was forwarded, 0 if packet is meant for local machine
int
forward_packet(network_interrupt_arg_t *packet) {
    routing_header_t receivedheader, replyheader;
    network_address_t destination;
    unsigned int id;
    unsigned int ttl;
    unsigned int path_len;
    network_address_t path[MAX_ROUTE_LENGTH];
    
    receivedheader = (routing_header_t) packet->buffer;
    
    // routing type is always ROUTING_DATA so don't worry about packet type
    unpack_address(receivedheader->destination, destination);
    
    if (network_address_same(destination, local_address)) return 0;
    
    id = unpack_unsigned_int(receivedheader->id);
    ttl = unpack_unsigned_int(receivedheader->ttl);
    path_len = unpack_unsigned_int(receivedheader->path_len);
    unpack_path(receivedheader->path, path);
    
    ttl--;
    replyheader = (routing_header_t) malloc(sizeof(struct routing_header));
    replyheader->routing_packet_type = ROUTING_DATA;
    pack_address(replyheader->destination, destination);
    pack_unsigned_int(replyheader->id, id);
    pack_unsigned_int(replyheader->ttl, ttl);
    pack_unsigned_int(replyheader->path_len, path_len);
    pack_path(replyheader->path, path);
    
    // forward packet to next person in path
    network_send_pkt(path[find_self(path) + 1], sizeof(struct routing_header),
                     (char *)replyheader, MAX_NETWORK_PKT_SIZE - sizeof(struct routing_header),
                     packet->buffer + sizeof(struct routing_header));
    free(replyheader);
    free(packet);
    return 1;
}

int
rebroadcast(void *arg) {
    route_cache_entry_t route = (route_cache_entry_t) arg;
    routing_header_t hdr;
    char junk;
    
    (route->routing_id)++;
    
    // max retries, set flag and allow all threads to wake up and fail
    if (route->retry_count > 2) {
        route->routing_flag = 2;
        
        if (route->waiting_count) {
            semaphore_V(route->routing_sem);
            return -1;
        } else {
            // no one is waiting for route, simply destroy the cache entry
            semaphore_destroy(route->routing_sem);
            free(hashtable_remove(routing_cache, (route->destination)));
            free(route);
        }
    }
    
    hdr = (routing_header_t) malloc(sizeof(struct routing_header));
    hdr->routing_packet_type = ROUTING_ROUTE_DISCOVERY;
    pack_address(hdr->destination, route->destination);
    // Is this right?
    pack_unsigned_int(hdr->id, route->routing_id);
    pack_unsigned_int(hdr->ttl, MAX_ROUTE_LENGTH);
    pack_unsigned_int(hdr->path_len, 0);
    pack_path(hdr->path, route->path);
    
    network_bcast_pkt(sizeof(struct routing_header), (char *)hdr, 0, &junk);
    // broadcast after timeout.
    route->alarm_id = register_alarm(15000, rebroadcast, route);
    
    (route->retry_count)++;
    
    return 0;
}

/*
 *  Returns route cache entry containing a valid route on success
 *  Returns NULL on failure
 */
route_cache_entry_t
retrieve_route(network_address_t dest_address) {
    route_cache_entry_t route;
    
    network_address_t *addr = malloc(sizeof(network_address_t));
    network_address_copy(dest_address, *addr);
    
    if (hashtable_get(routing_cache, *addr, &route)) {
        // item not present in hashtable so create a new entry
        route = (route_cache_entry_t) malloc(sizeof(struct route_cache_entry));
        network_address_copy(dest_address, route->destination);
        route->path_len = 0;
        memset(route->path, 0, MAX_ROUTE_LENGTH * 8);
        network_address_copy(local_address, route->path[0]);
        route->routing_sem = semaphore_create();
        semaphore_initialize(route->routing_sem, 0);
        route->waiting_count = 0;
        route->routing_flag = 0;
        route->alarm_id = -1;
        route->retry_count = 0;
        route->routing_id = 0;
        
        hashtable_put(routing_cache, addr, route);
        
        // now broadcast and sleep
        // further rebroadcasts are handled by alarm function
        rebroadcast(route);
        (route->waiting_count)++;
        semaphore_P(route->routing_sem);
        (route->waiting_count)--;
        if (route->waiting_count > 0) semaphore_V(route->routing_sem);
        
        if (route->routing_flag == 1) {
            // routing succeeded
            return route;
        } else {
            // routing has failed
            return NULL;
        }
    } else {
        // routing in progress, wait for a return value
        if (route->routing_flag == 0) {
            (route->waiting_count)++;
            semaphore_P(route->routing_sem);
            (route->waiting_count)--;
            if (route->waiting_count > 0) semaphore_V(route->routing_sem);
            
            if (route->routing_flag == 1) {
                // routing succeeded
                return route;
            } else {
                // routing has failed
                if (route->waiting_count == 0) {
                    semaphore_destroy(route->routing_sem);
                    free(hashtable_remove(routing_cache, (route->destination)));
                    free(route);
                }
                return NULL;
            }
        }
        // route is present
        if (route->routing_flag == 1) {
            return route;
        }
        // fail immediately
        if (route->routing_flag == 2) {
            // threads still blocked on routing, V on sema and return
            if (route->waiting_count > 0) {
                semaphore_V(route->routing_sem);
                return NULL;
            } else {
                // no threads blocked, destroy entry
                semaphore_destroy(route->routing_sem);
                free(hashtable_remove(routing_cache, (route->destination)));
                free(route);
                return NULL;
            }
        }
        
        return NULL;
    }
    return NULL;
}

/* sends a miniroute packet, automatically discovering the path if necessary. See description in the
 * .h file.
 */
int
miniroute_send_pkt(network_address_t dest_address, int hdr_len, char* hdr, int data_len, char* data) {
    route_cache_entry_t route;
    routing_header_t rhdr;
    int bytes_sent;
    int size;
    char nonroute_data[MAX_NETWORK_PKT_SIZE];
    network_address_t path[MAX_ROUTE_LENGTH];
    
    memcpy(nonroute_data, hdr, hdr_len);
    memcpy(nonroute_data + hdr_len, data, data_len);
    size = hdr_len + data_len;
    
    printf("Inside sendpkt\n");
    // send to self, don't check cache
    if (network_address_same(dest_address, local_address)) {
		printf("Sending to myself\n");
        rhdr = (routing_header_t) malloc(sizeof(struct routing_header));
        rhdr->routing_packet_type = ROUTING_DATA;
        pack_address(rhdr->destination, dest_address);
        pack_unsigned_int(rhdr->id, 0);
        pack_unsigned_int(rhdr->ttl, MAX_ROUTE_LENGTH);
        pack_unsigned_int(rhdr->path_len, 0);
        memset(path, 0, MAX_ROUTE_LENGTH * 8);
        network_address_copy(local_address, path[0]);
        pack_path(rhdr->path, path);
        
        bytes_sent = network_send_pkt(local_address, sizeof(struct routing_header), (char *)rhdr, size, nonroute_data);
        free(rhdr);
        return bytes_sent - sizeof(struct routing_header);
    }
    network_printaddr(dest_address);
	network_printaddr(local_address);
    route = retrieve_route(dest_address);
    
    if (route == NULL) {
        return -1;  //should return value be 0?
    }
    
    rhdr = (routing_header_t) malloc(sizeof(struct routing_header));
    rhdr->routing_packet_type = ROUTING_DATA;
    pack_address(rhdr->destination, dest_address);
    pack_unsigned_int(rhdr->id, 0);
    pack_unsigned_int(rhdr->ttl, MAX_ROUTE_LENGTH);
    pack_unsigned_int(rhdr->path_len, route->path_len);
    pack_path(rhdr->path, route->path);
    
    // send packet to first address in path
    bytes_sent = network_send_pkt(route->path[1], sizeof(struct routing_header), (char *)rhdr, size, nonroute_data);
    free(rhdr);
    return bytes_sent - sizeof(struct routing_header);
}

/* hashes a pointer to a network_address_t into a 16 bit unsigned int */
unsigned short
hash_address(network_address_t address) {
    unsigned int result = 0;
    int counter;
    
    for (counter = 0; counter < 3; counter++)
        result ^= ((unsigned short*) address)[counter];
    
    return result % 65521;
}