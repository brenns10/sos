#pragma once

#include <stdint.h>

typedef uint32_t sa_family_t;
typedef uint32_t socklen_t;
typedef uint16_t in_port_t;

enum {
	/* noformat */
	EAFNOSUPPORT = 1,
	EPROTONOSUPPORT,
	EPROTOTYPE,
};

enum {
	/* noformat */
	AF_INET = 1,
};

enum {
	/* noformat */
	SOCK_STREAM = 1,
	SOCK_DGRAM,
};

#define IPPROTO_UDP 17

struct in_addr {
	uint32_t s_addr;
};

struct sockaddr_in {
	sa_family_t sin_family;
	in_port_t sin_port;
	struct in_addr sin_addr;
};

struct sockaddr {
	sa_family_t s_family;
};
