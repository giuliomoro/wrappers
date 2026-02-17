#include "wrappers.h"
#include <pthread.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include <evl/evl.h>
static int verbose = 0;

int getrtid() { // relative thread id
	return gettid() - getpid();
}
enum {
	kLen = 100,
};
struct Map {
	void* key;
	void* value;
};

static void init() {
	static int inited = 0;
	if(!inited) {
		inited = 1;
	}
}

void* mapGet(struct Map* map, void* key) {
	init();
	for(unsigned int n = 0; n < kLen; ++n) {
		if(key == map[n].key) {
			return map[n].value;
		}
	}
	fprintf(stderr, "BUG: did not find element in map %p\n", key);
	return NULL;
}

static void mapRm(struct Map* map, void* key) {
	init();
	for(unsigned int n = 0; n < kLen; ++n) {
		if(key == map[n].key) {
			map[n].key = NULL;
		}
	}
}

static void mapAdd(struct Map* map, void* key, void* value) {
	init();
	int stored = 0;
	for(unsigned int n = 0; n < kLen; ++n) {
		if(NULL == map[n].key) {
			map[n].key = key;
			map[n].value = value;
			stored = 1;
			break;
		}
	}
	if(!stored) {
		fprintf(stderr, "wrappers: mapAdd(): no space left\n");
		abort();
	}
}

static struct Map conds[kLen];

static struct evl_event* evlGetCond(pthread_cond_t* c) {
	return (struct evl_event*)mapGet(conds, (void*)c);
}

static void evlRmCond(pthread_cond_t* c) {
	verbose && printf("evlRmCond %p\n", c);
	mapRm(conds, (void*)c);
}

static void evlAddCond(pthread_cond_t* c, struct evl_event* e) {
	verbose && printf("evlAddCond %p %p\n", c, e);
	mapAdd(conds, (void*)c, (void*)e);
}

static struct Map mtxs[kLen];

static struct evl_mutex* evlGetMtx(pthread_mutex_t* c) {
	return (struct evl_mutex*)mapGet(mtxs, (void*)c);
}

static void evlAddMtx(pthread_mutex_t* pm, struct evl_mutex* em) {
	verbose && printf("evlAddMtx %p %p\n", pm, em);
	mapAdd(mtxs, (void*)pm, (void*)em);
}

static void evlRmMtx(pthread_mutex_t* m) {
	verbose && printf("evlRmMtx %p\n", m);
	mapRm(mtxs, (void*)m);
}

WRAPPER_IMPL(int, pthread_cond_init, (pthread_cond_t* cond,  pthread_condattr_t *cond_attr))
{
	verbose && printf("wrappers: pthread_cond_init() %p\n", cond);
	struct evl_event* e = malloc(sizeof(struct evl_event));
	if(!e) {
		fprintf(stderr, "wrappers: pthread_cond_init(): unable to allocate memory\n");
		abort();
	}
	int ret = evl_create_event(e, EVL_CLOCK_MONOTONIC, EVL_CLONE_PRIVATE, "cond:%d:%p", getpid(), cond);
	if(ret >= 0) {
		evlAddCond(cond, e);
	} else {
		free(e);
	}
	return ret < 0 ? -ret : 0;
}

WRAPPER_IMPL(int, pthread_cond_destroy, (pthread_cond_t* cond))
{
	struct evl_event* e = evlGetCond(cond);
	int ret = evl_close_event(e);
	free(e);
	evlRmCond(cond);
	return ret;
}

WRAPPER_IMPL(int, pthread_cond_wait, (pthread_cond_t* cond, pthread_mutex_t* mutex))
{
	verbose > 1 && printf("%d pthread_cond_wait %p=%p %p=%p\n", getrtid(), cond, evlGetCond(cond), mutex, evlGetMtx(mutex));
	int ret = evl_wait_event(evlGetCond(cond), evlGetMtx(mutex));
	if(ret) {
		verbose && fprintf(stderr, "wrappers: pthread_cond_wait() errored %d\n", ret);
	} else {
		verbose > 1 && printf("%d pthread_cond_wait %p=%p %p=%p => %d\n", getrtid(), cond, evlGetCond(cond), mutex, evlGetMtx(mutex), ret);
	}
	return ret;
}

WRAPPER_IMPL(int, pthread_cond_timedwait, (pthread_cond_t* cond, pthread_mutex_t* mutex, const struct timespec *abstime))
{
	verbose > 1 && printf("%d pthred_cond_timedwait %p=%p %p=%p\n", getrtid(), cond, evlGetCond(cond), mutex, evlGetMtx(mutex));
	int ret = evl_timedwait_event(evlGetCond(cond), evlGetMtx(mutex), abstime);
	verbose > 1 && printf("%d pthred_cond_timedwait %p=%p %p=%p => %d\n", getrtid(), cond, evlGetCond(cond), mutex, evlGetMtx(mutex), ret);
	return -ret;
}

WRAPPER_IMPL(int, pthread_cond_signal, (pthread_cond_t* cond))
{
	verbose > 1 && printf("%d pthread_cond_signal %p=%p\n", getrtid(), cond, evlGetCond(cond));
	int ret = evl_signal_event(evlGetCond(cond));
	if(ret) {
		verbose && fprintf(stderr, "wrappers: pthread_cond_signal() couldn't signal: %d\n", ret);
	}
	return ret;
}

WRAPPER_IMPL(int, pthread_cond_broadcast, (pthread_cond_t* cond))
{
	return evl_broadcast_event(evlGetCond(cond));
}

