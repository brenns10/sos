/**
 * format.c: simple, stripped-down sprintf (and thus printf) implementation
 *
 * Only dependency is a puts() implementation, for printf(). Format specifier
 * support is quite simplistic. No memory allocation, but beware! we use the
 * stack to store our output buffer of size 1024 bytes. Depending on your stack
 * size requirements, you may want to tweak that.
 *
 * Stephen Brennan
 */
#include <stdint.h>
#include <stdarg.h>

/* Your puts() should fit this signature (roughly) */
extern void puts(char *string);

/**
 * This macro assigns a value to the buffer at the given index. It increments
 * the index after assigning the value. However, it takes care of bounds checks
 * so that we don't have to constantly think about them. It also "automatically"
 * nul-terminates the string if we hit the end of the buffer.
 */
#define SET(buf, size, out, value)                                      \
	do {                                                            \
		if (out < size - 1)                                     \
			buf[out] = value;                               \
		else if (out == size - 1)                               \
			buf[out] = '\0';                                \
		out++;                                                  \
	} while (0);

/**
 * This function implements the %x format specifier.
 */
static inline uint32_t _format_hex(char *buf, uint32_t size, uint32_t out,
                                   uint32_t val)
{
	uint32_t mask = 0xF0000000;
	uint32_t shift = 32;
	uint32_t digit;
	uint32_t started = 0;
	char c;

	do {
		shift -= 4;
		digit = (val & mask) >> shift;

		// If statement ensures we skip leading zeros.
		// I'm still debating if this is useful.
		if (digit || started || shift == 0) {
			started = 1;
			c = (digit >= 10 ? 'a' + digit - 10 : '0' + digit);
			SET(buf, size, out, c);
		}
		mask >>= 4;
	} while (shift > 0);
	return out;
}

/**
 * Implements the %u format specifier.
 */
static inline uint32_t _format_uint(char *buf, uint32_t size, uint32_t out,
                                    uint32_t val)
{
	uint8_t tmp[10]; // max base 10 digits for 32-bit int
	uint32_t tmpIdx = 0, rem;
	do {
		rem = val % 10; // should do uidivmod, only one call
		val = val / 10;
		tmp[tmpIdx++] = rem;
	} while(val);
	do {
		tmpIdx--;
		SET(buf, size, out, '0' + tmp[tmpIdx]);
	} while (tmpIdx > 0);
	return out;
}

/**
 * Implements the %s format specifier.
 */
static inline uint32_t _format_str(char *buf, uint32_t size, uint32_t out,
                                   char *val)
{
	for (; *val; val++) {
		SET(buf, size, out, *val);
	}
	return out;
}

/**
 * This is the fundamental formatting function, although it is not the one users
 * will call frequently. The v means that it takes a va_list directly, which is
 * useful for sharing code across variadic functions. The s means that it will
 * write to a buffer. The n means that it will not write past the given buffer
 * size.
 *
 * Supports a minimal subset of standard C format language. Format specifiers
 * may only be a single character: field widths, padding bytes, etc, may not be
 * specified in this implementation. Available format specifiers: %x, %s
 *
 * @buf: Where to write the output
 * @size: Size of the output buffer
 * @format: Format string
 * @vl: Argument list
 * @return: number of bytes written
 */
uint32_t vsnprintf(char *buf, uint32_t size, const char *format, va_list vl)
{
	uint32_t out = 0;
	uint32_t uintval;
	char *strval;

	for (uint16_t in = 0; format[in]; in++) {
		if (format[in] == '%') {
			in++;

			// when string ends with %, copy it literally
			if (!format[in]) {
				SET(buf, size, out, '%');
				goto nul_ret;
			}

			// otherwise, handle format specifiers
			switch (format[in]) {
			case 'x':
				uintval = va_arg(vl, uint32_t);
				out = _format_hex(buf, size, out, uintval);
				break;
			case 's':
				strval = va_arg(vl, char *);
				out = _format_str(buf, size, out, strval);
				break;
			case 'u':
				uintval = va_arg(vl, uint32_t);
				out = _format_uint(buf, size, out, uintval);
				break;
			case '%':
				SET(buf, size, out, '%');
				break;
			default:
				// default is to copy the unrecognized specifier
				// that may not be a great idea...
				SET(buf, size, out, '%');
				SET(buf, size, out, format[in]);
			}

		} else {
			SET(buf, size, out, format[in]);
		}
	}
nul_ret:
	SET(buf, size, out, '\0');
	return out - 1; // final count does not include nul-terminator
}

/**
 * Format a string into a buffer, without exceeding its size. See vsnprintf()
 * for details on formatting.
 * @buf: Where to write the output
 * @size: Size of the output buffer
 * @format: Format string
 * @return: Number of bytes written
 */
uint32_t snprintf(char *buf, uint32_t size, const char *format, ...)
{
	uint32_t res;
	va_list vl;
	va_start(vl, format);
	res = vsnprintf(buf, size, format, vl);
	va_end(vl);
	return res;
}

/**
 * Format a string and print it to the console. See vsnprintf() for details on
 * formatting.
 *
 * NOTE: There is a fixed buffer size (see below). Make sure your messages will
 * fit into it. Also, the buffer is stack-allocated, so we need to be careful
 * with the size, or we may start running into the TAGS section.
 *
 * @format: Format string
 * @return: Number of bytes written
 */
uint32_t printf(const char *format, ...)
{
	char buf[1024];
	uint32_t res;
	va_list vl;
	va_start(vl, format);
	res = vsnprintf(buf, sizeof(buf), format, vl);
	va_end(vl);
	puts(buf);
	return res;
}

int atoi(const char *str)
{
	int i, val = 0;
	for (i = 0; str[i]; i++)
		val = val * 10 + (str[i] - '0');
	return val;
}
