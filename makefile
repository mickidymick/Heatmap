all: heatmap
all: test
all: basic_ver

clean:
	rm -f heatmap
	rm -f test
	rm -f basic_ver

heatmap: heatmap.cpp
	g++ -std=c++17 -g -O0 -o heatmap heatmap.cpp

test: test.cpp
	g++ -std=c++17 -g -O0 -o test test.cpp

basic_ver: basic_ver.cpp
	g++ -std=c++17 -g -O0 -o basic_ver basic_ver.cpp


