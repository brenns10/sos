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
	INIT_SPINSEM(&wl->waitlock, 1);
	wl->triggered = false;
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
	int flags;
	struct waiter waiter;
	spin_acquire_irqsave(&wl->waitlock, &flags);
	if (wl->triggered) {
		spin_release_irqrestore(&wl->waitlock, &flags);
		return;
	}
	waiter.proc = current;
	wl->waitcount++;
	hlist_insert(&wl->waiting, &waiter.list);
	current->flags.pr_ready = 0;
	spin_release_irqrestore(&wl->waitlock, &flags);
	schedule();
}

void wait_list_awaken(struct waitlist *wl)
{
	struct waiter *waiter;
	int flags;
	spin_acquire_irqsave(&wl->waitlock, &flags);
	wl->triggered = true;
	list_for_each_entry(waiter, &wl->waiting, list)
	{
		waiter->proc->flags.pr_ready = 1;
	}
	spin_release_irqrestore(&wl->waitlock, &flags);
}
