#include "socket.h"
#include "kernel.h"
#include "slab.h"
#include "string.h"

struct slab *socket_slab;

DECLARE_LIST_HEAD(sockops_list);

static struct sockops *lookup_proto(int protocol)
{
	struct sockops *ops;
	list_for_each_entry(ops, &sockops_list, list, struct sockops)
	{
		if (ops->proto == protocol)
			return ops;
	}
	return NULL;
}

int socket_socket(int domain, int type, int protocol)
{
	struct socket *sock;
	struct sockops *ops;

	if (domain != AF_INET)
		return -EAFNOSUPPORT;

	if (type != SOCK_DGRAM)
		return -EPROTOTYPE;

	if (!protocol)
		protocol = IPPROTO_UDP;

	ops = lookup_proto(protocol);
	if (!ops)
		return -EPROTONOSUPPORT;

	sock = slab_alloc(socket_slab);
	memset(sock, 0, sizeof(struct socket));
	current->max_fildes++;
	sock->fildes = current->max_fildes;
	sock->proc = current;
	sock->ops = ops;
	list_insert_end(&current->sockets, &sock->sockets);
	return sock->fildes;
}

void socket_register_proto(struct sockops *ops)
{
	list_insert_end(&sockops_list, &ops->list);
}

void socket_destroy(struct socket *sock)
{
	slab_free(socket_slab, sock);
}

struct socket *socket_get_by_fd(struct process *proc, int fd)
{
	struct socket *sk;
	list_for_each_entry(sk, &proc->sockets, sockets, struct socket)
	{
		if (sk->fildes == fd)
			return sk;
	}
	return NULL;
}

void socket_init(void)
{
	socket_slab = slab_new("socket", sizeof(struct socket), kmem_get_page);
}
