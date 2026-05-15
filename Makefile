CXX=g++
LD=g++

CXXFLAGS=-O0 -g -std=c++23 -MMD -MP
LDFLAGS=-g
LDLIBS=

TARGET=build/lz77

SOURCES=$(wildcard src/*.cpp)
OBJECTS=$(SOURCES:src/%.cpp=obj/%.o)

all: $(TARGET)

$(TARGET): $(OBJECTS) | build
	$(LD) $(LDFLAGS) $^ -o $@ $(LDLIBS)

obj/%.o: src/%.cpp | obj
	$(CXX) $(CXXFLAGS) -c $< -o $@

-include $(OBJECTS:.o=.d)

build:
	mkdir -p $@

obj:
	mkdir -p $@

clean:
	rm -rf obj build

debug:
	@echo SOURCES=$(SOURCES)
	@echo OBJECTS=$(OBJECTS)

.PHONY: all clean debug
