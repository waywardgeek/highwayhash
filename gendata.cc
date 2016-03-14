#include <cstdio>
#include <cstring>

#include "code_annotation.h"
#include "highway_tree_hash.h"
#include "highway_tree_hash512.h"

int main(int argc, char* argv[]) {
  ALIGNED(uint64_t, 64) key[8] = {1,};
  ALIGNED(uint8_t, 64) in[512] = {0,};
  uint64_t inval = 0;
  while (true) {
    memcpy(in, &inval, sizeof(uint32_t));
    ALIGNED(uint64_t, 64) hash[8];
    HighwayTreeHash512(key, in, 512, hash);
    fwrite(hash, sizeof(uint64_t), 8, stdout);
    inval++;
  }
  return 0;
}
