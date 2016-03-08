// Copyright 2015 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "highway_tree_hash512.h"

#include <cstring>  // memcpy
#include <stdio.h>
#include "vec_scalar.h"

namespace {

// J-lanes tree hashing: see http://dx.doi.org/10.4236/jis.2014.53010

// Four (2 x 64-bit) hash states are updated in parallel by injecting
// four 64-bit packets per Update(). Finalize() combines the four states into
// one final 64-bit digest.
const int kNumLanes = 4;
const int kGraphSize = 8;
const int kBlockSize = 2 * kNumLanes * sizeof(uint64_t);
const int kPacketSize = kGraphSize * kBlockSize;

class HighwayTreeHashState512 {
 public:
  explicit INLINE HighwayTreeHashState512(const uint64_t (&keys)[kNumLanes]) {
    init0 = V4x64U(0x243f6a8885a308d3ull, 0x13198a2e03707344ull,
        0xa4093822299f31d0ull, 0xdbe6d5d5fe4cce2full);
    init1 = V4x64U(0x452821e638d01377ull, 0xbe5466cf34e90c6cull,
        0xc0acf169b5f18a8cull, 0x3bd39e10cb0ef593ull);
    const V4x64U key = LoadU(keys);
    // TODO: find better numbers for v2, v3.
    v0 = init0;
    v1 = key ^ init1;
    v2 = init0;
    v3 = init1;
  }

  INLINE V4x64U Permute(const V4x64U& val) {
    // For complete mixing, we need to swap the upper and lower 128-bit halves;
    // we also swap all 32-bit halves.
    // Slightly better to permute v0 than v1; it will be added to v1.
    V4x64U permuted;
    uint64_t v = val.v_[0];
    permuted.v_[2] = v >> 32 | v << 32;
    v = val.v_[1];
    permuted.v_[3] = v >> 32 | v << 32;
    v = val.v_[2];
    permuted.v_[0] = v >> 32 | v << 32;
    v = val.v_[3];
    permuted.v_[1] = v >> 32 | v << 32;
    return permuted;
  }

  inline V4x64U Multiply(const V4x64U& a, const V4x64U& b) {
      V4x64U r;
      r.v_[0] = uint64_t(uint32_t(a.v_[0]))*uint32_t(b.v_[0]);
      r.v_[1] = uint64_t(uint32_t(a.v_[1]))*uint32_t(b.v_[1]);
      r.v_[2] = uint64_t(uint32_t(a.v_[2]))*uint32_t(b.v_[2]);
      r.v_[3] = uint64_t(uint32_t(a.v_[3]))*uint32_t(b.v_[3]);
      return r;
  }

  inline void Update(const V4x64U& packet1, const V4x64U& packet2) {
    V4x64U mul0(Multiply(v0, Permute(v2)));
    V4x64U mul1(Multiply(v1, Permute(v3)));
    V4x64U mul2(Multiply(v0 >> 32, v2));
    V4x64U mul3(Multiply(v1 >> 32, v3));

    const V4x64U mask(0x00ff00ff00ff00ffull);
    v0 ^= packet1 & mask;
    v1 ^= AndNot(mask, packet1);
    v2 ^= packet2 & mask;
    v3 ^= AndNot(mask, packet2);
    v0 ^= ZipperMerge(mul1);
    v1 ^= ZipperMerge(mul0);
    v2 ^= ZipperMerge(mul3);
    v3 ^= ZipperMerge(mul2);
  }

  INLINE void UpdatePacket(const uint64_t *packets) {
    V4x64U packet1 = LoadU(packets + 0*4);
    V4x64U packet2 = LoadU(packets + 1*4);
    Update(packet1, packet2);
    packet1 = LoadU(packets + 2*4);
    packet2 = LoadU(packets + 3*4);
    Update(packet1, packet2);
    packet1 = LoadU(packets + 4*4);
    packet2 = LoadU(packets + 5*4);
    Update(packet1, packet2);
    packet1 = LoadU(packets + 6*4);
    packet2 = LoadU(packets + 7*4);
    Update(packet1, packet2);
    packet1 = LoadU(packets + 8*4);
    packet2 = LoadU(packets + 9*4);
    Update(packet1, packet2);
    packet1 = LoadU(packets + 10*4);
    packet2 = LoadU(packets + 11*4);
    Update(packet1, packet2);
    packet1 = LoadU(packets + 12*4);
    packet2 = LoadU(packets + 13*4);
    Update(packet1, packet2);
    packet1 = LoadU(packets + 14*4);
    packet2 = LoadU(packets + 15*4);
    Update(packet1, packet2);
    packet1 = LoadU(packets + 8*4);
    packet2 = LoadU(packets + 9*4);
    Update(packet1, packet2);
    packet1 = LoadU(packets + 2*4);
    packet2 = LoadU(packets + 3*4);
    Update(packet1, packet2);
    packet1 = LoadU(packets + 12*4);
    packet2 = LoadU(packets + 13*4);
    Update(packet1, packet2);
    packet1 = LoadU(packets + 6*4);
    packet2 = LoadU(packets + 7*4);
    Update(packet1, packet2);
    packet1 = LoadU(packets + 0*4);
    packet2 = LoadU(packets + 1*4);
    Update(packet1, packet2);
    packet1 = LoadU(packets + 10*4);
    packet2 = LoadU(packets + 11*4);
    Update(packet1, packet2);
    packet1 = LoadU(packets + 4*4);
    packet2 = LoadU(packets + 5*4);
    Update(packet1, packet2);
    packet1 = LoadU(packets + 14*4);
    packet2 = LoadU(packets + 15*4);
    Update(packet1, packet2);
  }

