/*
 * wait.h: Allow kernel threads to wait for events before resuming
 *
 * Anything which would like to allow other threads to wait on it may either
 * allocate a struct waitlist, or initialize a struct waitlist embedded within
 * some other data structure.
 *
 * Anything which would like to wait on another thread should initialize a
 * struct waiter, and use wait_for() to go to sleep until woken by the waitlist.
 * When the owner of the waitlist wants to trigger the waiting processes
 * (perhaps causing a stampeding herd but we don't worry about that yet), it
 * calls wait_list_awaken(), which awakens each kthread and empties the
 * waitlist.
 */
#pragma once

#include "list.h"

struct waitlist {
	struct hlist_head waiting;
	int waitcount;
};

struct waiter {
	struct hlist_head list;
	struct process *proc;
};

/**
 * @brief Initialize a waitlist
 * @param wl Waitlist to init
 */
void wait_list_init(struct waitlist *wl);

/**
 * @brief Destroy a waitlist
 *
 * Although the current wait list implementation does not require any clean up,
 * this provides an opportunity to issue a warning if anybody is still waiting.
 *
 * @param wl Waitlist to destroy
 */
void wait_list_destroy(struct waitlist *wl);

/**
 * @brief Begin waiting for a waitlist to be triggered
 * @param wl waitlist to wait for
 */
void wait_for(struct waitlist *wl);

/**
 * @brief Awaken all process in the waitlist
 * @param wl waitlist to awaken
 */
void wait_list_awaken(struct waitlist *wl);
