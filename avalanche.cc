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
  //ALIGNED(uint64_t, 64) key[8] = {0,};
  ALIGNED(uint64_t, 64) key[4] = {0,};
  ALIGNED(uint8_t, 64) in[512] = {0,};
  //const size_t hashbits = 512;
  const size_t hashbits = 64;
  const size_t inbits = 24;
  uint32_t *counts = new uint32_t[inbits * hashbits];
  uint32_t sizes[3] = {3, 64, 512};
  //uint32_t sizes[3] = {3, 16, 32};
  for (uint32_t i = 0; i < 3; ++i) {
    uint32_t in_bytes = sizes[i];
    for (uint32_t offset = 0; offset < in_bytes; ++offset) {
      memset(counts, 0, inbits * hashbits * sizeof(uint32_t));
      for (uint32_t inval = 0; inval < 1 << inbits; ++inval) {
        memset(in, 0, sizeof(in));
        memcpy(in + offset, &inval, sizeof(uint32_t));
        ALIGNED(uint64_t, 64) hash1[8] = {0,};
        //HighwayTreeHash512(key, in, in_bytes + offset, hash1);
        hash1[0] = HighwayTreeHash(key, in, in_bytes + offset);
        for (uint32_t inpos = 0; inpos < inbits; ++inpos) {
           // Only check for 1->0 transitions
           if (inval & (1 << inpos)) {
             uint32_t value = inval ^ (1 << inpos);
             memcpy(in + offset, &value, sizeof(uint32_t));
             ALIGNED(uint64_t, 64) hash2[8] = {0,};
             //HighwayTreeHash512(key, in, in_bytes + offset, hash2);
             hash2[0] = HighwayTreeHash(key, in, in_bytes + offset);
             for (uint32_t hashpos = 0; hashpos < hashbits; ++hashpos) {
               uint32_t index = hashpos/64;
               uint32_t mask = 1 << (hashpos % 64);
               if ((hash2[index] & mask) != (hash1[index] & mask)) {
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
