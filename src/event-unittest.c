#include "src/event.h"
#include "src/test_harness.h"

#include <sys/time.h>

TEST(every) {
	struct event *e = event_every(3);
	time_t then = time(NULL);
	time_t now;
	EXPECT_EQ(1, event_wait(e));
	now = time(NULL);
	EXPECT_GT(now, then + 2);
	event_free(e);
}

TEST(fdread) {
	int fds[2];
	struct event *e;
	char z = '0';

	ASSERT_EQ(0, pipe(fds));
	e = event_fdread(fds[0]);

	write(fds[1], &z, 1);
	EXPECT_EQ(1, event_wait(e));
	event_free(e);
}

TEST(composite) {
	struct event *e0 = event_every(2);
	struct event *e1 = event_every(3);
	struct event *ec = event_composite();
	time_t then = time(NULL);
	time_t now;

	ASSERT_EQ(0, event_composite_add(ec, e0));
	ASSERT_EQ(0, event_composite_add(ec, e1));
	EXPECT_EQ(1, event_wait(ec));
	EXPECT_EQ(1, event_wait(ec));
	EXPECT_EQ(1, event_wait(ec));
	now = time(NULL);
	EXPECT_EQ(now, then + 4);
	event_free(ec);
	event_free(e1);
	event_free(e0);
}

TEST_HARNESS_MAIN
