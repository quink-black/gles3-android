CXXFLAGS := -g -O0 -std=c++11 -I.
#CXXFLAGS += -fsanitize=address
LDFLAGS := $(shell pkg-config --libs glfw3 glesv2 egl) -pthread

all: main
	./main

main: image.cpp opengl-helper.cpp perf-monitor.cpp tonemap.cpp linux-main.cpp
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

clean:
	rm -f main

.PHONY: all clean
