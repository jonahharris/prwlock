CC=gcc
CFLAGS=-I../

all: ptbenchmark uvbenchmark

ptbenchmark:
	$(CC) $(CFLAGS) -o ptbenchmark ../prwlock.c benchmark.c -lpthread

uvbenchmark:
	$(CC) $(CFLAGS) -DUSE_LIBUV_RWLOCK -o uvbenchmark ../prwlock.c benchmark.c -luv

clean:
	rm ptbenchmark uvbenchmark
