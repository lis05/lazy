CXX=g++
CC=gcc
LD=g++

CXXFLAGS=-O3 -std=c++23 -MMD -MP -Ithird_party/finitestateentropy/lib
CFLAGS=-O3 -std=gnu11 -Ithird_party/finitestateentropy/lib
LDFLAGS=
LDLIBS=

TARGET=build/lz77

SOURCES=$(wildcard src/*.cpp)
FSE_SOURCES=$(wildcard third_party/finitestateentropy/lib/*.c)

OBJECTS=$(SOURCES:src/%.cpp=obj/%.o)
FSE_OBJECTS=$(FSE_SOURCES:third_party/finitestateentropy/lib/%.c=obj/fse_%.o)

ALL_OBJECTS=$(OBJECTS) $(FSE_OBJECTS)

all: $(TARGET)

$(TARGET): $(ALL_OBJECTS) | build
	$(LD) $(LDFLAGS) $^ -o $@ $(LDLIBS)

obj/%.o: src/%.cpp | obj
	$(CXX) $(CXXFLAGS) -c $< -o $@

obj/fse_%.o: third_party/finitestateentropy/lib/%.c | obj
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
