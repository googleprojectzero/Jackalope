/*
A C-program for MT19937, with initialization improved 2002/1/26.
Coded by Takuji Nishimura and Makoto Matsumoto.

Before using, initialize the state by using init_genrand(seed)
or init_by_array(init_key, key_length).

Copyright (C) 1997 - 2002, Makoto Matsumoto and Takuji Nishimura,
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

1. Redistributions of source code must retain the above copyright
notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright
notice, this list of conditions and the following disclaimer in the
documentation and/or other materials provided with the distribution.

3. The names of its contributors may not be used to endorse or promote
products derived from this software without specific prior written
permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


Any feedback is very welcome.
http://www.math.sci.hiroshima-u.ac.jp/~m-mat/MT/emt.html
email: m-mat @ math.sci.hiroshima-u.ac.jp (remove space)
*/

#define _CRT_RAND_S
#define _CRT_SECURE_NO_WARNINGS

#include "stdio.h"
#include "mersenne.h"

/* Period parameters */
#define N 624
#define M 397
#define MATRIX_A 0x9908b0dfU   /* constant vector a */
#define UPPER_MASK 0x80000000U /* most significant w-r bits */
#define LOWER_MASK 0x7fffffffU /* least significant r bits */

MTPRNG::MTPRNG() {
  mti=N+1;
  
  // 128 bits should be sufficient for everyone ;-)
  uint32_t seed[4];

  SecureRandom(seed, sizeof(seed));

  Seed(seed, 4);
}

MTPRNG::MTPRNG(uint32_t seed) {
  mti=N+1;
  Seed(seed);
}

MTPRNG::MTPRNG(uint32_t *seed_arr, size_t size) {
  mti=N+1;
  Seed(seed_arr, size);
}

void MTPRNG::Seed(uint32_t seed) {
  mt[0]= seed & 0xffffffffU;
  for (mti=1; mti<N; mti++) {
    mt[mti] =
      (1812433253U * (mt[mti-1] ^ (mt[mti-1] >> 30)) + mti);
      /* See Knuth TAOCP Vol2. 3rd Ed. P.106 for multiplier. */
      /* In the previous versions, MSBs of the seed affect   */
      /* only MSBs of the array mt[].                        */
      /* 2002/01/09 modified by Makoto Matsumoto             */
      mt[mti] &= 0xffffffffU;
      /* for >32 bit machines */
  }
}

void MTPRNG::Seed(uint32_t *seed_arr, size_t seed_size) {
  int i, j, k;
  Seed(19650218U);
  i=1; j=0;
  k = (N>seed_size ? N : (int)seed_size);
  for (; k; k--) {
      mt[i] = (mt[i] ^ ((mt[i-1] ^ (mt[i-1] >> 30)) * 1664525U))
        + seed_arr[j] + j; /* non linear */
      mt[i] &= 0xffffffffU; /* for WORDSIZE > 32 machines */
      i++; j++;
      if (i>=N) { mt[0] = mt[N-1]; i=1; }
      if (j>=seed_size) j=0;
  }
  for (k=N-1; k; k--) {
      mt[i] = (mt[i] ^ ((mt[i-1] ^ (mt[i-1] >> 30)) * 1566083941U))
        - i; /* non linear */
      mt[i] &= 0xffffffffU; /* for WORDSIZE > 32 machines */
      i++;
      if (i>=N) { mt[0] = mt[N-1]; i=1; }
  }

  mt[0] = 0x80000000U; /* MSB is 1; assuring non-zero initial array */
}

uint32_t MTPRNG::Rand() {
  uint32_t y;
  static uint32_t mag01[2]={0x0U, MATRIX_A};
  /* mag01[x] = x * MATRIX_A  for x=0,1 */

  if (mti >= N) { /* generate N words at one time */
      int kk;

      if (mti == N+1)   /* if init_genrand() has not been called, */
          Seed(5489U); /* a default initial seed is used */

      for (kk=0;kk<N-M;kk++) {
          y = (mt[kk]&UPPER_MASK)|(mt[kk+1]&LOWER_MASK);
          mt[kk] = mt[kk+M] ^ (y >> 1) ^ mag01[y & 0x1U];
      }
      for (;kk<N-1;kk++) {
          y = (mt[kk]&UPPER_MASK)|(mt[kk+1]&LOWER_MASK);
          mt[kk] = mt[kk+(M-N)] ^ (y >> 1) ^ mag01[y & 0x1U];
      }
      y = (mt[N-1]&UPPER_MASK)|(mt[0]&LOWER_MASK);
      mt[N-1] = mt[M-1] ^ (y >> 1) ^ mag01[y & 0x1U];

      mti = 0;
  }

  y = mt[mti++];

  /* Tempering */
  y ^= (y >> 11);
  y ^= (y << 7) & 0x9d2c5680U;
  y ^= (y << 15) & 0xefc60000U;
  y ^= (y >> 18);

  return y;
}
