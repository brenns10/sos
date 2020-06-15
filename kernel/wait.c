/*
 * wait.c: Allow kernel threads to wait for events before resuming
 */
#include "wait.h"
#include "kernel.h"
#include "list.h"

void wait_list_init(struct waitlist *wl)
{
	INIT_HLIST_HEAD(wl->waiting);
	wl->waitcount = 0;
}

void wait_list_destroy(struct waitlist *wl)
{
	if (wl->waitcount > 0) {
		printf("WARN: destroying waitlist with %d waiters\n",
		       wl->waitcount);
	}
}

void wait_for(struct waitlist *wl)
{
	struct waiter waiter;
	waiter.proc = current;
	wl->waitcount++;
	hlist_insert(&wl->waiting, &waiter.list);
	current->flags.pr_ready = 0;
	block((uint32_t *)&current->context);
}

void wait_list_awaken(struct waitlist *wl)
{
	struct waiter *waiter;
	list_for_each_entry(waiter, &wl->waiting, list, struct waiter)
	{
		waiter->proc->flags.pr_ready = 1;
	}
	wait_list_init(wl); /* reset the list */
}
