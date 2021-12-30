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
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef TEST_PREFIX
#define vsnprintf test_vsnprintf
#define snprintf  test_snprintf
#define printf    test_printf
#define atoi      test_atoi
#define puts      test_puts
#endif

/* Your puts() should fit this signature (roughly) */
extern void puts(char *string);

static inline void set(char *buf, uint32_t size, uint32_t *out, char val)
{
	if (*out < size - 1)
		buf[*out] = val;
	else if (*out == size - 1)
		buf[*out] = '\0';
	*out = *out + 1;
}

/**
 * This macro assigns a value to the buffer at the given index. It increments
 * the index after assigning the value. However, it takes care of bounds checks
 * so that we don't have to constantly think about them. It also "automatically"
 * nul-terminates the string if we hit the end of the buffer.
 */
#define SET(buf, size, out, val) set(buf, size, &out, val)

/**
 * This function implements the %x format specifier.
 */
static inline uint32_t _format_hex(char *buf, uint32_t size, uint32_t out,
                                   uint64_t val, bool lz, bool big)
{
	uint64_t mask;
	uint32_t shift = 32;
	uint64_t digit;
	char c;

	if (big)
		shift = 64;

	do {
		shift -= 4;
		mask = 0xFULL << shift;
		digit = (val & mask) >> shift;

		if (digit || lz || shift == 0) {
			lz = true;
			c = (digit >= 10 ? 'a' + digit - 10 : '0' + digit);
			SET(buf, size, out, c);
		}
	} while (shift > 0);
	return out;
}

static inline uint32_t _format_mac(char *buf, uint32_t size, uint32_t out,
                                   uint8_t *macptr)
{
	int i;
	for (i = 0; i < 6; i++) {
		if (i > 0)
			SET(buf, size, out, ':');
		out = _format_hex(buf, size, out, (uint32_t)macptr[i], false, false);
	}
	return out;
}

/**
 * Implements the %u and %d format specifiers.
 */
static inline uint32_t _format_int(char *buf, uint32_t size, uint32_t out,
                                   uint32_t val, bool is_signed)
{
	uint8_t tmp[10]; // max base 10 digits for 32-bit int
	uint32_t tmpIdx = 0, rem;

	if (is_signed && (val & 0x80000000)) {
		val = ~(val) + 1;
		SET(buf, size, out, '-');
	}

	do {
		rem = val % 10; // should do uidivmod, only one call
		val = val / 10;
		tmp[tmpIdx++] = rem;
	} while (val);
	do {
		tmpIdx--;
		SET(buf, size, out, '0' + tmp[tmpIdx]);
	} while (tmpIdx > 0);
	return out;
}

/**
 * Implements the %I format specifier - IPv4 address, in network byte order.
 *
 */
static inline uint32_t _format_ipv4(char *buf, uint32_t size, uint32_t out,
                                    uint32_t val)
{
	out = _format_int(buf, size, out, (val >> 0) & 0xFF, false);
	SET(buf, size, out, '.');
	out = _format_int(buf, size, out, (val >> 8) & 0xFF, false);
	SET(buf, size, out, '.');
	out = _format_int(buf, size, out, (val >> 16) & 0xFF, false);
	SET(buf, size, out, '.');
	out = _format_int(buf, size, out, (val >> 24) & 0xFF, false);
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
	uint64_t u64val;
	char *strval;
	uint8_t *macptr;
	char charval;

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
			case 'X':
				u64val = va_arg(vl, uint64_t);
				out = _format_hex(buf, size, out, u64val, false, true);
				break;
			case 'x':
				uintval = va_arg(vl, uint32_t);
				out = _format_hex(buf, size, out, (uint64_t)uintval, false, false);
				break;
			case '0':
				if (format[in+1] == 'x') {
					uintval = va_arg(vl, uint32_t);
					out = _format_hex(buf, size, out, uintval, true, false);
					in++;
				} else if (format[in+1] == 'X') {
					u64val = va_arg(vl, uint64_t);
					out = _format_hex(buf, size, out, u64val, true, true);
					in++;
				} else {
					SET(buf, size, out, '%');
					SET(buf, size, out, '0');
					in++;
				}
				break;
			case 's':
				strval = va_arg(vl, char *);
				out = _format_str(buf, size, out, strval);
				break;
			case 'u':
			case 'd':
				uintval = va_arg(vl, uint32_t);
				out = _format_int(buf, size, out, uintval,
				                  format[in] == 'd');
				break;
			case 'I':
				uintval = va_arg(vl, uint32_t);
				out = _format_ipv4(buf, size, out, uintval);
				break;
			case 'M':
				macptr = va_arg(vl, uint8_t *);
				out = _format_mac(buf, size, out, macptr);
				break;
			case 'c':
				charval = (char)(0xFF & va_arg(vl, int));
				SET(buf, size, out, charval);
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
	int i = 0, val = 0;
	bool negate = false;
	if (str[i] == '-') {
		negate = true;
		i++;
	}
	for (; str[i]; i++)
		val = val * 10 + (str[i] - '0');
	return negate ? -val : val;
}

#ifdef TEST_PREFIX
#undef vsnprintf
#undef snprintf
#undef printf
#undef atoi
#undef puts
#endif
