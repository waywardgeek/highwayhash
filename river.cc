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

#include "river.h"

#include <cstring>  // memcpy
#include <stdio.h>
#include "vec2.h"

const int kBlockSize = 64;
const int kPacketSize = 512;

class RiverImpl {
 public:
  void Init(const uint64_t key[kBlockSize / sizeof(uint64_t)]) {
    ALIGNED(uint64_t, 64) key_data[kBlockSize / sizeof(uint64_t)];
    memcpy(key_data, key, kBlockSize);
    const V4x64U init0(0x243f6a8885a308d3ull, 0x13198a2e03707344ull,
        0xa4093822299f31d0ull, 0xdbe6d5d5fe4cce2full);
    const V4x64U init1(0x452821e638d01377ull, 0xbe5466cf34e90c6cull,
        0xc0acf169b5f18a8cull, 0x3bd39e10cb0ef593ull);
    V4x64U key0 = LoadU(key);
    V4x64U key1 = LoadU(key + 4);
    // TODO: find better constants.
    v0 = init0 + key0;
    v1 = init1 ^ key1;
    v2 = init0 + init1;
    v3 = init0 ^ init1;
    mul0 = v0 + init0;
    mul1 = v1 ^ init1;
    mul2 = v2 + init0;
    mul3 = v3 ^ init1;
  }

  inline void Update(V4x64U *out1, V4x64U *out2) {
    v0 += *out1;
    v1 += *out2;
    v1 ^= mul0;
    mul0 ^= V4x64U(_mm256_mul_epu32(v0, Permute(v2)));
    v0 ^= mul1;
    mul1 ^= V4x64U(_mm256_mul_epu32(v1, Permute(v3)));
    v3 ^= mul2;
    mul2 ^= V4x64U(_mm256_mul_epu32(Permute(v0), v2));
    v2 ^= mul3;
    mul3 ^= V4x64U(_mm256_mul_epu32(Permute(v1), v3));
    v0 ^= ZipperMerge(v2);
    v1 ^= ZipperMerge(v3);
    v2 += ZipperMerge(v0);
    v3 += ZipperMerge(v1);
    *out1 += v2;
    *out2 += v3;
  }

  INLINE V4x64U Permute(const V4x64U& val) {
    // For complete mixing, we need to swap the upper and lower 128-bit halves;
    // we also swap all 32-bit halves.
    const V4x64U indices(0x0000000200000003ull, 0x0000000000000001ull,
                         0x0000000600000007ull, 0x0000000400000005ull);
    V4x64U permuted(_mm256_permutevar8x32_epi32(val, indices));
    return permuted;
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

  INLINE void UpdatePacket() {
    Update(packets + 0,  packets + 1);
    Update(packets + 2,  packets + 3);
    Update(packets + 4,  packets + 5);
    Update(packets + 6,  packets + 7);
    Update(packets + 8,  packets + 9);
    Update(packets + 10, packets + 11);
    Update(packets + 12, packets + 13);
    Update(packets + 14, packets + 15);
    /* These extra calls to Update do not seem to be required.
    Update(packets + 8,  packets + 9);
    Update(packets + 2,  packets + 3);
    Update(packets + 12, packets + 13);
    Update(packets + 6,  packets + 7);
    Update(packets + 0,  packets + 1);
    Update(packets + 10, packets + 11);
    Update(packets + 4,  packets + 5);
    Update(packets + 14, packets + 15);
    */
  }

  void print() {
      v0.print("v0");
      v1.print("v1");
      v2.print("v2");
      v3.print("v3");
  }

  // Returns pointer to uint64_ data[8] of pseudo-random data.
  const uint64_t *GenerateData() {
     UpdatePacket();
     return reinterpret_cast<const uint64_t*>(packets);
  }

  V4x64U v0;
  V4x64U v1;
  V4x64U v2;
  V4x64U v3;
  V4x64U mul0;
  V4x64U mul1;
  V4x64U mul2;
  V4x64U mul3;
  V4x64U packets[16];
};  // class RiverImpl

// Create a river object for generating a cryptogrpahic stream of
// pseudo-random data for use in a stream cipher.
River::River(const uint64_t key[kBlockSize / sizeof(uint64_t)]) {
  river_impl_ = reinterpret_cast<RiverImpl *>(buffer_);
  river_impl_->Init(key);
}

// Generate 64 uint64's worth (512 bytes) of cryptograpic pseudo-random data.
const uint64_t *River::GeneratePseudoRandomData() {
  return river_impl_->GenerateData();
}
