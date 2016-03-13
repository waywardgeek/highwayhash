#include <cstdio>
#include <cstring>

#include "code_annotation.h"
#include "highway_tree_hash.h"
#include "highway_tree_hash512.h"

void printStats(uint32_t in_bytes, uint32_t hashbits, uint32_t inbits, uint32_t offset, uint32_t *counts) {
  printf("*********************** Input size: %u, In bits: %u, Offset: %u\n", in_bytes, inbits, offset*8);
  for (uint32_t inpos = 0; inpos < inbits; inpos++) {
    double total_bias = 0.0;
    float max_bias = 0.0f;
    for (uint32_t hashpos = 0; hashpos < hashbits; ++hashpos) {
      float bias = 200.0f*(0.5f -
          counts[inpos*hashbits + hashpos]/float(1 << (inbits - 1)));
      if(bias < 0.0f) {
          bias = -bias;
      }
      if (bias > max_bias) {
          max_bias = bias;
      }
      total_bias += bias;
    }
    float ave_bias = total_bias / hashbits;
    printf("inpos %u: max bias = %f%%, ave bias = %f%%\n", inpos, max_bias, ave_bias);
  }
}

int main(int argc, char* argv[]) {
  ALIGNED(uint64_t, 64) key[4] = {0,};
  ALIGNED(uint8_t, 64) in[512] = {0,};
  const size_t hashbits = 64;
  const size_t inbits = 20;
  uint32_t *counts = new uint32_t[inbits * hashbits];
  //uint32_t sizes[3] = {3, 64, 512};
  uint32_t sizes[3] = {3, 16, 32};
  for (uint32_t i = 0; i < 3; ++i) {
    uint32_t in_bytes = sizes[i];
    for (uint32_t offset = 0; offset < in_bytes; ++offset) {
      memset(counts, 0, inbits * hashbits * sizeof(uint32_t));
      for (uint32_t inval = 0; inval < 1 << inbits; ++inval) {
        memset(in, 0, sizeof(in));
        memcpy(in + offset, &inval, sizeof(uint32_t));
        //uint64_t hash1 = HighwayTreeHash512(key, in, in_bytes + offset);
        int64_t hash1 = HighwayTreeHash(key, in, in_bytes + offset);
        for (uint32_t inpos = 0; inpos < inbits; ++inpos) {
           // Only check for 1->0 transitions
           if (inval & (1 << inpos)) {
             uint32_t value = inval ^ (1 << inpos);
             memcpy(in + offset, &value, sizeof(uint32_t));
             //uint64_t hash2 = HighwayTreeHash512(key, in, in_bytes + offset);
             uint64_t hash2 = HighwayTreeHash(key, in, in_bytes + offset);
             for (uint32_t hashpos = 0; hashpos < hashbits; ++hashpos) {
               uint32_t mask = 1 << hashpos;
               if ((hash2 & mask) != (hash1 & mask)) {
                 counts[inpos*hashbits + hashpos]++;
               }
             }
           }
        }
      }
      printStats(in_bytes, hashbits, inbits, offset, counts);
    }
  }
  return 0;
}