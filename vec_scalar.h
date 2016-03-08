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

#ifndef HIGHWAYHASH_VEC_SCALAR_H_
#define HIGHWAYHASH_VEC_SCALAR_H_

// Defines SIMD vector classes ("V4x64U") with overloaded arithmetic operators:
// const V4x64U masked_sum = (a + b) & m;
// This is shorter and more readable than compiler intrinsics:
// const __m256i masked_sum = _mm256_and_si256(_mm256_add_epi64(a, b), m);
// There is typically no runtime cost for these abstractions.
//
// The naming convention is VNxBBT where N is the number of lanes, BB the
// number of bits per lane and T is the lane type: unsigned integer (U),
// signed integer (I), or floating-point (F).
//
// Requires reasonable C++11 support (VC2015) and an AVX2-capable CPU.

#include <immintrin.h>
#include <cstdint>
#include "code_annotation.h"

// 256-bit AVX-2 vector with 4 uint64_t lanes.
class V4x64U {
 public:
  using T = uint64_t;
  static constexpr size_t kNumLanes = 4;

  // Leaves v_ uninitialized - typically used for output parameters.
  INLINE V4x64U() {}

  // Lane 0 (p_0) is the lowest.
  INLINE V4x64U(T p_3, T p_2, T p_1, T p_0) {
    v_[0] = p_0;
    v_[1] = p_1;
    v_[2] = p_2;
    v_[3] = p_3;
  }

  // Broadcasts i to all lanes.
  INLINE explicit V4x64U(T i) {
    v_[0] = i;
    v_[1] = i;
    v_[2] = i;
    v_[3] = i;
  }

  // _mm256_setzero_epi64 generates suboptimal code. Instead set
  // z = x - x (given an existing "x"), or x == x to set all bits.
  INLINE V4x64U& operator=(const V4x64U& other) {
    for(uint32_t i = 0; i < kNumLanes; i++) {
        v_[i] = other.v_[i];
    }
    return *this;
  }

  INLINE V4x64U& operator+=(const V4x64U& other) {
    for(uint32_t i = 0; i < kNumLanes; i++) {
        v_[i] += other.v_[i];
    }
    return *this;
  }
  INLINE V4x64U& operator-=(const V4x64U& other) {
    for(uint32_t i = 0; i < kNumLanes; i++) {
        v_[i] -= other.v_[i];
    }
    return *this;
  }

  INLINE V4x64U& operator&=(const V4x64U& other) {
    for(uint32_t i = 0; i < kNumLanes; i++) {
        v_[i] &= other.v_[i];
    }
    return *this;
  }
  INLINE V4x64U& operator|=(const V4x64U& other) {
    for(uint32_t i = 0; i < kNumLanes; i++) {
        v_[i] |= other.v_[i];
    }
    return *this;
  }
  INLINE V4x64U& operator^=(const V4x64U& other) {
    for(uint32_t i = 0; i < kNumLanes; i++) {
        v_[i] ^= other.v_[i];
    }
    return *this;
  }

  INLINE V4x64U& operator<<=(const int count) {
    for(uint32_t i = 0; i < kNumLanes; i++) {
        v_[i] <<= count;
    }
    return *this;
  }

  INLINE V4x64U& operator>>=(const int count) {
    for(uint32_t i = 0; i < kNumLanes; i++) {
        v_[i] >>= count;
    }
    return *this;
  }

  void print(const char *name) {
    printf("%s = %016lx%016lx%016lx%016lx\n", name, v_[3], v_[2], v_[1], v_[0]);
  }

  uint64_t v_[kNumLanes];
};

// Nonmember functions implemented in terms of member functions

static INLINE V4x64U operator+(const V4x64U& left, const V4x64U& right) {
  V4x64U t(left);
  return t += right;
}

static INLINE V4x64U operator-(const V4x64U& left, const V4x64U& right) {
  V4x64U t(left);
  return t -= right;
}

static INLINE V4x64U operator<<(const V4x64U& v, const int count) {
  V4x64U t(v);
  return t <<= count;
}

static INLINE V4x64U operator>>(const V4x64U& v, const int count) {
  V4x64U t(v);
  return t >>= count;
}

static INLINE V4x64U operator&(const V4x64U& left, const V4x64U& right) {
  V4x64U t(left);
  return t &= right;
}

static INLINE V4x64U operator|(const V4x64U& left, const V4x64U& right) {
  V4x64U t(left);
  return t |= right;
}

static INLINE V4x64U operator^(const V4x64U& left, const V4x64U& right) {
  V4x64U t(left);
  return t ^= right;
}

// Load/Store.

// "from" must be vector-aligned.
static INLINE V4x64U Load(const uint64_t* RESTRICT const from) {
  return V4x64U(from[3], from[2], from[1], from[0]);
}

static INLINE V4x64U LoadU(const uint64_t* RESTRICT const from) {
  return V4x64U(from[3], from[2], from[1], from[0]);
}

// "to" must be vector-aligned.
static INLINE void Store(const V4x64U& v, uint64_t* RESTRICT const to) {
    to[0] = v.v_[0];
    to[1] = v.v_[1];
    to[2] = v.v_[2];
    to[3] = v.v_[3];
}

static INLINE void StoreU(const V4x64U& v, uint64_t* RESTRICT const to) {
    to[0] = v.v_[0];
    to[1] = v.v_[1];
    to[2] = v.v_[2];
    to[3] = v.v_[3];
}

// Miscellaneous functions.

static INLINE V4x64U AndNot(const V4x64U& neg_mask, const V4x64U& values) {
  V4x64U res;
  for(uint32_t i = 0; i < V4x64U::kNumLanes; i++) {
    res.v_[i] = ~neg_mask.v_[i] & values.v_[i];
  }
  return res;
}

// There are no greater-than comparison instructions for unsigned T.
static INLINE bool operator==(const V4x64U& left, const V4x64U& right) {
  for(uint32_t i = 0; i < V4x64U::kNumLanes; i++) {
    if(left.v_[i] != right.v_[i]) {
      return false;
    }
  }
  return true;
}

#endif  // #ifndef HIGHWAYHASH_VEC_SCALAR_H_
