CXX     = g++
CXXFLAGS= -std=c++17 -Wall -O2 -fopenmp
LDFLAGS = -lm

NUM_THREADS ?= 40      # количество потоков
N_SIZE      ?= 10000   # размер системы
MAX_ITER    ?= 1000    # максимум итераций
EPS         ?= 1e-6    # точность

CXXFLAGS += -DNUM_THREADS=$(NUM_THREADS) \
            -DN_SIZE=$(N_SIZE) \
            -DMAX_ITER=$(MAX_ITER) \
            -DEPS=$(EPS)


all: richard

jacobi: richard.cpp
	$(CXX) $(CXXFLAGS) richard.cpp -o richard $(LDFLAGS)

clean:
	rm -f richard
