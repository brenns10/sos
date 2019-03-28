/**
 * list.c: a dead simple, C89 linked list library
 *
 * See list.h
 */

#include "list.h"

void list_insert(struct list_head *head, struct list_head *item)
{
	item->next = head->next;
	item->prev = head;
	item->next->prev = item;
	head->next = item;
}

void list_remove(struct list_head *item)
{
	item->prev->next = item->next;
	item->next->prev = item->prev;
}

void list_init(struct list_head *head)
{
	head->prev = head;
	head->next = head;
}
