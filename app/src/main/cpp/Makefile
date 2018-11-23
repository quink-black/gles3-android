CXXFLAGS := -g -O0 -std=c++11 -I.
#CXXFLAGS += -fsanitize=address
LDFLAGS := $(shell pkg-config --libs glfw3 glesv2 egl) -pthread

BUILD_DIR := build
OBJ := $(BUILD_DIR)/image.o \
	$(BUILD_DIR)/opengl-helper.o \
	$(BUILD_DIR)/perf-monitor.o \
	$(BUILD_DIR)/tonemap.o \
	$(BUILD_DIR)/linux-main.o

HEADER := hable-frag-1.h hable-frag-2.h hable-vertex.h \
	plain-frag-1.h plain-frag-2.h plain-vertex.h \
	stb_image.h tinyexr.h \
	tonemap.h image.h log.h opengl-helper.h perf-monitor.h \

all: build build/.sum main

build:
	mkdir $@

build/.sum: Makefile $(HEADER)
	touch *.cpp
	touch build/.sum

main: $(OBJ)
	$(CXX) $(CXXFLAGS) $(OBJ) -o $@ $(LDFLAGS)

%.o: ../%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f main
	rm -rf build

.PHONY: all clean
