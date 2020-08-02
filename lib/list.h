/**
 * list.h: a dead simple, C89 linked list library
 *
 * No credit taken here: this is essentially the Linux kernel's linked list
 * library. Some differences here are that we do not depend on the nonstandard
 * GNU `typeof()` keyword. We do not use inline functions, since these are C89
 * incompatible. However, the compiler is generally smart enough to inline them
 * anyway.
 */
#pragma once
#include <stddef.h>

/**
 * struct list_head: Embed this into your structs which will be part of a list
 *
 *     struct some_item_in_a_list {
 *       ... fields ...
 *       struct list_head list;
 *     }
 *
 * You can include it multiple times if the struct is present in multiple lists.
 * Next, declare a list_head outside of this list to be your reference:
 *
 *     struct list_head list_of_some_items;
 *     ...
 *     INIT_LIST_HEAD(list_of_some_items);
 *
 * Finally, use that as the head and begin using the methods described below.
 */
struct list_head {
	struct list_head *next;
	struct list_head *prev;
};

/**
 * struct hlist_head: Embed this into structs for a singly linked list
 */
struct hlist_head {
	struct hlist_head *next;
};

/**
 * Initialize a "header" for a list by making it point to itself (i.e. empty).
 * This should be used dynamically. You need not use this before adding an
 * element to an existing list.
 */
#define INIT_LIST_HEAD(name)                                                   \
	do {                                                                   \
		name.next = &name;                                             \
		name.prev = &name;                                             \
	} while (0);

#define INIT_HLIST_HEAD(name)                                                  \
	do {                                                                   \
		name.next = &name;                                             \
	} while (0);

/**
 * Declare a "header" for a list which points to itself. Use this for static
 * allocations.
 */
#define DECLARE_LIST_HEAD(name)  struct list_head name = { &name, &name }
#define DECLARE_HLIST_HEAD(name) struct hlist_head name = { &name }

/**
 * Insert `item` (presumably embedded into a struct) into a list which is
 * referred to via `head`. Inserts at the beginning of the list.
 */
void list_insert(struct list_head *head, struct list_head *item);
void hlist_insert(struct hlist_head *head, struct hlist_head *item);

/**
 * Insert `item` at the end of the list.
 */
void list_insert_end(struct list_head *head, struct list_head *item);

/**
 * Remove `item` from the list it is contained in.
 */
void list_remove(struct list_head *item);
void hlist_remove(struct hlist_head *parent_or_head, struct hlist_head *item);

/**
 * Iterate through list_head structures. This usually is not a very helpful
 * technique, but it's here for you.
 *
 * variable: name of a `struct list_head *` variable to store each node
 * head: name of the list header
 */
#define list_for_each(variable, head)                                          \
	for (variable = (head)->next; variable != (head);                      \
	     variable = variable->next)

/**
 * Offset, in bytes, of a field named `member` within type `type`.
 */
#define list_offsetof(type, member) ((size_t) & (((type *)0)->member))

/**
 * Evaluates to a pointer to the struct containing a member:
 *
 * ptr: pointer to a member within a struct
 * type: type of the containing struct (not a pointer, just the type)
 * member: field name of member within the struct
 */
#define container_of(ptr, type, member)                                        \
	((type *)((char *)ptr - list_offsetof(type, member)))

#define alt_offsetof(ptr, member) (((size_t) & (ptr)->member) - ((size_t)(ptr)))
#define alt_container_of(ptr, contptr, member)                                 \
	((void *)((char *)ptr - alt_offsetof(contptr, member)))

/**
 * Iterate over every item within a list. Use like a normal for statement.
 *
 * var_ptr: variable which is a pointer to the container type, used as the
 *          iteration variable
 * head: pointer to struct list_head which is the header of the list
 * field_name: name of the struct list_head within the container type
 *
 * Sample code:
 *
 *   struct my_example {
 *     struct list_head list_field;
 *     char *message;
 *   }
 *   struct list_head head;
 *
 *   // assume everything is initialized and populated
 *
 *   struct my_example *iter;
 *   list_for_each_entry(iter, &head, list_field) {
 *     printf("list entry: \"%s\"\n", iter->message);
 *   }
 *
 */
#define list_for_each_entry(var_ptr, head, field_name)                         \
	for (var_ptr = NULL,                                                   \
	    var_ptr = alt_container_of((head)->next, var_ptr, field_name);     \
	     &var_ptr->field_name != (head);                                   \
	     var_ptr = alt_container_of(var_ptr->field_name.next, var_ptr,     \
	                                field_name))

/**
 * Iterate over every item within a list, but allow removing the node you are
 * iterating over while doing so.
 *
 * var_ptr: variable which is a pointer to the container type, used as the
 *          iteration variable
 * next_ptr: variable to store a copy of the next item in iteration
 * head: pointer to struct list_head which is the header of the list
 * field_name: name of the struct list_head within the container type
 *
 */
#define list_for_each_entry_safe(var_ptr, next_ptr, head, field_name)          \
	for (var_ptr = NULL,                                                   \
	    var_ptr = alt_container_of((head)->next, var_ptr, field_name),     \
	    next_ptr = alt_container_of(var_ptr->field_name.next, var_ptr,     \
	                                field_name);                           \
	     &var_ptr->field_name != (head); var_ptr = next_ptr,               \
	    next_ptr = alt_container_of(next_ptr->field_name.next, var_ptr,    \
	                                field_name))