WRAPPER_IMPL(int, pthread_mutex_init, (pthread_mutex_t* mtx, const pthread_mutexattr_t *mutexattr))
{
	int flags = 0;
	if(mutexattr) {
		int kind = PTHREAD_MUTEX_FAST_NP;
		int err = pthread_mutexattr_gettype(mutexattr, &kind);
		switch(kind) {
			case PTHREAD_MUTEX_FAST_NP:
				flags |= EVL_MUTEX_NORMAL;
				break;
			case PTHREAD_MUTEX_RECURSIVE_NP:
				flags |= EVL_MUTEX_RECURSIVE;
				break;
			case PTHREAD_MUTEX_ERRORCHECK_NP: //unsupported by evl
			default:
				err = EINVAL;
				break;
		}
		if(err) {
			fprintf(stderr, "wrappers: pthread_mutex_init() passed invalid attr, using defaults\n");
		}

	}
	struct evl_mutex* m = malloc(sizeof(struct evl_mutex));
	if(!m) {
		fprintf(stderr, "wrappers: pthread_mutex_init(): unable to allocate memory\n");
		abort();
	}
	int ret = evl_create_mutex(m, EVL_CLOCK_MONOTONIC, 0, flags, "mutex:%d:%p", getpid(), m);
	if(ret >= 0) {
		evlAddMtx(mtx, m);
	} else {
		fprintf(stderr, "wrappers: pthread_mutex_init(): evl_create_mutex(\"mutex:%d:%p\") returned %d %s\n", getpid(), m, ret, strerrorname_np(-ret));
		free(m);
	}
	return ret < 0 ? -ret : 0;
}

WRAPPER_IMPL(int, pthread_mutex_destroy, (pthread_mutex_t* mtx))
{
	struct evl_mutex* m = evlGetMtx(mtx);
	int ret = evl_close_mutex(m);
	free(m);
	evlRmMtx(mtx);
	return ret;
}

WRAPPER_IMPL(int, pthread_mutex_lock, (pthread_mutex_t* mtx))
{
	verbose > 1 && printf("%d pthread_mutex_lock() %p=%p\n", getrtid(), mtx, evlGetMtx(mtx));
	int ret = evl_lock_mutex(evlGetMtx(mtx));
	if(ret) {
		verbose && fprintf(stderr, "wrappers: pthread_mutex_lock() couldn't lock: %d\n", ret);
	} else {
		verbose > 1 && printf("%d pthread_mutex_locked() %p=%p\n", getrtid(), mtx, evlGetMtx(mtx));
	}
	return -ret;
}

WRAPPER_IMPL(int, pthread_mutex_trylock, (pthread_mutex_t* mtx))
{
	return evl_trylock_mutex(evlGetMtx(mtx));
}

WRAPPER_IMPL(int, pthread_mutex_unlock, (pthread_mutex_t* mtx))
{
	verbose > 1 && printf("%d pthread_mutex_unlock() %p=%p\n", getrtid(), mtx, evlGetMtx(mtx));
	int ret = evl_unlock_mutex(evlGetMtx(mtx));
	if(ret) {
		verbose && fprintf(stderr, "wrappers: pthread_mutex_unlock() couldn't unlock: %d\n", ret);
	}
	return -ret;
}

struct TrampolineArgs {
	void* (*start_routine)(void*);
	void* restrict arg;
};

static void* trampoline(void* arg) {
	struct TrampolineArgs* a = (struct TrampolineArgs*)arg;
	verbose && printf("started thread %p %p\n", a->start_routine, a->arg);
	int efd = evl_attach_thread(EVL_CLONE_PRIVATE, "thread:%d:%p:%u", getpid(), a->start_routine, pthread_self());
	if(efd < 0)
		fprintf(stderr, "wrappers: evl_attach_thread failed with %s (%d)\n", strerror(-efd), -efd);
	else {
		int flags = 0;
#ifdef CATCH_MSW
		flags = EVL_T_WOSS | EVL_T_WOLI | EVL_T_WOSX | EVL_T_HMSIG;
#endif // CATCH_MSW
		int ret = evl_set_thread_mode(efd, flags, NULL);
		if(ret)
		{
			fprintf(stderr, "wrappers: evl_set_thread_mode failed with %s (%d)\n", strerror(-ret), -ret);
		}
	}
	void* ret = a->start_routine(a->arg);
	free(arg);
	return ret;
}

WRAPPER_IMPL(int, pthread_create, (pthread_t *restrict thread,
			const pthread_attr_t *restrict attr,
			void *(*start_routine)(void *),
			void *restrict arg))
{
	verbose && printf("wrappers: pthread_create() %p %p\n", start_routine, arg);
	struct TrampolineArgs* ta = malloc(sizeof(struct TrampolineArgs));
	if(!ta) {
		return EAGAIN;
	}
	ta->start_routine = start_routine;
	ta->arg = arg;
	pthread_t ret = __STD(pthread_create)(thread, attr, trampoline, ta);
	verbose && printf("wrappers: pthread_create() %p %p %p => %d\n", thread, start_routine, arg, ret);
	return ret;
}

WRAPPER_IMPL(int, clock_gettime, (clockid_t clockid, struct timespec *tp))
{
	int clockfd = CLOCK_MONOTONIC;
	switch(clockid) {
		case CLOCK_REALTIME:
		case CLOCK_REALTIME_ALARM:
		case CLOCK_REALTIME_COARSE:
			clockfd = EVL_CLOCK_REALTIME;
			break;
		case CLOCK_MONOTONIC:
		case CLOCK_MONOTONIC_RAW:
		case CLOCK_MONOTONIC_COARSE:
			clockfd = EVL_CLOCK_MONOTONIC;
			break;
		default:
			fprintf(stderr, "wrappers: clock_gettime(): unknown clock %d, using defaults\n", clockid);
	}
	return evl_read_clock(clockfd, tp);
}

