CC=g++

FILES= \
highway_tree_hash.cc \
highway_tree_hash512.cc \
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
scalar_highway_tree_hash.h \
scalar_highway_tree_hash512.h \
scalar_sip_tree_hash.h \
sip_hash.h \
sip_tree_hash.h \
vec.h \
vec2.h \
vec_scalar.h

sip_tree_hash: $(FILES) $(HEADERS)
	$(CC) -Wall -std=c++11 -O3 -march=native $(FILES) -o sip_tree_hash

clean:
	rm -f sip_tree_hash
