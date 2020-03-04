/**
 * PL011 Driver
 */
#include "kernel.h"

typedef volatile struct __attribute__((packed)) {
	uint32_t UARTDR;
	uint32_t UARTRSR;
	uint32_t _reserved0[4];
	const uint32_t UARTFR;
#define UARTFR_RI   (1<<8)
#define UARTFR_TXFE (1<<7)
#define UARTFR_RXFF (1<<6)
#define UARTFR_TXFF (1<<5)
#define UARTFR_RXFE (1<<4)
#define UARTFR_BUSY (1<<3)
#define UARTFR_DCD  (1<<2)
#define UARTFR_DSR  (1<<1)
#define UARTFR_CTS  (1<<0)
	uint32_t _reserved;
	uint32_t UARTILPR;
	uint32_t UARTIBRD;
	uint32_t UARTFBRD;
	uint32_t UARTLCR_H;
	uint32_t UARTCR;
	uint32_t UARTIFLS;
	uint32_t UARTIMSC;
	uint32_t UARTRIS;
	uint32_t UARTMIS;
	uint32_t UARTICR;
	uint32_t UARTDMACR;
} pl011_registers;

uint32_t uart_base = 0x09000000;
#define base ((pl011_registers*) uart_base)
#define WRITE32(_reg, _val) (*(volatile uint32_t *)&_reg = _val)

void putc(char c)
{
	while (base->UARTFR & UARTFR_TXFF) {}
	WRITE32(base->UARTDR, c);
}

void puts(char *string)
{
	while (*string)
		putc(*(string++));
}

char getc(void)
{
	while (base->UARTFR & UARTFR_RXFE) {}
	return (char) base->UARTDR;
}
