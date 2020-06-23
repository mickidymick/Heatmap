all: heatmap
all: heatmap2
all: test

clean:
	rm -f heatmap
	rm -f heatmap2
	rm -f test

heatmap: heatmap.cpp
	g++ -std=c++17 -g -O0 -o heatmap heatmap.cpp

heatmap2: heatmap_save.cpp
	g++ -std=c++17 -g -O0 -o heatmap2 heatmap_save.cpp

test: test.cpp
	g++ -std=c++17 -g -O0 -o test test.cpp
