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
const int kBlockShift = 6;
const int kBlockSize = 1 << kBlockShift;  // 64
const int kPacketShift = 9;
const int kPacketSize = 1 << kPacketShift;  // 512

class HighwayTreeHashState512 {
 public:
  explicit INLINE HighwayTreeHashState512(const uint64_t (&keys)[kNumLanes]) {
    const ALIGNED(V4x64S, 64) init0(0x243f6a8885a308d3ull, 0x13198a2e03707344ull,
        0xa4093822299f31d0ull, 0xdbe6d5d5fe4cce2full);
    const ALIGNED(V4x64S, 64) init1(0x452821e638d01377ull, 0xbe5466cf34e90c6cull,
        0xc0acf169b5f18a8cull, 0x3bd39e10cb0ef593ull);
    const V4x64S key = LoadU(keys);
    // TODO: find better numbers for v2, v3.
    v0 = init0 ^ key;
    v1 = init1;
    v2 = init0 + init1;
    v3 = init0 ^ init1;
  }

  INLINE V4x64S Permute(const V4x64S& val) {
    // For complete mixing, we need to swap the upper and lower 128-bit halves;
    // we also swap all 32-bit halves.
    // Slightly better to permute v0 than v1; it will be added to v1.
    V4x64S permuted;
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

  inline V4x64S Multiply(const V4x64S& a, const V4x64S& b) {
      V4x64S r;
      r.v_[0] = uint64_t(uint32_t(a.v_[0]))*uint32_t(b.v_[0]);
      r.v_[1] = uint64_t(uint32_t(a.v_[1]))*uint32_t(b.v_[1]);
      r.v_[2] = uint64_t(uint32_t(a.v_[2]))*uint32_t(b.v_[2]);
      r.v_[3] = uint64_t(uint32_t(a.v_[3]))*uint32_t(b.v_[3]);
      return r;
  }

  inline void Update(const V4x64S& packet1, const V4x64S& packet2) {
    const V4x64S mask(0x5555555555555555ull);
    V4x64S mul0(Multiply(v0, v2 >> 32));
    V4x64S mul1(Multiply(v1, v3 >> 32));
    V4x64S mul2(Multiply(Permute(v0), v2));
    V4x64S mul3(Multiply(Permute(v1), v3));
    v0 ^= packet1 & mask;
    v1 ^= AndNot(mask, packet1);
    v2 ^= packet2 & mask;
    v3 ^= AndNot(mask, packet2);
    v0 ^= mul1;
    v1 ^= mul0;
    v2 ^= ZipperMerge(mul3);
    v3 ^= ZipperMerge(mul2);
  }

  INLINE void UpdatePacket(const uint64_t *packets) {
    V4x64S packet1 = LoadU(packets + 0*4);
    V4x64S packet2 = LoadU(packets + 1*4);
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
    return v0.v_[0] + v1.v_[0] + v2.v_[0] + v3.v_[0];
  }

  static INLINE V4x64S ZipperMerge(const V4x64S& val) {
    const uint8_t *v = reinterpret_cast<const uint8_t*>(val.v_);
    V4x64S res;
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

  INLINE void UpdateFinalBlock(const uint64_t *packets) {
    const V4x64S packet1 = LoadU(packets);
    const V4x64S packet2 = LoadU(packets + 4);
    Update(packet1, packet2);
    Update(packet1, packet2);
    Update(packet1, packet2);
    Update(packet1, packet2);
  }

  INLINE void UpdateFinalPacket(const uint64_t *packets, size_t remainder) {
    // Absorb the length to avoid collisions between messages with different
    // lengths of trailing 0's.
    v0 ^= V4x64S(0, 0, 0, remainder);
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
    printf("v0 = %016lx%016lx%016lx%016lx\n", v0.v_[3], v0.v_[2], v0.v_[1], v0.v_[0]);
    printf("v1 = %016lx%016lx%016lx%016lx\n", v1.v_[3], v1.v_[2], v1.v_[1], v1.v_[0]);
    printf("v2 = %016lx%016lx%016lx%016lx\n", v2.v_[3], v2.v_[2], v2.v_[1], v2.v_[0]);
    printf("v3 = %016lx%016lx%016lx%016lx\n", v3.v_[3], v3.v_[2], v3.v_[1], v3.v_[0]);
  }

  V4x64S v0;
  V4x64S v1;
  V4x64S v2;
  V4x64S v3;
};

}  // namespace

uint64_t ScalarHighwayTreeHash512(const uint64_t (&key)[4], const uint8_t* bytes,
                         const uint64_t size) {
  HighwayTreeHashState512 state(key);

  size_t num_full_packets = size >> kPacketShift;
  if ((size & (kPacketSize - 1)) == 0 && num_full_packets > 0) {
    // The last block is hashed differently, so make sure we still have data to hash.
    num_full_packets--;
  }
  // Note: this requires aligned input data, which wont be true in general.
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
