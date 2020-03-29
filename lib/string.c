/*
 * String utilities - very inefficient ones at that
 */
#include "string.h"

/*
 * TODO: please for the love of god write an optimized strcpy / memcpy :(
 */

uint32_t strlen(const char *string)
{
	uint32_t len;
	for (len = 0; string[len]; len++){};
	return len;
}

int strcmp(const char *lhs, const char *rhs)
{
	uint32_t i;
	for (i = 0; lhs[i] == rhs[i]; i++)
		if (lhs[i] == '\0')
			return 0;
	return lhs[i] - rhs[i];
}

bool strprefix(const char *haystack, const char *prefix)
{
	uint32_t i;
	for (i = 0; haystack[i] == prefix[i]; i++)
		if (haystack[i] == '\0')
			return 0; /* strings are equal! */
	if (prefix[i] == '\0')
		return 0; /* haystack is longer but that's ok */
	return 1;
}

bool strsuffix(const char *haystack, const char *suffix)
{
	uint32_t haylen = strlen(haystack);
	uint32_t suflen = strlen(suffix);
	if (haylen < suflen)
		return false;
	return strcmp(haystack + (haylen - suflen), suffix) == 0;
}

unsigned int strlcpy(char *dst, const char *src, unsigned int size)
{
	unsigned int i;

	/* safe path; avoid nul-termination if size is 0 */
	if (size == 0)
		return 0;

	for (i = 0; i < size - 1 && src[i]; i++) {
		dst[i] = src[i];
	}
	dst[i] = '\0';
	return i;
}

/* wow this is bad */
void *memcpy(void *dest, void *src, size_t n)
{
	size_t i;
	char *destc, *srcc;
	destc = dest;
	srcc = src;

	for (i = 0; i < n; i++)
		destc[i] = srcc[i];

	return dest;
}

void *memset(void *dest, int c, size_t n)
{
	uint8_t *destc = (uint8_t *)dest;
	size_t i;
	for (i = 0; i < n; i++)
		destc[i] = (uint8_t)c;
	return dest;
}

unsigned int strlcat(char *dst, const char *src, unsigned int size)
{
	unsigned int len = strlen(dst);
	return strlcpy(&dst[len], src, size - len);
}

int buf_append(struct buffer *buf, const char *str)
{
	/* basically strlcat, but we already know len & size */
	unsigned int len_src = strlen(str);
	unsigned int copied = strlcpy(&buf->buf[buf->len], str, buf->cap - buf->len);
	if (copied != len_src)
		return 1;
	else
		return 0;
}

int buf_trim(struct buffer *buf, unsigned int newlen)
{
	if (newlen >= buf->cap)
		return 1;

	buf->len = newlen;
	buf->buf[newlen] = '\0';
}
