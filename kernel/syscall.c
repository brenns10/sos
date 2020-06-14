/*
 * System call implementations.
 *
 * These aren't declared in a header since they are only linked against by
 * entry.s. They shouldn't be called by external code anyway.
 */
#include "cxtk.h"
#include "kernel.h"
#include "socket.h"

void sys_relinquish(void)
{
	cxtk_track_syscall();
	puts("[kernel] tRelinquish()\n");
	block(current->context);
	cxtk_track_syscall_return();
}

void sys_display(char *buffer)
{
	cxtk_track_syscall();
	puts(buffer);
	cxtk_track_syscall_return();
}

void sys_exit(uint32_t code)
{
	cxtk_track_syscall();
	printf("[kernel] Process %u exited with code %u.\n", current->id, code);
	destroy_current_process();
	/* never returns */
}

int sys_getchar(void)
{
	int rv;
	cxtk_track_syscall();
	rv = getc_blocking();
	cxtk_track_syscall_return();
	return rv;
}

#define RUNPROC_F_WAIT 1
int sys_runproc(char *imagename, int flags)
{
	int32_t img;
	struct process *proc;
	cxtk_track_syscall();

	img = process_image_lookup(imagename);
	if (img < 0) {
		cxtk_track_syscall_return();
		return -1;
	}
	proc = create_process(img);

	if (flags & RUNPROC_F_WAIT) {
		wait_for(&proc->endlist);
	}

	return 0;
}

int sys_getpid(void)
{
	cxtk_track_syscall();
	cxtk_track_syscall_return();
	return current->id;
}

int sys_socket(int domain, int type, int protocol)
{
	int rv;
	cxtk_track_syscall();
	rv = socket_socket(domain, type, protocol);
	cxtk_track_syscall_return();
	return rv;
}

int sys_bind(int sockfd, const struct sockaddr *address, socklen_t address_len)
{
	struct socket *sk;
	int rv;
	cxtk_track_syscall();

	sk = socket_get_by_fd(current, sockfd);
	if (!sk) {
		rv = -EBADF;
		goto out;
	}

	if (!sk->ops->bind) {
		rv = -EOPNOTSUPP;
		goto out;
	}

	rv = sk->ops->bind(sk, address, address_len);
out:
	cxtk_track_syscall_return();
	return rv;
}

int sys_connect(int sockfd, const struct sockaddr *address,
                socklen_t address_len)
{
	int rv;
	struct socket *sk;
	cxtk_track_syscall();

	sk = socket_get_by_fd(current, sockfd);
	if (!sk) {
		rv = -EBADF;
		goto out;
	}

	if (!sk->ops->connect) {
		rv = -EOPNOTSUPP;
		goto out;
	}

	rv = sk->ops->connect(sk, address, address_len);
out:
	cxtk_track_syscall_return();
	return rv;
}

int sys_send(int sockfd, const void *buffer, size_t length, int flags)
{
	int rv;
	struct socket *sk;
	cxtk_track_syscall();

	sk = socket_get_by_fd(current, sockfd);
	if (!sk) {
		rv = -EBADF;
		goto out;
	}

	if (!sk->ops->send) {
		rv = -EOPNOTSUPP;
		goto out;
	}

	rv = sk->ops->send(sk, buffer, length, flags);
out:
	cxtk_track_syscall_return();
	return rv;
}

int sys_recv(int sockfd, void *buffer, size_t length, int flags)
{
	int rv;
	struct socket *sk;
	cxtk_track_syscall();

	sk = socket_get_by_fd(current, sockfd);
	if (!sk) {
		rv = -EBADF;
		goto out;
	}

	if (!sk->ops->recv) {
		rv = -EOPNOTSUPP;
		goto out;
	}

	rv = sk->ops->recv(sk, buffer, length, flags);
out:
	cxtk_track_syscall_return();
	return rv;
}

void sys_unknown(uint32_t svc_num)
{
	cxtk_track_syscall();
	printf("ERROR: unknown syscall number %u!\n", svc_num);
	cxtk_track_syscall_return();
}
