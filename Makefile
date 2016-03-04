CC=g++

FILES= \
highway_tree_hash256.cc \
highway_tree_hash512.cc \
highway_twisted_hash512.cc \
scalar_highway_tree_hash.cc \
scalar_sip_tree_hash.cc \
sip_hash.cc \
sip_hash_main.cc \
sip_tree_hash.cc

sip_tree_hash: $(FILES)
	$(CC) -std=c++11 -O3 -march=native $(FILES) -o sip_tree_hash

clean:
	rm -f sip_tree_hash
