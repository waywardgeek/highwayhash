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

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#else
#include <time.h>
#endif

#include "highway_tree_hash.h"
#include "highway_tree_hash512.h"
#include "scalar_highway_tree_hash.h"
#include "scalar_highway_tree_hash512.h"
#include "scalar_sip_tree_hash.h"
#include "river.h"
#include "sip_hash.h"
#include "sip_tree_hash.h"
#include "vec2.h"

uint64_t TimerTicks() {
#ifdef _WIN32
  LARGE_INTEGER counter;
  (void)QueryPerformanceCounter(&counter);
  return counter.QuadPart;
#else
  timespec t;
  clock_gettime(CLOCK_REALTIME, &t);
  return static_cast<uint64_t>(t.tv_sec) * 1000000000 + t.tv_nsec;
#endif
}

uint64_t TimerFrequency() {
#ifdef _WIN32
  LARGE_INTEGER frequency;
  (void)QueryPerformanceFrequency(&frequency);
  return frequency.QuadPart;
#else
  return 1000000000;  // ns
#endif
}

/*
Known-good SipHash-2-4 output from D. Bernstein.
key = 00 01 02 ...
and
in = (empty string)
in = 00 (1 byte)
in = 00 01 (2 bytes)
in = 00 01 02 (3 bytes)
...
in = 00 01 02 ... 3e (63 bytes)
*/
static const uint8_t vectors[64 * 8] = {
    0x31, 0x0E, 0x0E, 0xDD, 0x47, 0xDB, 0x6F, 0x72, 0xFD, 0x67, 0xDC, 0x93,
    0xC5, 0x39, 0xF8, 0x74, 0x5A, 0x4F, 0xA9, 0xD9, 0x09, 0x80, 0x6C, 0x0D,
    0x2D, 0x7E, 0xFB, 0xD7, 0x96, 0x66, 0x67, 0x85, 0xB7, 0x87, 0x71, 0x27,
    0xE0, 0x94, 0x27, 0xCF, 0x8D, 0xA6, 0x99, 0xCD, 0x64, 0x55, 0x76, 0x18,
    0xCE, 0xE3, 0xFE, 0x58, 0x6E, 0x46, 0xC9, 0xCB, 0x37, 0xD1, 0x01, 0x8B,
    0xF5, 0x00, 0x02, 0xAB, 0x62, 0x24, 0x93, 0x9A, 0x79, 0xF5, 0xF5, 0x93,
    0xB0, 0xE4, 0xA9, 0x0B, 0xDF, 0x82, 0x00, 0x9E, 0xF3, 0xB9, 0xDD, 0x94,
    0xC5, 0xBB, 0x5D, 0x7A, 0xA7, 0xAD, 0x6B, 0x22, 0x46, 0x2F, 0xB3, 0xF4,
    0xFB, 0xE5, 0x0E, 0x86, 0xBC, 0x8F, 0x1E, 0x75, 0x90, 0x3D, 0x84, 0xC0,
    0x27, 0x56, 0xEA, 0x14, 0xEE, 0xF2, 0x7A, 0x8E, 0x90, 0xCA, 0x23, 0xF7,
    0xE5, 0x45, 0xBE, 0x49, 0x61, 0xCA, 0x29, 0xA1, 0xDB, 0x9B, 0xC2, 0x57,
    0x7F, 0xCC, 0x2A, 0x3F, 0x94, 0x47, 0xBE, 0x2C, 0xF5, 0xE9, 0x9A, 0x69,
    0x9C, 0xD3, 0x8D, 0x96, 0xF0, 0xB3, 0xC1, 0x4B, 0xBD, 0x61, 0x79, 0xA7,
    0x1D, 0xC9, 0x6D, 0xBB, 0x98, 0xEE, 0xA2, 0x1A, 0xF2, 0x5C, 0xD6, 0xBE,
    0xC7, 0x67, 0x3B, 0x2E, 0xB0, 0xCB, 0xF2, 0xD0, 0x88, 0x3E, 0xA3, 0xE3,
    0x95, 0x67, 0x53, 0x93, 0xC8, 0xCE, 0x5C, 0xCD, 0x8C, 0x03, 0x0C, 0xA8,
    0x94, 0xAF, 0x49, 0xF6, 0xC6, 0x50, 0xAD, 0xB8, 0xEA, 0xB8, 0x85, 0x8A,
    0xDE, 0x92, 0xE1, 0xBC, 0xF3, 0x15, 0xBB, 0x5B, 0xB8, 0x35, 0xD8, 0x17,
    0xAD, 0xCF, 0x6B, 0x07, 0x63, 0x61, 0x2E, 0x2F, 0xA5, 0xC9, 0x1D, 0xA7,
    0xAC, 0xAA, 0x4D, 0xDE, 0x71, 0x65, 0x95, 0x87, 0x66, 0x50, 0xA2, 0xA6,
    0x28, 0xEF, 0x49, 0x5C, 0x53, 0xA3, 0x87, 0xAD, 0x42, 0xC3, 0x41, 0xD8,
    0xFA, 0x92, 0xD8, 0x32, 0xCE, 0x7C, 0xF2, 0x72, 0x2F, 0x51, 0x27, 0x71,
    0xE3, 0x78, 0x59, 0xF9, 0x46, 0x23, 0xF3, 0xA7, 0x38, 0x12, 0x05, 0xBB,
    0x1A, 0xB0, 0xE0, 0x12, 0xAE, 0x97, 0xA1, 0x0F, 0xD4, 0x34, 0xE0, 0x15,
    0xB4, 0xA3, 0x15, 0x08, 0xBE, 0xFF, 0x4D, 0x31, 0x81, 0x39, 0x62, 0x29,
    0xF0, 0x90, 0x79, 0x02, 0x4D, 0x0C, 0xF4, 0x9E, 0xE5, 0xD4, 0xDC, 0xCA,
    0x5C, 0x73, 0x33, 0x6A, 0x76, 0xD8, 0xBF, 0x9A, 0xD0, 0xA7, 0x04, 0x53,
    0x6B, 0xA9, 0x3E, 0x0E, 0x92, 0x59, 0x58, 0xFC, 0xD6, 0x42, 0x0C, 0xAD,
    0xA9, 0x15, 0xC2, 0x9B, 0xC8, 0x06, 0x73, 0x18, 0x95, 0x2B, 0x79, 0xF3,
    0xBC, 0x0A, 0xA6, 0xD4, 0xF2, 0x1D, 0xF2, 0xE4, 0x1D, 0x45, 0x35, 0xF9,
    0x87, 0x57, 0x75, 0x19, 0x04, 0x8F, 0x53, 0xA9, 0x10, 0xA5, 0x6C, 0xF5,
    0xDF, 0xCD, 0x9A, 0xDB, 0xEB, 0x75, 0x09, 0x5C, 0xCD, 0x98, 0x6C, 0xD0,
    0x51, 0xA9, 0xCB, 0x9E, 0xCB, 0xA3, 0x12, 0xE6, 0x96, 0xAF, 0xAD, 0xFC,
    0x2C, 0xE6, 0x66, 0xC7, 0x72, 0xFE, 0x52, 0x97, 0x5A, 0x43, 0x64, 0xEE,
    0x5A, 0x16, 0x45, 0xB2, 0x76, 0xD5, 0x92, 0xA1, 0xB2, 0x74, 0xCB, 0x8E,
    0xBF, 0x87, 0x87, 0x0A, 0x6F, 0x9B, 0xB4, 0x20, 0x3D, 0xE7, 0xB3, 0x81,
    0xEA, 0xEC, 0xB2, 0xA3, 0x0B, 0x22, 0xA8, 0x7F, 0x99, 0x24, 0xA4, 0x3C,
    0xC1, 0x31, 0x57, 0x24, 0xBD, 0x83, 0x8D, 0x3A, 0xAF, 0xBF, 0x8D, 0xB7,
    0x0B, 0x1A, 0x2A, 0x32, 0x65, 0xD5, 0x1A, 0xEA, 0x13, 0x50, 0x79, 0xA3,
    0x23, 0x1C, 0xE6, 0x60, 0x93, 0x2B, 0x28, 0x46, 0xE4, 0xD7, 0x06, 0x66,
    0xE1, 0x91, 0x5F, 0x5C, 0xB1, 0xEC, 0xA4, 0x6C, 0xF3, 0x25, 0x96, 0x5C,
    0xA1, 0x6D, 0x62, 0x9F, 0x57, 0x5F, 0xF2, 0x8E, 0x60, 0x38, 0x1B, 0xE5,
    0x72, 0x45, 0x06, 0xEB, 0x4C, 0x32, 0x8A, 0x95};

