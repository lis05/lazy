CXX=g++
CC=gcc
LD=g++

INCLUDES=-Ithird_party/
CXXFLAGS=-O3 -g -ftree-vectorize -march=native -std=c++23 -MMD -MP $(INCLUDES)
CFLAGS=-O3 -g -ftree-vectorize -march=native -std=gnu11 $(INCLUDES)
LDFLAGS=-g
LDLIBS=

TARGET=build/lazy

SOURCES=$(wildcard src/*.cpp)
FSE_SOURCES=$(wildcard third_party/finitestateentropy/lib/*.c)

OBJECTS=$(SOURCES:src/%.cpp=obj/%.o)
FSE_OBJECTS=$(FSE_SOURCES:third_party/finitestateentropy/lib/%.c=obj/fse_%.o)
TURBO_OBJECTS=$(filter-out third_party/turborc/turborc.o, $(wildcard third_party/turborc/*.o))
TURBO_OBJECTS += $(wildcard third_party/turborc/libsais/src/*.o)

ALL_OBJECTS=$(OBJECTS) $(FSE_OBJECTS) $(TURBO_OBJECTS)

all: $(TARGET)

$(TARGET): $(ALL_OBJECTS) | build
	$(LD) $(LDFLAGS) $^ -o $@ $(LDLIBS)

obj/%.o: src/%.cpp | obj
	$(CXX) $(CXXFLAGS) -c $< -o $@

obj/fse_%.o: third_party/finitestateentropy/lib/%.c | obj
	$(CC) $(CFLAGS) -c $< -o $@

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
