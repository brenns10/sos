/*
 * Timer-related functionality
 */
#include "gic.h"
#include "kernel.h"

#define TIMER_INTID 30

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
	uint32_t dst;

	/* get timer frequency */
	GET_CNTFRQ(dst);

	/* Set timer tval to tick at appx HZ per second, by dividing the
	 * frequency by HZ */
	dst /= HZ;
	SET_CNTP_TVAL(dst);

	/* Enable the timer */
	dst = 1;
	SET_CNTP_CTL(dst); /* enable timer */

	gic_register_isr(TIMER_INTID, 1, timer_isr);
	gic_enable_interrupt(TIMER_INTID);
}

void timer_isr(uint32_t intid)
{
	uint32_t reg;

	/* Reset timer to go off in another 1/HZ second */
	GET_CNTFRQ(reg);
	reg /= HZ;
	SET_CNTP_TVAL(reg);

	/* Ensure the timer is still on */
	reg = 1;
	SET_CNTP_CTL(reg);

	/* Interrupt should now be safe to clear */
	gic_end_interrupt(intid);

	get_spsr(reg);
	reg = reg & ARM_MODE_MASK;
	if (reg == ARM_MODE_USER || reg == ARM_MODE_SYS) {
		/* We interrupted sys/user mode. This means we can go ahead and
		 * reschedule safely. */
		schedule();
	}

}
