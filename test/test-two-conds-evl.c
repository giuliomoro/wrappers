#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#include <evl/evl.h>

struct evl_event cond0;
struct evl_event cond1;
struct evl_mutex mtx;

void allCores() {
	cpu_set_t set;
	CPU_ZERO(&set);
	for(unsigned int n = 0; n < 64; ++n)
		CPU_SET(n, &set);

	if(sched_setaffinity(gettid(), sizeof(set), &set) == -1) {
		fprintf(stderr, "sched_setaffinity error\n");
		abort();
	}
}

void loop(const char* id) {
	int mn = 0 == strcmp(id, "main");
	struct evl_event* firstCond = mn ? &cond0 : &cond1;
	const char* firstCondStr = mn ? "cond0" : "cond1";
	struct evl_event* secondCond = mn ? &cond1 : &cond0;
	const char* secondCondStr = mn ? "cond1" : "cond0";
	while(1) {
		printf("%s: evl_lock_mutex\n", id);
		int ret = evl_lock_mutex(&mtx);
		if(ret) {
			fprintf(stderr, "thread: evl_signal_event returned %d %s\n", ret, strerrorname_np(-ret));
			abort();
		}
		printf("%s: evl mutex locked\n", id);
		printf("%s: evl_signal_event %s\n", id, firstCondStr);
		ret = evl_signal_event(firstCond);
		if(ret) {
			fprintf(stderr, "%s: evl_signal_event returned %d %s\n", id, ret, strerrorname_np(-ret));
			abort();
		}
		printf("%s: evl_wait_event %s, drops mutex\n", id, secondCondStr);
		ret = evl_wait_event(secondCond, &mtx);
		if(ret) {
			fprintf(stderr, "%s: evl_wait_event returned %d %s\n", id, ret, strerrorname_np(-ret));
			abort();
		}
		evl_unlock_mutex(&mtx);
		printf("%s: sleeps\n", id);
		usleep(1000);
	}
}

void* thread(void*) {
	int ret = evl_attach_thread(EVL_CLONE_PRIVATE, "thread:%d:%u", getpid(), pthread_self());
	if(ret <= 0) {
		fprintf(stderr, "evl_attach_thread returned %d %s\n", ret, strerrorname_np(-ret));
		abort();
	}
	allCores();
	loop("thread");
}

int main()
{
	int ret = 0;
	ret = evl_init();
	if(ret) {
		fprintf(stderr, "evl_init() returned %d\n", ret);
		abort();
	}
	ret = evl_attach_self("%s:%d:%u", "main", gettid(), rand());
	if(ret < 0) {
		fprintf(stderr, "evl_attach_self() returned %d\n", ret);
		abort();
	}
	allCores();
	ret = evl_create_mutex(&mtx, EVL_CLOCK_MONOTONIC, 0, EVL_MUTEX_NORMAL, "mutex:%d:%p", getpid(), &mtx) < 0;
	ret |= evl_create_event(&cond0, EVL_CLOCK_MONOTONIC, EVL_CLONE_PRIVATE, "cond0:%d:%p", getpid(), &cond0) < 0;
	ret |= evl_create_event(&cond1, EVL_CLOCK_MONOTONIC, EVL_CLONE_PRIVATE, "cond1:%d:%p", getpid(), &cond1) < 0;
	pthread_t t;
	ret |= pthread_create(&t, 0, thread, 0);
	if(ret) {
		fprintf(stderr, "Iniitalisation error\n");
		return ret;
	}
	loop("main");
	while(1) {
		printf("main: evl_lock_mutex\n");
		ret = evl_lock_mutex(&mtx);
		if(ret) {
			fprintf(stderr, "main: evl_lock_mutex returned %d %s\n", ret, strerrorname_np(-ret));
			abort();
		}
		printf("main: evl locked mutex\n");
		printf("main: evl_signal_event cond0\n");
		ret = evl_signal_event(&cond0);
		if(ret) {
			fprintf(stderr, "main: evl_signal_event returned %d %s\n", ret, strerrorname_np(-ret));
			abort();
		}
		printf("main: evl_wait_event cond1, drops mutex\n");
		ret = evl_wait_event(&cond1, &mtx);
		if(ret) {
			fprintf(stderr, "main: evl_wait_event returned %d %s\n", ret, strerrorname_np(ret));
			abort();
		}
		evl_unlock_mutex(&mtx);
		printf("main sleeps\n");
		usleep(100000);
	}
	return 0;
}

