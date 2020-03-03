/*
 * Timer-related functionality
 */
#include "gic.h"
#include "kernel.h"

#define GET_CNTFRQ(dst) get_cpreg(dst, c14, 0, c0, 0)
#define SET_CNTFRQ(dst) set_cpreg(dst, c14, 0, c0, 0)

#define GET_CNTPCT(dst_lo, dst_hi) get_cpreg64(dst_lo, dst_hi, c14, 0)

#define GET_CNTP_CVAL(dst_lo, dst_hi) get_cpreg64(dst_lo, dst_hi, c14, 2)
#define SET_CNTP_CVAL(dst_lo, dst_hi) set_cpreg64(dst_lo, dst_hi, c14, 2)

#define GET_CNTP_CTL(dst) get_cpreg(dst, c14, 0, c2, 1)
#define SET_CNTP_CTL(dst) set_cpreg(dst, c14, 0, c2, 1)

#define GET_CNTP_TVAL(dst) get_cpreg(dst, c14, 0, c2, 0);
#define SET_CNTP_TVAL(dst) set_cpreg(dst, c14, 0, c2, 0);

#define HZ 100

int cmd_timer_get_freq(int argc, char **argv)
{
	uint32_t dst;
	GET_CNTFRQ(dst);
	printf("CNTFRQ: %u\n", dst);
}

int cmd_timer_get_count(int argc, char **argv)
{
	uint32_t dst_hi, dst_lo;
	GET_CNTPCT(dst_hi, dst_lo);
	printf("CNTPCT: hi 0x%x lo 0x%x\n", dst_hi, dst_lo);
}

int cmd_timer_get_ctl(int argc, char **argv)
{
	uint32_t dst;
	GET_CNTP_CTL(dst);
	printf("CNTP_CTL: 0x%x\n", dst);
}

void timer_init(void)
{
	uint32_t dst, hi;

	/* get timer frequency */
	GET_CNTFRQ(dst);

	/* Set timer tval to tick at appx HZ per second, by dividing the
	 * frequency by HZ */
	dst /= HZ;
	SET_CNTP_TVAL(dst);

	/* Enable the timer */
	dst = 1;
	SET_CNTP_CTL(dst); /* enable timer */

	/* cpu enable interrupts */
	asm("msr CPSR, #0x13"); /* processor mode 0x13, plus 0x40 to mask fiq */

	gic_enable_interrupt(30u);
}

void timer_isr(void)
{
	uint32_t reg;

	/* Reset timer to go off in another 1/HZ second */
	GET_CNTFRQ(reg);
	reg /= HZ;
	SET_CNTP_TVAL(reg);

	get_spsr(reg);
	if ((reg & ARM_MODE_MASK) == ARM_MODE_USER) {
		/* We interrupted a user process. This means we can go ahead and
		 * reschedule safely. TODO do this. */
	}

	reg = 1;
	SET_CNTP_CTL(reg);
}
