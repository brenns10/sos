/*
 * Simple userspace formatting library.
 */
#pragma once

#include <stdarg.h>
#include <stdint.h>

#ifdef TEST_PREFIX
#define vsnprintf test_vsnprintf
#define snprintf  test_snprintf
#define printf    test_printf
#define atoi      test_atoi
#endif

uint32_t vsnprintf(char *buf, uint32_t size, const char *format, va_list vl);
uint32_t snprintf(char *buf, uint32_t size, const char *format, ...);
uint32_t printf(const char *format, ...);

int atoi(const char *str);

#ifdef TEST_PREFIX
#undef vsnprintf
#undef snprintf
#undef printf
#undef atoi
#endif
