CXX = g++
CXXFLAGS = -std=c++11 -Wall -Wextra
LDFLAGS = -lbfd

OBJ = loader.o loader_demo.o
BIN = loader_demo

.PHONY: all clean

all: $(BIN)

loader.o: loader.cc
	$(CXX) $(CXXFLAGS) -c loader.cc -o loader.o

loader_demo.o: loader_demo.cc
	$(CXX) $(CXXFLAGS) -c loader_demo.cc -o loader_demo.o

$(BIN): $(OBJ)
	$(CXX) $(OBJ) -o $(BIN) $(LDFLAGS)

clean:
	rm -f $(BIN) *.o
