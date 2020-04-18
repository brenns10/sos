#pragma once

#include <stdint.h>

uint32_t ntohl(uint32_t orig);
uint16_t ntohs(uint16_t orig);
uint32_t htonl(uint32_t orig);
uint32_t htons(uint16_t orig);
int inet_aton(const char *cp, uint32_t *addr);
