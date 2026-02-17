#include <pthread.h>
#include <sys/time.h>
#include <stddef.h>

#define __WRAP(call)		__wrap_ ## call
#define __STD(call)		__real_ ## call
#define __WRAPPER(call)		__wrapper_ ## call
#define __RT(call)		__WRAPPER(call)
#define WRAPPER_DECL(T, FN, I)                                                  \
	__typeof__(T) __RT(FN) I;                                              \
	__typeof__(T) __STD(FN) I;                                             \
	__typeof__(T) __WRAP(FN) I
#define WRAPPER_IMPL(T, FN, I)                                                  \
	__typeof__(T) __wrap_##FN I                                            \
		__attribute__((alias("__wrapper_" __stringify(FN)), weak));     \
	__typeof__(T) __wrapper_##FN I

#if __USE_TIME_BITS64 && __TIMESIZE == 32
/*
 * Make __RT() and __STD() usable in combination with time64_t related services.
 */
#define COBALT_IMPL_TIME64(T, FN, FN_64, I)                                    \
    __typeof__(T) __wrap_##FN_64 I                                         \
        __attribute__((alias("__wrapper_" __stringify(FN)), weak));     \
    COBALT_IMPL(T, FN, I)
#define COBALT_DECL_TIME64(T, FN, A, I)                                        \
    __typeof__(T) __STD(A) I;                                              \
    extern T __REDIRECT_NTH(__STD(FN), I, __real_##A);                     \
    COBALT_DECL(T, FN, I)
#else
#define COBALT_IMPL_TIME64(T, FN, FN_64, I) COBALT_IMPL(T, FN, I)
#define COBALT_DECL_TIME64(T, FN, FN_64, I) COBALT_DECL(T, FN, I)
#endif


// skipped:  pthread_cond_init ??
WRAPPER_DECL(int, pthread_cond_signal, (pthread_cond_t *cond));

/*
WRAPPER_DECL_TIME64(int, pthread_cond_timedwait, __pthread_cond_timedwait64,
		   (pthread_cond_t *cond, pthread_mutex_t *mutex,
		    const struct timespec *abstime));
		    */

WRAPPER_DECL(int, pthread_cond_wait,
	    (pthread_cond_t *cond, pthread_mutex_t *mutex));

WRAPPER_DECL(int, pthread_cond_destroy, (pthread_cond_t *cond));

WRAPPER_DECL(int, pthread_create,
	    (pthread_t *ptid_r, const pthread_attr_t *attr,
	     void *(*start)(void *), void *arg));

WRAPPER_DECL(int, pthread_join, (pthread_t ptid, void **retval));

WRAPPER_DECL(int, pthread_mutex_init,
	    (pthread_mutex_t *mutex, const pthread_mutexattr_t *attr));

WRAPPER_DECL(int, pthread_mutex_destroy, (pthread_mutex_t *mutex));

WRAPPER_DECL(int, pthread_mutex_lock, (pthread_mutex_t *mutex));

WRAPPER_DECL(int, pthread_mutex_trylock, (pthread_mutex_t *mutex));

WRAPPER_DECL(int, pthread_mutex_unlock, (pthread_mutex_t *mutex));

// WRAPPER_DECL(int, pthread_setname_np, (pthread_t thread, const char *name));

//WRAPPER_DECL(int, pthread_setschedparam,
	    //(pthread_t thread, int policy, const struct sched_param *param));
WRAPPER_DECL(int, clock_gettime, (clockid_t clockid, struct timespec *tp));
