all: heatmap
all: test

clean:
	rm -f heatmap
	rm -f test

heatmap: heatmap.cpp
	g++ -std=c++17 -g -O0 -o heatmap heatmap.cpp

test: test.cpp
	g++ -std=c++17 -g -O0 -o test test.cpp
