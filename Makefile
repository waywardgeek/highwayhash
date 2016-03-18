CC=g++

FILES= \
highway_tree_hash.cc \
highway_tree_hash512.cc \
river.cc \
scalar_highway_tree_hash.cc \
scalar_highway_tree_hash512.cc \
scalar_sip_tree_hash.cc \
sip_hash.cc \
sip_hash_main.cc \
sip_tree_hash.cc

HEADERS= \
code_annotation.h \
highway_tree_hash.h \
highway_tree_hash512.h \
river.h \
scalar_highway_tree_hash.h \
scalar_highway_tree_hash512.h \
scalar_sip_tree_hash.h \
sip_hash.h \
sip_tree_hash.h \
vec.h \
vec2.h \
vec_scalar.h

all: sip_tree_hash river avalanche gendata test

test: test.cc
	$(CC) -Wall -std=c++11 -O2 -march=native test.cc -o test

sip_tree_hash: $(FILES) $(HEADERS)
	$(CC) -Wall -std=c++11 -O3 -march=native $(FILES) -o sip_tree_hash

river: river.cc river.h river_main.cc
	$(CC) -Wall -std=c++11 -O3 -march=native river.cc river_main.cc -o river

avalanche: avalanche.cc highway_tree_hash512.cc highway_tree_hash512.h highway_tree_hash.cc highway_tree_hash.h
	$(CC) -Wall -std=c++11 -O3 -march=native avalanche.cc highway_tree_hash.cc highway_tree_hash512.cc -o avalanche

gendata: gendata.cc river.cc river.h highway_tree_hash512.cc highway_tree_hash512.h
	$(CC) -Wall -std=c++11 -O3 -march=native gendata.cc river.cc highway_tree_hash512.cc -o gendata

clean:
	rm -f sip_tree_hash river avalanche gendata
