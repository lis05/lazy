MAKEFLAGS := --jobs=$(shell nproc)

MODE ?= release

CXX := g++
CC  := gcc
LD  := g++

ifeq ($(MODE), debug)
    CXXFLAGS := -DLZMPODEBUG -O0 -g -ftree-vectorize -march=native -std=c++23 -MMD -MP -Ithird_party/
    CFLAGS   := -DLZMPODEBUG -O0 -g -ftree-vectorize -march=native -std=gnu11 -Ithird_party/
    LDFLAGS  := -g
else ifeq ($(MODE), size)
    CXXFLAGS := -Oz -ffunction-sections -fdata-sections -flto -fno-ident -fno-asynchronous-unwind-tables -march=native -std=c++23 -MMD -MP -Ithird_party/
    CFLAGS   := -Oz -ffunction-sections -fdata-sections -flto -fno-ident -fno-asynchronous-unwind-tables -std=gnu11 -Ithird_party/
    LDFLAGS  := -s -flto -Wl,--gc-sections
else ifeq ($(MODE), speed)
    CXXFLAGS := -O3 -ffunction-sections -fdata-sections -flto -fno-ident -fno-asynchronous-unwind-tables -march=native -std=c++23 -MMD -MP -Ithird_party/
    CFLAGS   := -O3 -ffunction-sections -fdata-sections -flto -fno-ident -fno-asynchronous-unwind-tables -march=native -std=gnu11 -Ithird_party/
    LDFLAGS  := -flto -Wl,--gc-sections
else
    CXXFLAGS := -O3 -ffunction-sections -fdata-sections -flto -fno-ident -fno-asynchronous-unwind-tables -march=x86-64-v3 -std=c++23 -MMD -MP -Ithird_party/
    CFLAGS   := -O3 -ffunction-sections -fdata-sections -flto -fno-ident -fno-asynchronous-unwind-tables -march=x86-64-v3 -std=gnu11 -Ithird_party/
    LDFLAGS  := -flto -Wl,--gc-sections

endif

TARGET := build/lzmpo

SOURCES := $(wildcard src/*.cpp)
FSE_SOURCES=$(wildcard third_party/fse/lib/*.c)

OBJECTS := $(SOURCES:src/%.cpp=obj/%.o)
TURBO_OBJECTS := $(filter-out third_party/turborc/turborc.o, $(wildcard third_party/turborc/*.o))
TURBO_OBJECTS += $(wildcard third_party/turborc/libsais/src/*.o)
FSE_OBJECTS=$(FSE_SOURCES:third_party/fse/lib/%.c=obj/fse_%.o)

ALL_OBJECTS := $(OBJECTS) $(TURBO_OBJECTS) $(FSE_OBJECTS)

all: $(TARGET)

$(TARGET): $(ALL_OBJECTS) | build
	$(LD) $(LDFLAGS) $^ -o $@

obj/%.o: src/%.cpp | obj
	$(CXX) $(CXXFLAGS) -c $< -o $@

obj/turbo_%.o: third_party/turborc/%.c | obj
	$(CC) $(CFLAGS) -c $< -o $@

obj/fse_%.o: third_party/fse/lib/%.c | obj
	$(CC) $(CFLAGS) -c $< -o $@

build:
	mkdir -p $@

obj:
	mkdir -p $@

clean:
	rm -rf obj build

-include $(ALL_OBJECTS:.o=.d)

.PHONY: all clean
