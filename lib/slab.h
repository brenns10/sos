#pragma once

/*
 * A private structure representing a cache of objects which you can allocate
 * and free from. Dynamically grows and shrinks as necessary.
 */
struct slab;

/**
 * Create a new slab cache.
 *
 * size: the size of the objects which you will allocate from it
 * getter: a function which returns pages of memory
 * freer: a function which frees pages of memory
 */
struct slab *slab_new(unsigned int size, void *(*getter)(void), void (*freer)(void*));

void *slab_alloc(struct slab *slab);

void slab_free(void *ptr);
