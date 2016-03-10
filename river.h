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

#ifndef HIGHWAYHASH_RIVER_H_
#define HIGHWAYHASH_RIVER_H_

#include <cstdint>
#include "code_annotation.h"

class RiverImpl;

class River {
 public:
  static const uint32_t kBlockSize = 64;
  static const uint32_t kPacketSize = 512;

  // Create a river object for generating a cryptogrpahic stream of
  // pseudo-random data for use in a stream cipher.
  River(const uint64_t key[kBlockSize / sizeof(uint64_t)]);

  // Generate 64 uint64's worth (512 bytes) of cryptograpic pseudo-random data.
  const uint64_t *GeneratePseudoRandomData();

 private:
  RiverImpl* river_impl_;
  ALIGNED(uint64_t, 64) buffer_[kPacketSize + 16*32 + 64];
};

#endif  // #ifndef HIGHWAYHASH_RIVER_H_
