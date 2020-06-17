#pragma once
#include <stdint.h>

#include "list.h"
#include "wait.h"

/**
 * A request to read or write a block from a block device.
 *
 * Each device may carry additional fields in a block request, so this structure
 * must be allocated and initialized by the device driver. Any parameters placed
 * in by callers (such as the buf) MUST be freed prior to calling ops->free() on
 * the req, since the device driver will only cleanup the fields it initialized,
 * (which are marked below).
 */
struct blkreq {
	/* PARAMETERS GIVEN AS INPUT */
	enum blkreq_type {
		BLKREQ_READ,
		BLKREQ_WRITE,
	} type;
	uint64_t blkidx;
	uint8_t *buf;
	uint32_t size;
	/* PARAMETERS RETURNED AS OUTPUT */
	enum blkreq_status {
		BLKREQ_OK,
		BLKREQ_ERR,
	} status;
	/* FIELDS INITIALIZED BY alloc() */
	struct waitlist wait;
	struct list_head reqlist;
};

struct blkdev;

/**
 * Block device operations. Each operation MUST be implemented by a block device
 * driver.
 */
struct blkdev_ops {
	/**
	 * Create a blkreq structure, and initialize the waitlist and request
	 * list fields of it. Return the structure.
	 */
	struct blkreq *(*alloc)(struct blkdev *dev);
	/**
	 * Free a blkreq structure. THIS WILL NOT FREE THE BUFFER CONTAINED
	 * WITHIN IT.
	 */
	void (*free)(struct blkdev *dev, struct blkreq *req);
	/**
	 * Submit a block request to the device.
	 *
	 * When this function returns, the request is submitted, but not
	 * finished. The `wait` field of the req is an event which can be used
	 * to block until the request is satisfied. However, a caller may want
	 * to submit additional requests first, and then wait for all of them.
	 *
	 * Submitting the request to the device hands over ownership until the
	 * `wait` event is triggered. Callers MAY NOT modify the request after
	 * this function returns, until the wait event is triggered.
	 */
	void (*submit)(struct blkdev *dev, struct blkreq *req);
	/**
	 * Print out status information.
	 */
	void (*status)(struct blkdev *dev);
};

struct blkdev {
	struct list_head blklist;
	struct blkdev_ops *ops;
	uint64_t blkcnt;
	uint32_t blksiz;
	char name[16];
};

/**
 * Initialize the block subsystem.
 */
void blk_init(void);
/**
 * Initialize a block request. This is called by device drivers as part of
 * alloc. Users of the block API need not use it.
 */
void blkreq_init(struct blkreq *req);
/**
 * Register a block device with the block subsystem.
 */
void blkdev_register(struct blkdev *blk);
/**
 * Retrieve a block device by its name.
 */
struct blkdev *blkdev_get_by_name(char *name);
