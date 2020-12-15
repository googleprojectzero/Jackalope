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

#define _CRT_RAND_S
#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <string.h>
#include "common.h"
#include "prng.h"

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32)
#include <windows.h>
#endif

int PRNG::SecureRandom(void *data, size_t size) {

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32)

  uint32_t r;
  size_t remainder = size % 4;
  size_t size4 = size / 4;
  uint32_t *data4 = (uint32_t *)data;
  for (int i = 0; i < size4; i++) {
    rand_s(&data4[i]);
  }
  if (remainder) {
    rand_s(&r);
    memcpy((char *)data + (size4 * 4), &r, remainder);
  }

#else

  FILE *fp = fopen("/dev/urandom", "rb");
  if (!fp) FATAL("Error opening /dev/urandom");
  if (fread(data, 1, size, fp) != size) {
    FATAL("Error reading /dev/urandom");
  }
  fclose(fp);

#endif

  return 1;
}
