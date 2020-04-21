#pragma once

#include <stdint.h>

#include "kernel.h"
#include "list.h"
#include "net.h"
#include "packets.h"
#include "sys/socket.h"
#include "wait.h"

struct socket;

struct sockops {
	int (*connect)(struct socket *socket, const struct sockaddr *address,
	               socklen_t address_len);
	int (*bind)(struct socket *socket, const struct sockaddr *address,
	            socklen_t address_len);
	int (*send)(struct socket *socket, const void *buffer, size_t length,
	            int flags);
	int (*recv)(struct socket *socket, void *buffer, size_t length,
	            int flags);
	int (*close)(struct socket *socket);

	struct list_head list;
	uint32_t proto;
};

struct socket {
	int fildes;
	struct process *proc;
	struct list_head sockets;
	struct sockops *ops;
	struct {
		int sk_bound : 1;
		int sk_connected : 1;
		int sk_open : 1;
	} flags;
	struct sockaddr_in src;
	struct sockaddr_in dst;
	struct list_head recvq;
	struct waitlist recvwait;
};

int socket_socket(int domain, int type, int protocol);
void socket_register_proto(struct sockops *ops);
void socket_destroy(struct socket *sock);
struct socket *socket_get_by_fd(struct process *proc, int fd);
void socket_init(void);
