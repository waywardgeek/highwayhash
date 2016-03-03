FILES= \
highway_tree_hash.cc \
scalar_highway_tree_hash.cc \
scalar_sip_tree_hash.cc \
sip_hash.cc \
sip_hash_main.cc \
sip_tree_hash.cc

sip_tree_hash: $(FILES)
	g++ -std=c++11 -O3 -march=native $(FILES) -o sip_tree_hash
