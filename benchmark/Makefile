CC=gcc
CFLAGS=-Wall -O3 -I../

all: ptbenchmark uvbenchmark atbenchmark

ptbenchmark:
	$(CC) $(CFLAGS) -o ptbenchmark ../prwlock.c benchmark.c -lpthread

uvbenchmark:
	$(CC) $(CFLAGS) -DUSE_LIBUV_RWLOCK -o uvbenchmark ../prwlock.c benchmark.c -luv

atbenchmark:
	$(CC) $(CFLAGS) -DUSE_ATOMICS -o atbenchmark ../prwlock.c benchmark.c

clean:
	rm ptbenchmark uvbenchmark atbenchmark
