CC=g++

FILES= \
hwsum.cc

HEADERS= \
hwhash.h \
vec2.h

all:hwsum

hwsum: $(FILES) $(HEADERS)
	$(CC) -Wall -std=c++11 -O3 -march=native $(FILES) -o hwsum

clean:
	rm -f hwsum
