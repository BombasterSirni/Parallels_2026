
CC = gcc
CFLAGS = -std=c17 -Wall -O2 -fopenmp

NUM_THREADS ?= 40
M_SIZE ?= 40000
N_SIZE ?= 40000

CFLAGS += -DNUM_THREADS=$(NUM_THREADS) \
          -DM_SIZE=$(M_SIZE) \
          -DN_SIZE=$(N_SIZE)


all: dgemv

dgemv: dgemv_task.c
	$(CC) $(CFLAGS) dgemv_task.c -o dgemv

clean:
	rm -f dgemv

