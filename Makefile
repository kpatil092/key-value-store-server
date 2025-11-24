CXX = g++
FLAGS = -std=c++17 -O2 -pthread
INCLUDES = -Iinclude
PG_INCLUDES = -I/usr/include/postgresql
LIBS = -lpq

SERVER_SRC = $(wildcard ./server/*.cpp)
LOADGEN_SRC = ./client/load_generator.cpp

SERVER_OUT = server.out
LOADGEN_OUT = load_generator.out

all: $(SERVER_OUT) $(LOADGEN_OUT)

# Build server
$(SERVER_OUT): $(SERVER_SRC) 
	$(CXX) $(FLAGS) $(INCLUDES) $(PG_INCLUDES) $(SERVER_SRC) -o $(SERVER_OUT) $(LIBS)

# Build load generator
$(LOADGEN_OUT): $(LOADGEN_SRC) 
	$(CXX) $(FLAGS) $(INCLUDES) $(LOADGEN_SRC) -o $(LOADGEN_OUT)

clean: 
	rm -f $(SERVER_OUT) $(LOADGEN_OUT)
