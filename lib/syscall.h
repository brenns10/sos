/*
 * syscall.h: user-space header file containing system call declarations
 */
#pragma once

#include <stddef.h>

#include "sys/socket.h"

/* macro quoting utilities */
#define syscall_h_quote(blah)            #blah
#define syscall_h_expand_and_quote(blah) syscall_h_quote(blah)

/**
 * Generic system call, no args.
 */
#define sys0(sys_n)                                                            \
	__asm__ __volatile__("svc #" syscall_h_expand_and_quote(sys_n)         \
	                     : /* output operands */                           \
	                     : /* input operands */                            \
	                     : /* clobbers */ "a1", "a2", "a3", "a4")

/**
 * Generic system call, one arg.
 */
#define sys1(sys_n, arg1)                                                      \
	__asm__ __volatile__("mov a1, %[a1]\n"                                 \
	                     "svc #" syscall_h_expand_and_quote(sys_n)         \
	                     : /* output operands */                           \
	                     : /* input operands */[ a1 ] "r"(arg1)            \
	                     : /* clobbers */ "a1", "a2", "a3", "a4")

/*
 * System call numbers
 */
#define SYS_RELINQUISH 0
#define SYS_DISPLAY    1
#define SYS_EXIT       2
#define SYS_GETCHAR    3
#define SYS_RUNPROC    4
#define SYS_GETPID     5
#define SYS_SOCKET     6
#define SYS_BIND       7
#define SYS_CONNECT    8
#define SYS_SEND       9
#define MAX_SYS        9

/*
 * System call syntax sugars
 */
#define display(string) sys1(SYS_DISPLAY, string)
#define relinquish()    sys0(SYS_RELINQUISH)
#define exit(code)      sys1(SYS_EXIT, code)

#define RUNPROC_F_WAIT 1

int getchar(void);
int runproc(char *imagename, int flags);
int getpid(void);
int socket(int domain, int type, int protocol);
int bind(int sockfd, const struct sockaddr *address, socklen_t address_len);
int send(int sockfd, const void *buffer, size_t length, int flags);

/*
 * Declare a puts() which wraps the display() system call, necessary for printf
 * to link to something
 */
void puts(char *string);
void putc(char val);

#define nelem(x) (sizeof(x) / sizeof(x[0]))
