/*
 * Simple userspace formatting library.
 */
#pragma once

#include <stdarg.h>
#include <stdint.h>

uint32_t vsnprintf(char *buf, uint32_t size, const char *format, va_list vl);
uint32_t snprintf(char *buf, uint32_t size, const char *format, ...);
uint32_t printf(const char *format, ...);

int atoi(const char *str);
