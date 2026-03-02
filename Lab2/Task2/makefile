CC = gcc
CFLAGS = -std=c17 -Wall -O2 -fopenmp
LDFLAGS = -lm

NUM_THREADS ?= 40
CFLAGS += -DNUM_THREADS=$(NUM_THREADS)

all: integral

integral: integral.c
	$(CC) $(CFLAGS) integral.c -o integral $(LDFLAGS)

clean:
	rm -f integral