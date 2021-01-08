/**
 * PL011 Driver
 */
#include "config.h"
#include "kernel.h"
#include "ldisc.h"
#include "sync.h"

#define UART_INTID 33

typedef volatile struct __attribute__((packed)) {
	uint32_t UARTDR;
#define UARTDR_OE    (1 << 11)
#define UARTDR_BE    (1 << 10)
#define UARTDR_PE    (1 << 9)
#define UARTDR_FE    (1 << 8)
#define UARTDR_FLAGS 0xF00
#define UARTDR_DATA  0xFF
	uint32_t UARTRSR;
	uint32_t _reserved0[4];
	const uint32_t UARTFR;
#define UARTFR_RI   (1 << 8)
#define UARTFR_TXFE (1 << 7)
#define UARTFR_RXFF (1 << 6)
#define UARTFR_TXFF (1 << 5)
#define UARTFR_RXFE (1 << 4)
#define UARTFR_BUSY (1 << 3)
#define UARTFR_DCD  (1 << 2)
#define UARTFR_DSR  (1 << 1)
#define UARTFR_CTS  (1 << 0)
	uint32_t _reserved;
	uint32_t UARTILPR;
	uint32_t UARTIBRD;
	uint32_t UARTFBRD;
	uint32_t UARTLCR_H;
#define UARTLCR_FEN  (1 << 4)
#define UARTLCR_8BIT (3 << 5)
	uint32_t UARTCR;
#define UARTCR_UARTEN (1 << 0)
#define UARTCR_TXE    (1 << 8)
#define UARTCR_RXE    (1 << 9)
	uint32_t UARTIFLS;
	uint32_t UARTIMSC;
#define UARTIMSC_UART_RXIM (1 << 4)
	uint32_t UARTRIS;
	uint32_t UARTMIS;
	uint32_t UARTICR;
	uint32_t UARTDMACR;
} pl011_registers;

uint32_t uart_base = CONFIG_UART_BASE;
DECLARE_SPINSEM(uart_sem, 1);
#define base ((pl011_registers *)uart_base)
struct ldisc_line_edit uart_lle = { 0 };
struct file *uart_file = NULL;

void putc(char c)
{
	if (c == '\n')
		putc('\r');
	while (base->UARTFR & UARTFR_TXFF) {
	}
	WRITE32(base->UARTDR, c);
}

void puts(char *string)
{
	preempt_disable();
	while (*string)
		putc(*(string++));
	preempt_enable();
}

void nputs(char *string, int n)
{
	int i;
	preempt_disable();
	for (i = 0; i < n; i++)
		putc(string[i]);
	preempt_enable();
}

int try_getc(void)
{
	if (READ32(base->UARTFR) & UARTFR_RXFE)
		return -1;
	return READ32(base->UARTDR) & UARTDR_DATA;
}

int getc_spinning(void)
{
	int rv;
	do {
		rv = try_getc();
	} while (rv == -1);
	return rv;
}

int getc_blocking(void)
{
	int rv;
	char c;

	rv = uart_file->ops->read(uart_file, &c, 1);
	if (rv == 1) {
		return (int)c;
	} else {
		return rv;
	}
}

void uart_isr(uint32_t intid, struct ctx *ctx)
{
	uint32_t reg;

	reg = READ32(base->UARTMIS);
	if (reg != UARTIMSC_UART_RXIM) {
		puts("BAD UART INTERRUPT\n");
		goto out;
	}

	reg = READ32(base->UARTDR);
	if (reg & UARTDR_BE) {
		panic(ctx);
		goto out;
	} else if (reg & UARTDR_FLAGS) {
		printf("uart: got error in rx, UARTDR=0x%x\n", reg);
		goto out;
	}

	/* Deliver to the line discipline */
	lle_char(&uart_lle, reg & UARTDR_DATA);

	/* Deliver any additional characters */
	while (!(READ32(base->UARTFR) & UARTFR_RXFE)) {
		reg = READ32(base->UARTDR);
		lle_char(&uart_lle, reg & UARTDR_DATA);
	}

out:
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

	/* Set 8 bit words, and enable FIFO */
	WRITE32(base->UARTLCR_H, UARTLCR_FEN | UARTLCR_8BIT);

	/* Enable UART, Tx, Rx */
	reg = READ32(base->UARTCR);
	reg |= (UARTCR_UARTEN | UARTCR_TXE | UARTCR_RXE);
	WRITE32(base->UARTCR, reg);
}

void uart_init_irq(void)
{
	/* Initialize the UART file */
	uart_lle.dest = flip_file_new();
	uart_lle.fb = flip_buffer_new();
	uart_lle.state = LS_NORMAL;
	uart_file = uart_lle.dest;

	/* Half full TX/RX interrupt */
	WRITE32(base->UARTIFLS, 0x12);

	/* Only interrupt for RX */
	WRITE32(base->UARTIMSC, UARTIMSC_UART_RXIM);

	gic_register_isr(UART_INTID, 1, uart_isr, "uart");
	gic_enable_interrupt(UART_INTID);
}

/*
 * Remap the UART base. This should be called after MMU is enabled, and we need
 * to map a virtual address to the physical UART base.
 */
void uart_remap(void)
{
	uart_base = kmem_remap_periph(uart_base);
}
