.PHONY: all

simulation: main.cpp
	g++ -std=c++11 -o $@ $^ -pthread

all: simulation