static void VerifySipHash() {
  const int kMaxSize = 64;
  uint8_t in[kMaxSize];
  bool ok = true;

  const uint64_t key[2] = {0x0706050403020100ULL, 0x0F0E0D0C0B0A0908ULL};

  for (int size = 0; size < kMaxSize; ++size) {
    in[size] = static_cast<uint8_t>(size);
    const uint64_t hash = SipHash(key, in, size);

    uint8_t out[8];
    memcpy(out, &hash, sizeof(hash));

    if (memcmp(out, vectors + size * 8, 8)) {
      printf("Failed for length %d\n", size);
      ok = false;
    }
  }

  if (!ok) {
    exit(1);
  }
  printf("Verified SipHash.\n");
}

template <class Function1, class Function2>
static void VerifyEqual(const char* caption, const Function1& hash_function1,
                        const Function2& hash_function2) {
  const int kMaxSize = 128;
  ALIGNED(uint8_t, 64) in[kMaxSize] = {0};

  const ALIGNED(uint64_t, 64) key[4] = {0x0706050403020100ULL, 0x0F0E0D0C0B0A0908ULL,
                           0x1716151413121110ULL, 0x1F1E1D1C1B1A1918ULL};

  for (int size = 0; size < kMaxSize; ++size) {
    in[size] = static_cast<uint8_t>(size);
    const uint64_t hash = hash_function1(key, in, size);
    const uint64_t hash2 = hash_function2(key, in, size);
    if (hash != hash2) {
      printf("Failed for length %d %lx %lx\n", size, hash, hash2);
      exit(1);
    }
  }
  printf("Verified %s.\n", caption);
}

