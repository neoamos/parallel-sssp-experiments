all: sssp_serial sssp

sssp_serial: sssp_serial.cpp Timer.h simplegraph.h
	g++ -g -O3 $< -o $@

sssp: sssp.cpp Timer.h simplegraph.h
	g++ -g -O3 -std=c++11 -pthread $< -o $@
