/*
Copyright 2020 Google LLC

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    https://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#pragma once

#include <inttypes.h>

class PRNG {
public:
  static int SecureRandom(void *data, size_t size);

  /* generates a random number on [0,0xffffffff]-interval */
  virtual uint32_t Rand() = 0;

  int Rand(int min, int max) {
    if (min == max) return min;
    return ((Rand() % (max - min + 1)) + min);
  }

  /* generates a random number on [0,1]-real-interval */
  double RandReal() {
    return Rand() * (1.0 / 4294967295.0);
  }
};