template <class Function>
static void Benchmark(const char* caption, const Function& hash_function, const int size) {
  ALIGNED(uint8_t, 64) in[size];
  for (int i = 0; i < size; ++i) {
    in[i] = static_cast<uint8_t>(i);
  }

  const uint64_t key[4] = {0x0706050403020100ULL, 0x0F0E0D0C0B0A0908ULL,
                           0x1716151413121110ULL, 0x1F1E1D1C1B1A1918ULL};

  uint64_t sum = 1;
  uint64_t minTicks = 99999999999;
  const int kLoops = 50000;
  //const int kLoops = 1;
  for (int rep = 0; rep < 25; ++rep) {
  //for (int rep = 0; rep < 1; ++rep) {
    const uint64_t t0 = TimerTicks();
    COMPILER_FENCE;
    for (int loop = 0; loop < kLoops; ++loop) {
      sum <<= 1;
      const uint64_t hash = hash_function(key, in, size);
      sum ^= hash;
    }
    const uint64_t t1 = TimerTicks();
    COMPILER_FENCE;
    minTicks = std::min(minTicks, t1 - t0);
  }
  const double minSec = double(minTicks) / TimerFrequency();
  const double cyclesPerByte = 3.5E9 * minSec / (kLoops * size);
  const double GBps = kLoops * size / minSec * 1E-9;
  printf("%-28s %5d sum=0x%016lx\tGBps=%6.2f  c/b=%.2f\n", caption, size, sum, GBps,
         cyclesPerByte);
}

