CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -Wpedantic -O2 -Iinclude -D_DEFAULT_SOURCE -D_XOPEN_SOURCE=700
LDFLAGS =

SRCS = src/main.cpp src/tty.cpp src/dict.cpp src/match.cpp src/modem.cpp src/selftest.cpp
OBJS = $(SRCS:src/%.cpp=build/%.o)

.PHONY: all clean test

all: at-server

build/%.o: src/%.cpp
	@mkdir -p build
	$(CXX) $(CXXFLAGS) -c $< -o $@

at-server: $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS) $(LDFLAGS)

test: at-server
	./at-server --self-test -f data/responses.csv

clean:
	rm -rf build at-server
