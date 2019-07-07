/**
 * unittest.c: routines for running unit tests
 */
#include "unittest.h"

#define nelem(x) (sizeof(x) / sizeof(x[0]))

void unittest_fail(struct unittest *test, struct unittest_failure failure)
{
	if (test->failure_count < nelem(test->failures))
		test->failures[test->failure_count++] = failure;
}

static unsigned int unittest_run_case(struct unittest *test, struct unittest_case kase)
{
	unsigned int i = 0;
	test->failure_count = 0;
	test->module->printf("\t%s: ", kase.name);
	kase.function(test);

	if (test->failure_count) {
		test->module->printf("FAIL\n");
		for (i = 0; i < test->failure_count; i++)
			test->module->printf(
				"%s:%u: %s\n",
				test->failures[i].file,
				test->failures[i].line,
				test->failures[i].message
			);
		return 1;
	} else {
		test->module->printf("pass\n");
		return 0;
	}
}

unsigned int unittest_run_module(struct unittest_module *module)
{
	struct unittest test;
	unsigned int i, failures = 0;
	test.module = module;

	module->printf("test module: %s\n", module->name);

	if (module->init)
		module->init(&test);

	for (i = 0; module->cases[i].function; i++)
		failures += unittest_run_case(&test, module->cases[i]);

	if (module->exit)
		module->exit(&test);

	return failures;
}
