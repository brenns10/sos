/**
 * PL011 Driver
 */
#include "kernel.h"

#define UART_INTID 33

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
#define UARTLCR_FEN (1<<4)
#define UARTLCR_8BIT (3<<5)
	uint32_t UARTCR;
#define UARTCR_UARTEN (1<<0)
#define UARTCR_TXE    (1<<8)
#define UARTCR_RXE    (1<<9)
	uint32_t UARTIFLS;
	uint32_t UARTIMSC;
#define UARTIMSC_UART_RXIM (1<<4)
	uint32_t UARTRIS;
	uint32_t UARTMIS;
	uint32_t UARTICR;
	uint32_t UARTDMACR;
} pl011_registers;

uint32_t uart_base = 0x09000000;
struct process *waiting = NULL;
#define base ((pl011_registers*) uart_base)

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

int getc_blocking(void)
{
	if (waiting)
		return -EBUSY;

	while (READ32(base->UARTFR) & UARTFR_RXFE) {
		current->flags.pr_ready = 0;
		waiting = current;
		block(current->context);
	}
	return READ32(base->UARTDR) & 0xFF;
}

void uart_isr(uint32_t intid)
{
	uint32_t reg;
	int result;

	reg = base->UARTMIS;
	if (reg != UARTIMSC_UART_RXIM) {
		puts("BAD UART INTERRUPT\n");
	} else if (waiting) {
		/* get the getc return value and mark process for return */
		waiting->flags.pr_ready = 1;
		waiting = NULL;
	}

	/* clear the interrupt */
	reg = UARTIMSC_UART_RXIM;
	WRITE32(base->UARTICR, reg);
	gic_end_interrupt(intid);
}

/**
 * Initialize the UART.
 *
 * The UART starts up pretty well initialized -- the getc(), putc(), and puts()
 * functions work more or less without any initialization in qemu. However, this
 * initialization is necessary for enabling interrupts from the UART, so we
 * aren't constantly busy-waiting.
 */
void uart_init(void)
{
	uint32_t reg;

	/* Half full TX/RX interrupt */
	reg = 0x12;
	WRITE32(base->UARTIFLS, reg);
	/* Set 8 bit words, and enable FIFO */
	reg = UARTLCR_FEN | UARTLCR_8BIT;
	WRITE32(base->UARTLCR_H, reg);
	/* Enable UART, Tx, Rx */
	reg = base->UARTCR;
	reg |= (UARTCR_UARTEN | UARTCR_TXE | UARTCR_RXE);
	WRITE32(base->UARTCR, reg);
	/* Only interrupt for RX */
	reg = UARTIMSC_UART_RXIM;
	WRITE32(base->UARTIMSC, reg);

	gic_register_isr(UART_INTID, 1, uart_isr);
	gic_enable_interrupt(UART_INTID);
}
