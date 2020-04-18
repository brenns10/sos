/*
 * System call implementations.
 *
 * These aren't declared in a header since they are only linked against by
 * entry.s. They shouldn't be called by external code anyway.
 */
#include "syscall.h"
#include "kernel.h"
#include "socket.h"

void sys_relinquish(void)
{
	puts("[kernel] tRelinquish()\n");
	block(current->context);
}

void sys_display(char *buffer)
{
	puts(buffer);
}

void sys_exit(uint32_t code)
{
	printf("[kernel] Process %u exited with code %u.\n", current->id, code);
	destroy_current_process();
	/* never returns */
}

int sys_getchar(void)
{
	return getc_blocking();
}

int sys_runproc(char *imagename, int flags)
{
	int32_t img = process_image_lookup(imagename);
	struct process *proc;
	if (img < 0)
		return -1;
	proc = create_process(img);

	if (flags & RUNPROC_F_WAIT) {
		wait_for(&proc->endlist);
	}

	return 0;
}

int sys_getpid(void)
{
	return current->id;
}

int sys_socket(int domain, int type, int protocol)
{
	return socket_socket(domain, type, protocol);
}

int sys_bind(int sockfd, const struct sockaddr *address, socklen_t address_len)
{
	struct socket *sk = socket_get_by_fd(current, sockfd);
	if (!sk)
		return -EBADF;

	if (!sk->ops->bind)
		return -EOPNOTSUPP;

	return sk->ops->bind(sk, address, address_len);
}

int sys_connect(int sockfd, const struct sockaddr *address,
                socklen_t address_len)
{
	struct socket *sk = socket_get_by_fd(current, sockfd);
	if (!sk)
		return -EBADF;

	if (!sk->ops->connect)
		return -EOPNOTSUPP;

	return sk->ops->connect(sk, address, address_len);
}

int sys_send(int sockfd, const void *buffer, size_t length, int flags)
{
	struct socket *sk = socket_get_by_fd(current, sockfd);
	if (!sk)
		return -EBADF;

	if (!sk->ops->send)
		return -EOPNOTSUPP;

	return sk->ops->send(sk, buffer, length, flags);
}

void sys_unknown(uint32_t svc_num)
{
	printf("ERROR: unknown syscall number %u!\n", svc_num);
}
