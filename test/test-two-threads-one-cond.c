#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#ifdef USE_WRAPPERS
#include <evl/evl.h>
#endif

pthread_cond_t cond;
pthread_mutex_t mtx;

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
		printf("%s: pthread_mutex_lock\n", id);
		int ret = pthread_mutex_lock(&mtx);
		if(ret) {
			fprintf(stderr, "%s: pthread_mutex_lock returned %d %s\n", id, ret, strerrorname_np(ret));
			abort();
		}
		printf("%s: pthread_mutex_locked\n", id);
		printf("%s: pthread_cond_signal cond\n", id);
		ret = pthread_cond_signal(&cond);
		if(ret) {
			fprintf(stderr, "%s: pthread_cond_signal returned %d %s\n", id, ret, strerrorname_np(ret));
			abort();
		}
		printf("%s: pthread_cond_wait cond, should drop mutex\n", id);
		ret = pthread_cond_wait(&cond, &mtx);
		if(ret) {
			fprintf(stderr, "%s: pthread_cond_wait returned %d %s\n", id, ret, strerrorname_np(ret));
			abort();
		}
		pthread_mutex_unlock(&mtx);
		printf("%s sleeps\n", id);
		usleep(1000);
	}
}

void* thread(void*) {
	// when USE_WRAPPERS, this thread is already attached to evl by the backend
	allCores();
	loop("thread");
}

void* auxthread(void*) {
	while(1) {
		usleep(1000000);
		printf("auxthread: pthread_mutex_lock\n");
		int ret = pthread_mutex_lock(&mtx);
		if(ret) {
			fprintf(stderr, "auxthread: pthread_mutex_lock returned %d %s\n", ret, strerrorname_np(ret));
			abort();
		}
		printf("auxthread: pthread_mutex_locked\n");
		// do some work
		for(unsigned int n = 0; n < 100000u; ++n) {
			rand();
		}
		printf("auxthread: pthread_mutex_unlock\n");
		ret = pthread_mutex_unlock(&mtx);
		if(ret) {
			fprintf(stderr, "auxthread: pthread_mutex_unlock returned %d %s\n", ret, strerrorname_np(ret));
			abort();
		}
		printf("auxthread: pthread mutex unlocked\n");
	}
	return NULL;
}

int main()
{
	int ret = 0;
#ifdef USE_WRAPPERS
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
#endif
	ret = pthread_mutex_init(&mtx, 0);
	ret |= pthread_cond_init(&cond, 0);
	pthread_t t;
	ret |= pthread_create(&t, 0, thread, 0);
	pthread_t at;
	ret |= pthread_create(&t, 0, auxthread, 0);
	if(ret) {
		fprintf(stderr, "Iniitalisation error\n");
		return ret;
	}
	loop("main");
	return 0;
}
