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
#include "vec2.h"

namespace {

// J-lanes tree hashing: see http://dx.doi.org/10.4236/jis.2014.53010

// Four (2 x 64-bit) hash states are updated in parallel by injecting
// four 64-bit packets per Update(). Finalize() combines the four states into
// one final 64-bit digest.
const int kNumLanes = 4;
const int kBlockShift = 6;
const int kBlockSize = 1 << kBlockShift;  // 64
const int kPacketShift = 9;
const int kPacketSize = 1 << kPacketShift;  // 512

class HighwayTreeHashState512 {
 public:
  explicit INLINE HighwayTreeHashState512(const uint64_t (&keys)[kNumLanes]) {
    const V4x64U init0(0x243f6a8885a308d3ull, 0x13198a2e03707344ull,
        0xa4093822299f31d0ull, 0xdbe6d5d5fe4cce2full);
    const V4x64U init1(0x452821e638d01377ull, 0xbe5466cf34e90c6cull,
        0xc0acf169b5f18a8cull, 0x3bd39e10cb0ef593ull);
    const V4x64U key = LoadU(keys);
    // TODO: find better numbers for v2, v3.
    v0 = init0 ^ key;
    v1 = init1;
    v2 = init0 + init1;
    v3 = init0 ^ init1;
  }

  INLINE V4x64U Permute(const V4x64U& val) {
    // For complete mixing, we need to swap the upper and lower 128-bit halves;
    // we also swap all 32-bit halves.
    const V4x64U indices(0x0000000200000003ull, 0x0000000000000001ull,
                         0x0000000600000007ull, 0x0000000400000005ull);
    // Slightly better to permute v0 than v1; it will be added to v1.
    V4x64U permuted(_mm256_permutevar8x32_epi32(val, indices));
    return permuted;
  }

  inline void Update(const V4x64U& packet1, const V4x64U& packet2) {
    const V4x64U mask(0x5555555555555555ull);
    V4x64U prevV0 = v0;
    V4x64U prevV1 = v1;
    V4x64U prevV2 = v2;
    V4x64U prevV3 = v3;
    V4x64U mul0(_mm256_mul_epu32(v0, v2 >> 32));
    V4x64U mul1(_mm256_mul_epu32(v1, v3 >> 32));
    V4x64U mul2(_mm256_mul_epu32(v0 >> 32, v2));
    V4x64U mul3(_mm256_mul_epu32(v1 >> 32, v3));
    v0 += packet1 & mask;
    v1 += AndNot(mask, packet1);
    v2 += packet2 & mask;
    v3 += AndNot(mask, packet2);
    v0 += ZipperMerge(prevV1);
    v1 += ZipperMerge(prevV0);
    v2 += ZipperMerge(prevV3);
    v3 += ZipperMerge(prevV2);
    v0 ^= mul3;
    v1 ^= mul2;
    v2 ^= mul1;
    v3 ^= mul0;
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

  static INLINE V4x64U ZipperMerge(const V4x64U& v) {
    // Multiplication mixes/scrambles bytes 0-7 of the 64-bit result to
    // varying degrees. In descending order of goodness, bytes
    // 3 4 2 5 1 6 0 7 have quality 228 224 164 160 100 96 36 32.
    // As expected, the upper and lower bytes are much worse.
    // For each 64-bit lane, our objectives are:
    // 1) maximizing and equalizing total goodness across the four lanes.
    // 2) mixing with bytes from the neighboring lane (AVX-2 makes it difficult
    //    to cross the 128-bit wall, but PermuteAndUpdate takes care of that);
    // 3) placing the worst bytes in the upper 32 bits because those will not
    //    be used in the next 32x32 multiplication.
    const uint64_t hi = 0x070806090D0A040Bull;
    const uint64_t lo = 0x000F010E05020C03ull;
    return V4x64U(_mm256_shuffle_epi8(v, V4x64U(hi, lo, hi, lo)));
  }

  INLINE uint64_t Finalize() {
    // Much faster than Store(v0 + v1) to uint64_t[].
    return _mm_cvtsi128_si64(_mm256_extracti128_si256(v0 + v1 + v2 + v3, 0));
  }

  INLINE void UpdateFinalBlock(const uint64_t *packets) {
    const V4x64U packet1 = LoadU(packets);
    const V4x64U packet2 = LoadU(packets + 4);
    Update(packet1, packet2);
    Update(packet1, packet2);
    Update(packet1, packet2);
    Update(packet1, packet2);
  }

  INLINE void UpdateFinalPacket(const uint64_t *packets, size_t remainder) {
    // Absorb the length to avoid collisions between messages with different
    // lengths of trailing 0's.
    v0 ^= V4x64U(0, 0, 0, remainder);
    if (remainder == kPacketSize) {
      UpdatePacket(packets);
    } else if (remainder > kPacketSize/2) {
      ALIGNED(uint8_t, 64) final_packet[kPacketSize];
      memcpy(final_packet, reinterpret_cast<const uint8_t*>(packets), remainder);
      memset(final_packet + remainder, 0, kPacketSize - remainder);
      UpdatePacket(reinterpret_cast<const uint64_t*>(final_packet));
    } else {
      // It is faster to do 4-rounds of Update in this case.
      const int num_full_blocks = remainder >> kBlockShift;
      for (int i = 0; i < num_full_blocks; ++i) {
        UpdateFinalBlock(packets);
        packets += kBlockSize / sizeof(uint64_t);
      }
      const size_t byte_remainder = remainder & (kBlockSize - 1);
      if (byte_remainder > 0) {
        // Final block is padded with 0's.
        ALIGNED(uint8_t, 64) final_block[kBlockSize] = {0,};
        memcpy(final_block, reinterpret_cast<const uint8_t*>(packets), byte_remainder);
        memset(final_block + byte_remainder, 0, kBlockSize - byte_remainder);
        UpdateFinalBlock(reinterpret_cast<const uint64_t*>(final_block));
      }
    }
  }

  void print() {
      v0.print("v0");
      v1.print("v1");
      v2.print("v2");
      v3.print("v3");
  }

  V4x64U v0;
  V4x64U v1;
  V4x64U v2;
  V4x64U v3;
};

}  // namespace

uint64_t HighwayTreeHash512(const uint64_t (&key)[4], const uint8_t* bytes,
                            const uint64_t size) {
  HighwayTreeHashState512 state(key);

  size_t num_full_packets = size >> kPacketShift;
  if ((size & (kPacketSize - 1)) == 0 && num_full_packets > 0) {
    // The last block is hashed differently, so make sure we still have data to hash.
    num_full_packets--;
  }
  const uint64_t* packets = reinterpret_cast<const uint64_t*>(bytes);
  for (size_t i = 0; i < num_full_packets; ++i) {
    state.UpdatePacket(packets);
    packets += kPacketSize / sizeof(uint64_t);
  }
  size_t remainder = size - (num_full_packets << kPacketShift);
  if (remainder > 0) {
      state.UpdateFinalPacket(packets, remainder);
  }
  return state.Finalize();
}
