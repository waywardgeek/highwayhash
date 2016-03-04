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

#include "highway_bush_hash512.h"

#include <cstring>  // memcpy
#include "vec2.h"

namespace {

// J-lanes tree hashing: see http://dx.doi.org/10.4236/jis.2014.53010

// Four (2 x 64-bit) hash states are updated in parallel by injecting
// four 64-bit packets per Update(). Finalize() combines the four states into
// one final 64-bit digest.
const int kNumLanes = 8;
const int kPacketSize = 2 * kNumLanes * sizeof(uint64_t);

class HighwayBushHashState512 {
 public:
  explicit INLINE HighwayBushHashState512(const uint64_t (&keys)[kNumLanes]) {
    // "Nothing up my sleeve" numbers, concatenated hex digits of Pi from
    // http://www.numberworld.org/digits/Pi/, retrieved Feb 22, 2016.
    //
    // We use this python code to generate the fourth number to have
    // more even mixture of bits:
    /*
def x(a,b,c):
  retval = 0
  for i in range(64):
    count = ((a >> i) & 1) + ((b >> i) & 1) + ((c >> i) & 1)
    if (count <= 1):
      retval |= 1 << i
  return retval
    */
    const V4x64U init0(0x243f6a8885a308d3ull, 0x13198a2e03707344ull,
                       0xa4093822299f31d0ull, 0xdbe6d5d5fe4cce2full);
    const V4x64U init1(0x452821e638d01377ull, 0xbe5466cf34e90c6cull,
                       0xc0acf169b5f18a8cull, 0x3bd39e10cb0ef593ull);
    const V4x64U key = LoadU(keys);
    v0 = init0;
    v1 = key ^ init1;
    v0 = init0; // TODO: choose better constants
    v1 = init1;
  }

  INLINE void Update(const V4x64U& packet1, const V4x64U& packet2) {
    v1 ^= packet1;
    v3 ^= packet2;
    V4x64U v0odd = v0 | V4x64U(0x0000000070000001ULL);
    V4x64U v1odd = v1 | V4x64U(0x0000000070000001ULL);
    V4x64U v2odd = v2 | V4x64U(0x0000000070000001ULL);
    V4x64U v3odd = v3 | V4x64U(0x0000000070000001ULL);
    V4x64U mul0(_mm256_mul_epu32(v0odd, v0 >> 32));
    V4x64U mul1(_mm256_mul_epu32(v1odd, v1 >> 32));
    V4x64U mul2(_mm256_mul_epu32(v2odd, v2 >> 32));
    V4x64U mul3(_mm256_mul_epu32(v3odd, v3 >> 32));
    v1 ^= ZipperMerge(mul0);
    v0 ^= ZipperMerge(mul1);
    v3 ^= ZipperMerge(mul2);
    v2 ^= ZipperMerge(mul3);
  }

  INLINE void Swap() {
    V4x64U temp = v0;
    v0 = v2;
    v2 = temp;
  }

  INLINE uint64_t Finalize() {
    // Mix together all lanes.
    Swap();
    PermuteAndUpdate();
    Swap();
    PermuteAndUpdate();
    Swap();
    PermuteAndUpdate();
    Swap();
    PermuteAndUpdate();

    // Much faster than Store(v0 + v1) to uint64_t[].
    return _mm_cvtsi128_si64(_mm256_extracti128_si256(v0 + v1, 0));
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

  INLINE void PermuteAndUpdate() {
    // For complete mixing, we need to swap the upper and lower 128-bit halves;
    // we also swap all 32-bit halves.
    const V4x64U indices(0x0000000200000003ull, 0x0000000000000001ull,
                         0x0000000600000007ull, 0x0000000400000005ull);
    // Slightly better to permute v0 than v1; it will be added to v1.
    const V4x64U permuted1(_mm256_permutevar8x32_epi32(v0, indices));
    const V4x64U permuted2(_mm256_permutevar8x32_epi32(v2, indices));
    Update(permuted1, permuted2);
  }

  V4x64U v0;
  V4x64U v1;
  V4x64U v2;
  V4x64U v3;
};

}  // namespace

uint64_t HighwayBushHash512(const uint64_t (&key)[kNumLanes], const uint8_t* bytes,
                         const uint64_t size) {
  HighwayBushHashState512 state(key);

  const size_t remainder = size & (kPacketSize - 1);
  const size_t truncated_size = size - remainder;
  const uint64_t* packets = reinterpret_cast<const uint64_t*>(bytes);
  for (int i = 0; i < truncated_size / sizeof(uint64_t); i += kNumLanes*2) {
    const V4x64U packet1 = LoadU(packets + i);
    const V4x64U packet2 = LoadU(packets + i + kNumLanes);
    state.Update(packet1, packet2);
  }

  if (remainder > 0) {
      uint64_t final_packet[kPacketSize / sizeof(uint64_t)] = {0,};
      memcpy(final_packet, packets + truncated_size / sizeof(uint64_t), remainder);
      const V4x64U packet1 = LoadU(final_packet);
      const V4x64U packet2 = LoadU(final_packet + kNumLanes);
      state.Update(packet1, packet2);
  }
  return state.Finalize();
}