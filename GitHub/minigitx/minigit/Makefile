# Linux / macOS / MSYS2.  On plain Windows use build.bat instead.
CXX      ?= g++
CXXFLAGS ?= -std=c++11 -O2 -Wall -Wextra -Iinclude
SRC      := $(wildcard src/*.cpp)
OBJ      := $(SRC:src/%.cpp=build/%.o)

LDLIBS   :=
ifeq ($(OS),Windows_NT)
  LDLIBS += -lws2_32
endif

minigit: $(OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJ) $(LDLIBS)

build/%.o: src/%.cpp | build
	$(CXX) $(CXXFLAGS) -c $< -o $@

build:
	mkdir -p build

clean:
	rm -rf build minigit minigit.exe

.PHONY: clean
