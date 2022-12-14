CC=gcc
CFLAGS=-O3 -fopenmp -march=native
LDFLAGS=-lrt

all: cma_read cma_write posix_read posix_write stream

cma_read: test_cma_read.c shmem_tests.h
	$(CC) $(CFLAGS) -o $@ $<

cma_write: test_cma_write.c shmem_tests.h
	$(CC) $(CFLAGS) -o $@ $<

posix_read: test_posix_read.c shmem_tests.h
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

posix_write: test_posix_write.c shmem_tests.h
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

stream: simple_stream.c shmem_tests.h
	$(CC) $(CFLAGS) -o $@ $<


.PHONY: clean
	rm -f cma_read cma_write posix_read posix_write stream
