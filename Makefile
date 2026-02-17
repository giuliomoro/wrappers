CFLAGS=-pthread -std=gnu99 -g
CPPFLAGS=-I/usr/evl/include -D_GNU_SOURCE
EVL_LDFLAGS=-L /usr/evl/lib/*/
EVL_LDLIBS=-levl

WRAP_LDFLAGS=-L. -Wl,@evl.wrappers $(EVL_LDFLAGS)
WRAP_LDLIBS=-lwrappers $(EVL_LDLIBS)

lib: libwrappers.so

wrappers-real.o: wrappers.h
wrappers-impl.o: wrappers.h

libwrappers.so: wrappers-impl.o wrappers-real.o
	$(CC) -shared -Wl,-soname,libwrappers.so -o $@ $^ -lpthread -levl -L/usr/evl/lib/aarch64-linux-gnu

clean:
	rm -rf *.o *.so test/test-two-conds-evl test/test-two-conds-wrapped test/test-two-conds-posix

.PHONY: test
test: test/test-two-conds-wrapped test/test-two-conds-evl test/test-two-conds-posix
test: test/test-two-threads-one-cond-wrapped test/test-two-threads-one-cond-evl test/test-two-threads-one-cond-posix

test/test-two-threads-one-cond-wrapped: test/test-two-threads-one-cond.c libwrappers.so
	$(CC) $(WRAP_LDFLAGS) $(CFLAGS) $(CPPFLAGS) $^ -o $@ $(WRAP_LDLIBS) -DUSE_WRAPPERS

test/test-two-threads-one-cond-posix: test/test-two-threads-one-cond.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $^ -o $@ -lpthread

test/test-two-threads-one-cond-evl: test/test-two-threads-one-cond-evl.c
	$(CC) $(EVL_LDFLAGS) $(CFLAGS) $(CPPFLAGS) $^ -o $@ $(EVL_LDLIBS) -lpthread

test/test-two-conds-wrapped: test/test-two-conds.c libwrappers.so
	$(CC) $(WRAP_LDFLAGS) $(CFLAGS) $(CPPFLAGS) $^ -o $@ $(WRAP_LDLIBS) -DUSE_WRAPPERS

test/test-two-conds-posix: test/test-two-conds.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $^ -o $@ -lpthread

test/test-two-conds-evl: test/test-two-conds-evl.c
	$(CC) $(EVL_LDFLAGS) $(CFLAGS) $(CPPFLAGS) $^ -o $@ $(EVL_LDLIBS) -lpthread
