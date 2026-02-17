CFLAGS=-pthread -std=gnu99 -g
CPPFLAGS=-I/usr/evl/include -D_GNU_SOURCE

all: libwrappers.so

wrappers-real.o: wrappers.h
wrappers-impl.o: wrappers.h

libwrappers.so: wrappers-impl.o wrappers-real.o
	$(CC) -shared -Wl,-soname,libwrappers.so -o $@ $^ -lpthread -levl -L/usr/evl/lib/aarch64-linux-gnu

clean:
	rm -rf *.o *.so
