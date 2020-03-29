/*
 * String library
 */
#ifndef SOS_STRING_H
#define SOS_STRING_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

uint32_t strlen(const char *string);

bool strprefix(const char *haystack, const char *prefix);
bool strsuffix(const char *haystack, const char *suffix);

int strcmp(const char *lhs, const char *rhs);

void *memcpy(void *dest, void *src, size_t n);
void *memset(void *dest, int c, size_t n);

struct buffer {
	unsigned int cap;
	unsigned int len;
	char buf[0];
};

unsigned int strlcpy(char *dst, const char *src, unsigned int size);
unsigned int strlcat(char *dst, const char *src, unsigned int size);

/*
 * Append a string onto a buffer.
 * Return: 0 on success, non-zero on error
 * RV 1: not enough capacity
 */
int buf_append(struct buffer *buf, const char *str);

/*
 * Trim a buffer to a specific length. This simply updates len, and
 * null-terminates if necessary.
 * Return: 0 on success, non-zero on error
 * RV 1: out of bounds
 */
int buf_trim(struct buffer *buf, unsigned int newlen);

#endif