template <class Function>
static void Benchmark512(const char* caption, const Function& hash_function, const int size) {
  ALIGNED(uint8_t, 64) in[size];
  for (int i = 0; i < size; ++i) {
    in[i] = static_cast<uint8_t>(i);
  }

  const uint64_t key[8] = {0x0706050403020100ULL, 0x0F0E0D0C0B0A0908ULL,
                           0x1716151413121110ULL, 0x1F1E1D1C1B1A1918ULL,
                           0x0706050403020100ULL, 0x0F0E0D0C0B0A0908ULL,
                           0x1716151413121110ULL, 0x1F1E1D1C1B1A1918ULL};

  uint64_t sum = 1;
  uint64_t minTicks = 99999999999;
  const int kLoops = 50000;
  //const int kLoops = 1;
  for (int rep = 0; rep < 25; ++rep) {
  //for (int rep = 0; rep < 1; ++rep) {
    const uint64_t t0 = TimerTicks();
    COMPILER_FENCE;
    for (int loop = 0; loop < kLoops; ++loop) {
      sum <<= 1;
      ALIGNED(uint64_t, 64) hash[8];
      hash_function(key, in, size, hash);
      sum ^= hash[0];
    }
    const uint64_t t1 = TimerTicks();
    COMPILER_FENCE;
    minTicks = std::min(minTicks, t1 - t0);
  }
  const double minSec = double(minTicks) / TimerFrequency();
  const double cyclesPerByte = 3.5E9 * minSec / (kLoops * size);
  const double GBps = kLoops * size / minSec * 1E-9;
  printf("%-28s %5d sum=0x%016lx\tGBps=%6.2f  c/b=%.2f\n", caption, size, sum, GBps,
         cyclesPerByte);
}

static void BenchmarkRiver() {
  const ALIGNED(uint64_t, 64) key[8] = {
      0x0706050403020100ULL, 0x0F0E0D0C0B0A0908ULL,
      0x1716151413121110ULL, 0x1F1E1D1C1B1A1918ULL,
      0x0706050403020100ULL, 0x0F0E0D0C0B0A0908ULL,
      0x1716151413121110ULL, 0x1F1E1D1C1B1A1918ULL
  };

  River river(key);
  uint64_t sum = 1;
  uint64_t minTicks = 99999999999;
  const int kLoops = 50000;
  //const int kLoops = 1;
  for (int rep = 0; rep < 25; ++rep) {
  //for (int rep = 0; rep < 1; ++rep) {
    const uint64_t t0 = TimerTicks();
    COMPILER_FENCE;
    for (uint32_t loop = 0; loop < kLoops; ++loop) {
      const uint64_t* data = river.GeneratePseudoRandomData();
      //for (uint32_t j = 0; j < River::kPacketSize / sizeof(uint64_t); ++j) {
          sum <<= 1;
          sum ^= data[(River::kPacketSize / sizeof(uint64_t)) - 1];
      //}
    }
    const uint64_t t1 = TimerTicks();
    COMPILER_FENCE;
    minTicks = std::min(minTicks, t1 - t0);
  }
  const double minSec = double(minTicks) / TimerFrequency();
  const double cyclesPerByte = 3.5E9 * minSec / (kLoops * River::kPacketSize);
  const double GBps = kLoops * River::kPacketSize / minSec * 1E-9;
  printf("%-28s       sum=0x%016lx\tGBps=%6.2f  c/b=%.2f\n", "River", sum,
      GBps, cyclesPerByte);
}

int main(int argc, char* argv[]) {
  int size = 1024;
  if(argc == 2) {
      size = atoi(argv[1]);
  }
  if (argc > 2) {
      fprintf(stderr, "Usage: sip_hash_main [size]\n");
      return 1;
  }
  Benchmark("ScalarSipTreeHash", ScalarSipTreeHash, size);
  Benchmark("ScalarHighwayTreeHash", ScalarHighwayTreeHash, size);
  //Benchmark("ScalarHighwayTreeHash512", ScalarHighwayTreeHash512, size);
  Benchmark("SipHash", SipHash, size);
  Benchmark("SipTreeHash", SipTreeHash, size);
  Benchmark("HighwayTreeHash", HighwayTreeHash, size);
  Benchmark512("HighwayTreeHash512", HighwayTreeHash512, size);
  BenchmarkRiver();

  VerifySipHash();
  VerifyEqual("SipTree scalar", SipTreeHash, ScalarSipTreeHash);
  VerifyEqual("HighwayTree scalar", HighwayTreeHash, ScalarHighwayTreeHash);
  //VerifyEqual("HighwayTree512 scalar", HighwayTreeHash512, ScalarHighwayTreeHash512);

  return 0;
}
