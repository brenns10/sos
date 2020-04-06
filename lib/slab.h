/*
 * slab.h: A simple slab allocator library.
 *
 * Slab allocators are used to allocate commonly used structures, such as files,
 * sockets, packets, etc. They help reduce memory fragmentation and can very
 * quickly allocate in most cases.
 *
 * This implementation can grow, but not shrink. Note that it depends on the
 * page size, and that wasted memory can occur if your objects don't fit nicely
 * into a page. Common examples of this are:
 *
 * - Objects larger than a page, of course
 * - Objects which are large and not near a power of two. For example, a
 *   3072-byte structure will fit once within a page, wasting 1024 bytes in
 *   every page which is part of the slab. This allocator is not equipped to
 *   allocate many contiguous pages in order to reduce waste.
 */

#pragma once

/*
 * A private structure representing a cache of objects which you can allocate
 * and free from. Dynamically grows and shrinks as necessary.
 */
struct slab;

/**
 * Create a new named slab cache.
 *
 * name: name of the slab allocator (used in diagnostics)
 * size: size of the item
 * getter: function which returns freshly allocated pages
 */
struct slab *slab_new(char *name, unsigned int size, void *(*getter)(void));

/**
 * Allocate an object from the slab.
 *
 * slab: the slab returned by slab_new()
 */
void *slab_alloc(struct slab *slab);

/**
 * Free an object back to the slab.
 *
 * slab: the slab you allocated the object from
 * ptr: a pointer to free, must be the same as one returned by slab_alloc()
 */
void slab_free(struct slab *slab, void *ptr);

/**
 * Report on the status of a slab. Requires a printf implementation linked in
 * with the library.
 *
 * slab: slab you would like to report on
 */
void slab_report(struct slab *slab);

/**
 * Report on the status of all slabs. Requires a printf implementation linked in
 * with the library.
 */
void slab_report_all(void);
