MAKEFLAGS := --jobs=$(shell nproc)

MODE ?= release

CXX := g++
CC  := gcc
LD  := g++

ifeq ($(MODE), debug)
    CXXFLAGS := -O0 -g -ftree-vectorize -march=native -std=c++23 -MMD -MP -Ithird_party/
    CFLAGS   := -O0 -g -ftree-vectorize -march=native -std=gnu11 -Ithird_party/
    LDFLAGS  := -g
else
    CXXFLAGS := -Oz -ffunction-sections -fdata-sections -flto -fno-ident -fno-asynchronous-unwind-tables -march=native -std=c++23 -MMD -MP -Ithird_party/
    CFLAGS   := -Oz -ffunction-sections -fdata-sections -flto -fno-ident -fno-asynchronous-unwind-tables -std=gnu11 -Ithird_party/
    LDFLAGS  := -s -flto -Wl,--gc-sections
endif

TARGET := build/lzmpo
SOURCES := $(wildcard src/*.cpp)
OBJECTS := $(SOURCES:src/%.cpp=obj/%.o)
TURBO_SOURCES := $(filter-out third_party/turborc/turborc.o, $(wildcard third_party/turborc/*.o))
TURBO_SOURCES += $(wildcard third_party/turborc/libsais/src/*.o)
ALL_OBJECTS := $(OBJECTS) $(TURBO_SOURCES)

all: $(TARGET)

$(TARGET): $(ALL_OBJECTS) | build
	$(LD) $(LDFLAGS) $^ -o $@

obj/%.o: src/%.cpp | obj
	$(CXX) $(CXXFLAGS) -c $< -o $@

obj/turbo_%.o: third_party/turborc/%.c | obj
	$(CC) $(CFLAGS) -c $< -o $@

build:
	mkdir -p $@

obj:
	mkdir -p $@

clean:
	rm -rf obj build

-include $(ALL_OBJECTS:.o=.d)

.PHONY: all clean
