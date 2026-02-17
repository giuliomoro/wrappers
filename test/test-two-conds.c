#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#ifdef USE_WRAPPERS
#include <evl/evl.h>
#endif

pthread_cond_t cond0;
pthread_cond_t cond1;
pthread_mutex_t mtx;

void loop(const char* id) {
	int mn = 0 == strcmp(id, "main");
	pthread_cond_t* firstCond = mn ? &cond0 : &cond1;
	const char* firstCondStr = mn ? "cond0" : "cond1";
	pthread_cond_t* secondCond = mn ? &cond1 : &cond0;
	const char* secondCondStr = mn ? "cond1" : "cond0";
	while(1)
	{
		printf("%s: pthread_mutex_lock\n", id);
		int ret = pthread_mutex_lock(&mtx);
		if(ret) {
			fprintf(stderr, "%s: pthread_mutex_lock returned %d %s\n", id, ret, strerrorname_np(ret));
			abort();
		}
		printf("%s: pthread_mutex_locked\n", id);
		printf("%s: pthread_cond_signal %s\n", id, firstCondStr);
		ret = pthread_cond_signal(firstCond);
		if(ret) {
			fprintf(stderr, "%s: pthread_cond_signal returned %d %s\n", id, ret, strerrorname_np(ret));
			abort();
		}
		printf("%s: pthread_cond_wait %s, drops mutex\n", id, secondCondStr);
		ret = pthread_cond_wait(secondCond, &mtx);
		if(ret) {
			fprintf(stderr, "%s: pthread_cond_wait returned %d %s\n", id, ret, strerrorname_np(ret));
			abort();
		}
		pthread_mutex_unlock(&mtx);
		printf("%s: sleeps\n", id);
		usleep(1000);
	}
}

void* thread(void*) {
	// when USE_WRAPPERS, this thread is already attached to evl by the backend
	loop("thread");
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
	ret |= pthread_cond_init(&cond0, 0);
	ret |= pthread_cond_init(&cond1, 0);
	pthread_t t;
	ret |= pthread_create(&t, 0, thread, 0);
	if(ret) {
		fprintf(stderr, "Iniitalisation error\n");
		return ret;
	}
	loop("main");
	return 0;
}
