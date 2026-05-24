CXX = pgc++
CXXFLAGS = -acc -Minfo=all -std=c++11 -O3 -I$(CONDA_PREFIX)/include

LDFLAGS = -L$(CONDA_PREFIX)/lib -Wl,-rpath,$(CONDA_PREFIX)/lib -lboost_program_options

TARGET = therm
SRC = therm.cpp

all: $(TARGET)

$(TARGET): $(SRC)
    $(CXX) $(CXXFLAGS) $(SRC) -o $(TARGET) $(LDFLAGS)

clean:
    rm -f $(TARGET)