/*
 * Timer-related functionality
 */
#include "gic.h"
#include "kernel.h"
#include "ksh.h"

#include "arm-mailbox.h"
#include "config.h"

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

static int cmd_timer_get_freq(int argc, char **argv)
{
	uint32_t dst;
	GET_CNTFRQ(dst);
	printf("CNTFRQ: %u\n", dst);
	return 0;
}

static int cmd_timer_get_count(int argc, char **argv)
{
	uint32_t dst_hi, dst_lo;
	GET_CNTPCT(dst_hi, dst_lo);
	printf("CNTPCT: hi 0x%x lo 0x%x\n", dst_hi, dst_lo);
	return 0;
}

static int cmd_timer_get_ctl(int argc, char **argv)
{
	uint32_t dst;
	GET_CNTP_CTL(dst);
	printf("CNTP_CTL: 0x%x\n", dst);
	return 0;
}

struct ksh_cmd timer_ksh_cmds[] = {
	KSH_CMD("get-freq", cmd_timer_get_freq, "get timer frequency"),
	KSH_CMD("get-count", cmd_timer_get_count, "get current timer value"),
	KSH_CMD("get-ctl", cmd_timer_get_ctl, "get timer ctl register"),
	{ 0 },
};

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

	gic_register_isr(TIMER_INTID, 1, timer_isr, "timer");
	gic_enable_interrupt(TIMER_INTID);
}

static uint32_t timer_count = 0;


static void timer_tick_fallback(uint32_t arg)
{
}

void timer_isr(uint32_t intid, struct ctx *ctx)
{
	uint32_t reg;

	/* Reset timer to go off in another 1/HZ second */
	GET_CNTFRQ(reg);
	reg /= HZ;
	SET_CNTP_TVAL(reg);

	/* Ensure the timer is still on */
	reg = 1;
	SET_CNTP_CTL(reg);

	timer_count++;
	timer_tick(timer_count);

	if (timer_can_reschedule(ctx)) {
		/* We interrupted sys/user mode. This means we can go ahead and
		 * reschedule safely. */
		irq_schedule(ctx);
	}

	/* Interrupt should now be safe to clear */
	gic_end_interrupt(intid);
}
