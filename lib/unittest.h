/**
 * unittest.h: a simple unit testing library, inspired by KUnit
 *
 * See tests/test_list.c for a full usage example.
 */

#pragma once

struct unittest_failure {
	char *file;
	unsigned int line;
	char *message;
};

struct unittest_module; /* forward declare */
struct unittest {
	struct unittest_failure failures[20];
	unsigned int failure_count;
	struct unittest_module *module;
};

typedef void (*unittest_t)(struct unittest *);

struct unittest_case {
	unittest_t function;
	char *name;
};

struct unittest_module {
	char *name;
	unittest_t init;
	unittest_t exit;
	struct unittest_case *cases;
	int (*printf) (const char *fmt, ...);
};

void unittest_fail(struct unittest *, struct unittest_failure);
unsigned int unittest_run_module(struct unittest_module *module);

#define UNITTEST(module) \
	int main(int argc, char **argv) \
	{ \
		return (int)unittest_run_module(&module); \
	}

#define UNITTEST_EXPECT_EQ(test, a, b) \
	do { \
		if (a != b) \
			unittest_fail(test, (struct unittest_failure){ \
				.line=__LINE__, \
				.file=__FILE__, \
				.message="expectation failed" \
			}); \
	} while (0)

#define UNITTEST_ASSERT_EQ(test, a, b) \
	do { \
		if (a != b) { \
			unittest_fail(test, (struct unittest_failure){ \
				.line=__LINE__, \
				.file=__FILE__, \
				.message="expectation failed" \
			}); \
			return; \
		} \
	} while (0)

#define UNITTEST_CASE(func) ((struct unittest_case) { \
	.function=func, \
	.name=#func \
})