  INLINE uint64_t Finalize() {
    // Mix together all lanes.
    Update(v0, v2);
    Update(v1, v3);
    Update(v2, v0);
    Update(v3, v1);

    return v0.v_[0] + v1.v_[0];
  }

  static INLINE V4x64U ZipperMerge(const V4x64U& val) {
    const uint8_t *v = reinterpret_cast<const uint8_t*>(val.v_);
    V4x64U res;
    uint8_t *r = reinterpret_cast<uint8_t*>(res.v_);
    for (int half = 0; half < 32; half += 32 / 2) {
      r[half + 0] = v[half + 3];
      r[half + 1] = v[half + 12];
      r[half + 2] = v[half + 2];
      r[half + 3] = v[half + 5];
      r[half + 4] = v[half + 14];
      r[half + 5] = v[half + 1];
      r[half + 6] = v[half + 15];
      r[half + 7] = v[half + 0];
      r[half + 8] = v[half + 11];
      r[half + 9] = v[half + 4];
      r[half + 10] = v[half + 10];
      r[half + 11] = v[half + 13];
      r[half + 12] = v[half + 9];
      r[half + 13] = v[half + 6];
      r[half + 14] = v[half + 8];
      r[half + 15] = v[half + 7];
    }
    return res;
  }

  void print() {
    printf("v0 = %016lx%016lx%016lx%016lx\n", v0.v_[3], v0.v_[2], v0.v_[1], v0.v_[0]);
    printf("v1 = %016lx%016lx%016lx%016lx\n", v1.v_[3], v1.v_[2], v1.v_[1], v1.v_[0]);
    printf("v2 = %016lx%016lx%016lx%016lx\n", v2.v_[3], v2.v_[2], v2.v_[1], v2.v_[0]);
    printf("v3 = %016lx%016lx%016lx%016lx\n", v3.v_[3], v3.v_[2], v3.v_[1], v3.v_[0]);
  }

  V4x64U init0;
  V4x64U init1;
  V4x64U v0;
  V4x64U v1;
  V4x64U v2;
  V4x64U v3;
};

}  // namespace

uint64_t ScalarHighwayTreeHash512(const uint64_t (&key)[4], const uint8_t* bytes,
                         const uint64_t size) {
  HighwayTreeHashState512 state(key);

  const size_t remainder = size & (kPacketSize - 1);
  const size_t truncated_size = size - remainder;
  const uint64_t* packets = reinterpret_cast<const uint64_t*>(bytes);
  uint32_t i; //  Used later so declared outside for loop.
  for (i = 0; i < truncated_size / sizeof(uint64_t); i += kPacketSize / sizeof(uint64_t)) {
    state.UpdatePacket(packets + i);
  }

  if (remainder > kPacketSize / 2) {
      // Do a full twisted round padding with 0's in this case.
      uint64_t final_packet[kPacketSize / sizeof(uint64_t)] = {0,};
      memcpy(final_packet, packets + i, remainder);
      state.UpdatePacket(final_packet);
  } else if (remainder > 0) {
      // It is faster to do 4-rounds of Update in this case.
      const size_t block_remainder = size & (kBlockSize - 1);
      const size_t stop = (size - block_remainder) / sizeof(uint64_t);
      for (; i < stop; i += kBlockSize) {
          const V4x64U packet1 = LoadU(packets + i);
          const V4x64U packet2 = LoadU(packets + i + 4);
          state.Update(packet1, packet2);
          state.Update(packet1, packet2);
          state.Update(packet1, packet2);
          state.Update(packet1, packet2);
      }
      if (block_remainder > 0) {
          // Final block padded with 0's.
          uint64_t final_block[kBlockSize / sizeof(uint64_t)] = {0,};
          memcpy(final_block, packets + i, block_remainder);
          const V4x64U packet1 = LoadU(final_block);
          const V4x64U packet2 = LoadU(final_block + 4);
          state.Update(packet1, packet2);
          state.Update(packet1, packet2);
          state.Update(packet1, packet2);
          state.Update(packet1, packet2);
      }
  }
  return state.Finalize();
}
