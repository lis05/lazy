MAKEFLAGS := --jobs=$(shell nproc)

CXX=g++
CC=gcc
LD=g++

INCLUDES=-Ithird_party/
CXXFLAGS=-Oz -ffunction-sections -fdata-sections -flto -fno-ident -fno-asynchronous-unwind-tables -march=native -std=c++23 -MMD -MP $(INCLUDES)
CFLAGS=-Oz -ffunction-sections -fdata-sections -flto -fno-ident -fno-asynchronous-unwind-tables -std=gnu11 $(INCLUDES)
#CXXFLAGS=-O3 -g -ftree-vectorize -march=native -std=c++23 -MMD -MP $(INCLUDES)
#CFLAGS=-O3 -g -ftree-vectorize -march=native -std=gnu11 $(INCLUDES)
LDFLAGS=-s -flto -Wl,--gc-sections
LDLIBS=

TARGET=build/lzmpo

SOURCES=$(wildcard src/*.cpp)

OBJECTS=$(SOURCES:src/%.cpp=obj/%.o)
TURBO_OBJECTS=$(filter-out third_party/turborc/turborc.o, $(wildcard third_party/turborc/*.o))
TURBO_OBJECTS += $(wildcard third_party/turborc/libsais/src/*.o)

ALL_OBJECTS=$(OBJECTS) $(TURBO_OBJECTS)

all: $(TARGET)

$(TARGET): $(ALL_OBJECTS) | build
	$(LD) $(LDFLAGS) $^ -o $@ $(LDLIBS)

obj/%.o: src/%.cpp | obj
	$(CXX) $(CXXFLAGS) -c $< -o $@

obj/turbo_%.o: third_party/turborc/%.c | obj
	$(CC) $(CFLAGS) -c $< -o $@

-include $(ALL_OBJECTS:.o=.d)

build:
	mkdir -p $@

obj:
	mkdir -p $@

clean:
	rm -rf obj build

debug:
	@echo SOURCES=$(SOURCES)
	@echo FSE_SOURCES=$(FSE_SOURCES)
	@echo ALL_OBJECTS=$(ALL_OBJECTS)

.PHONY: all clean debug
