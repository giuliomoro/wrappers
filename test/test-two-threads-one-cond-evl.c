#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#include <evl/evl.h>

// #define AUXTHREAD

struct evl_event cond;
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
	while(1) {
		printf("%s: evl_lock_mutex\n", id);
		int ret = evl_lock_mutex(&mtx);
		if(ret) {
			fprintf(stderr, "%s: evl_signal_event returned %d %s\n", id, ret, strerrorname_np(-ret));
			abort();
		}
		printf("%s: evl mutex locked\n", id);
		printf("%s: evl_signal_event cond\n", id);
		ret = evl_signal_event(&cond);
		if(ret) {
			fprintf(stderr, "thread: evl_signal_event returned %d %s\n", ret, strerrorname_np(-ret));
			abort();
		}
		printf("%s: evl_wait_event cond, should drop mutex\n", id);
		ret = evl_wait_event(&cond, &mtx);
		if(ret) {
			fprintf(stderr, "%s: evl_wait_event returned %d %s\n", id, ret, strerrorname_np(-ret));
			abort();
		}
		evl_unlock_mutex(&mtx);
		printf("%s: sleeps\n", id);
		usleep(10000);
	}
}

void* thread(void* arg) {
	int ret = evl_attach_thread(EVL_CLONE_PRIVATE, "thread:%d:%u", getpid(), pthread_self());
	if(ret <= 0) {
		fprintf(stderr, "evl_attach_thread returned %d %s\n", ret, strerrorname_np(-ret));
		abort();
	}
	allCores();
	loop("thread");
}

void* auxthread(void*) {
	int ret = evl_attach_thread(EVL_CLONE_PRIVATE, "auxthread:%d:%u", getpid(), pthread_self());
	if(ret <= 0) {
		fprintf(stderr, "evl_attach_thread returned %d %s\n", ret, strerrorname_np(-ret));
		abort();
	}
	while(1) {
		usleep(1000000);
		printf("auxthread: evl_lock_mutex()\n");
		ret = evl_lock_mutex(&mtx);
		if(ret) {
			fprintf(stderr, "auxthread: evl_mutex_lock returned %d %s\n", ret, strerrorname_np(ret));
			abort();
		}
		printf("auxthread: evl locked mutex\n");
		printf("auxthread: evl_unlock_mutex()\n");
		ret = evl_unlock_mutex(&mtx);
		if(ret) {
			fprintf(stderr, "auxthread: evl_mutex_unlock returned %d %s\n", ret, strerrorname_np(ret));
			abort();
		}
		printf("auxthread: evl mutex unlocked()\n");
	}
	return NULL;
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
	ret |= evl_create_event(&cond, EVL_CLOCK_MONOTONIC, EVL_CLONE_PRIVATE, "cond:%d:%p", getpid(), &cond) < 0;
	pthread_t t;
	ret |= pthread_create(&t, 0, thread, 0);
#ifdef AUXTHREAD
	pthread_t at;
	ret |= pthread_create(&at, 0, auxthread, 0);
#endif // AUXTHREAD
	if(ret) {
		fprintf(stderr, "Initialisation error\n");
		return ret;
	}
	loop("main");
	return 0;
}

