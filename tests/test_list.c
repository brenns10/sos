#include <stdio.h>

#include "unittest.h"
#include "list.h"

struct list_head list;

struct item {
	struct list_head list;
	int number;
};

static void init(struct unittest *test)
{
	INIT_LIST_HEAD(list);
}

static void test_insert_and_remove(struct unittest *test)
{
	struct item my_item;

	UNITTEST_EXPECT_EQ(test, list.next, &list);
	UNITTEST_EXPECT_EQ(test, list.prev, &list);

	list_insert(&list, &my_item.list);

	UNITTEST_EXPECT_EQ(test, list.next, &my_item.list);
	UNITTEST_EXPECT_EQ(test, list.prev, &my_item.list);
	UNITTEST_EXPECT_EQ(test, my_item.list.next, &list);
	UNITTEST_EXPECT_EQ(test, my_item.list.prev, &list);

	list_remove(&my_item.list);

	UNITTEST_EXPECT_EQ(test, list.next, &list);
	UNITTEST_EXPECT_EQ(test, list.prev, &list);
}

static void test_insert_end(struct unittest *test)
{
	struct item first;
	struct item second;
	struct item *iter;
	int i = 0;

	first.number = 1;
	second.number = 2;

	list_insert(&list, &first.list);
	list_insert_end(&list, &second.list);

	list_for_each_entry(iter, &list, list, struct item) {
		i++;
		UNITTEST_EXPECT_EQ(test, iter->number, i);
	}
}

struct unittest_case cases[] = {
	UNITTEST_CASE(test_insert_and_remove),
	UNITTEST_CASE(test_insert_end),
	{0}
};

struct unittest_module module = {
	.name="lists",
	.init=init,
	.cases=cases,
	.printf=printf,
};

UNITTEST(module);
