// Copyright 2016 Google Inc. All Rights Reserved.
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

#include <cstdint>
#include <cstdio>
#include <cstring>
#include "river.h"

int main() {
  const ALIGNED(uint64_t, 64) key[8] = {
      0x0706050403020100ULL, 0x0F0E0D0C0B0A0908ULL,
      0x1716151413121110ULL, 0x1F1E1D1C1B1A1918ULL,
      0x0706050403020100ULL, 0x0F0E0D0C0B0A0908ULL,
      0x1716151413121110ULL, 0x1F1E1D1C1B1A1918ULL
  };
  River river(key);
  while (true) {
    const uint64_t* data = river.GeneratePseudoRandomData();
    fwrite(data, sizeof(uint64_t), River::kPacketSize / sizeof(uint64_t), stdout);
  }
  return 0;  // Dummy return
}
