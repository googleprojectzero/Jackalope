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

#include "common.h"
#include "mutator.h"

int Mutator::GetRandBlock(size_t samplesize, size_t minblocksize, size_t maxblocksize, size_t *blockstart, size_t *blocksize, PRNG *prng) {
  if (samplesize == 0) return 0;
  if (samplesize < minblocksize) return 0;
  if (samplesize < maxblocksize) maxblocksize = samplesize;
  *blocksize = prng->Rand((int)minblocksize, (int)maxblocksize);
  *blockstart = prng->Rand(0, (int)(samplesize - (*blocksize)));
  return 1;
}

bool ByteFlipMutator::Mutate(Sample *inout_sample, PRNG *prng, std::vector<Sample *> &all_samples) {
  // printf("In ByteFlipMutator::Mutate\n");
  if (inout_sample->size == 0) return true;
  int charpos = prng->Rand(0, (int)(inout_sample->size - 1));
  char c = (char)prng->Rand(0, 255);
  inout_sample->bytes[charpos] = c;
  return true;
}

bool BlockFlipMutator::Mutate(Sample *inout_sample, PRNG *prng, std::vector<Sample *> &all_samples) {
  // printf("In BlockFlipMutator::Mutate\n");
  size_t blocksize, blockpos;
  if (!GetRandBlock(inout_sample->size, min_block_size, max_block_size, &blockpos, &blocksize, prng)) return true;
  if (uniform) {
    char c = (char)prng->Rand(0, 255);
    for (size_t i = 0; i<blocksize; i++) {
      inout_sample->bytes[blockpos + i] = c;
    }
  } else {
    for (size_t i = 0; i<blocksize; i++) {
      inout_sample->bytes[blockpos + i] = (char)prng->Rand(0, 255);
    }
  }
  return true;
}

bool AppendMutator::Mutate(Sample *inout_sample, PRNG *prng, std::vector<Sample *> &all_samples) {
  // printf("In AppendMutator::Mutate\n");
  size_t old_size = inout_sample->size;
  if (old_size >= MAX_SAMPLE_SIZE) return true;
  size_t append = prng->Rand(min_append, max_append);
  if ((old_size + append) > MAX_SAMPLE_SIZE) {
    append = MAX_SAMPLE_SIZE - old_size;
  }
  if (append <= 0) return true;
  size_t new_size = old_size + append;
  inout_sample->bytes =
    (char *)realloc(inout_sample->bytes, new_size);
  inout_sample->size = new_size;
  for (size_t i = old_size; i < new_size; i++) {
    inout_sample->bytes[i] = (char)prng->Rand(0, 255);
  }
  return true;
}

bool BlockInsertMutator::Mutate(Sample *inout_sample, PRNG *prng, std::vector<Sample *> &all_samples) {
  // printf("In BlockInsertMutator::Mutate\n");
  size_t old_size = inout_sample->size;
  if (old_size >= MAX_SAMPLE_SIZE) return true;
  size_t to_insert = prng->Rand(min_insert, max_insert);
  if ((old_size + to_insert) > MAX_SAMPLE_SIZE) {
    to_insert = MAX_SAMPLE_SIZE - old_size;
  }
  size_t where = prng->Rand(0, (int)old_size);
  size_t new_size = old_size + to_insert;
  if (to_insert <= 0) return true;
  
  char *old_bytes = inout_sample->bytes;
  char *new_bytes = (char *)malloc(new_size);
  memcpy(new_bytes, old_bytes, where);
  
  for (size_t i = 0; i < to_insert; i++) {
    new_bytes[where + i] = (char)prng->Rand(0, 255);
  }
  
  memcpy(new_bytes + where + to_insert, old_bytes + where, old_size - where);

  if (old_bytes) free(old_bytes);
  inout_sample->bytes = new_bytes;
  inout_sample->size = new_size;
  return true;
}

bool BlockDuplicateMutator::Mutate(Sample *inout_sample, PRNG *prng, std::vector<Sample *> &all_samples) {
  // printf("In BlockDuplicateMutator::Mutate\n");
  if (inout_sample->size >= MAX_SAMPLE_SIZE) return true;
  size_t blockpos, blocksize;
  if (!GetRandBlock(inout_sample->size, min_block_size, max_block_size, &blockpos, &blocksize, prng)) return true;
  int64_t blockcount = prng->Rand(min_duplicate_cnt, max_duplicate_cnt);
  if ((inout_sample->size + blockcount * blocksize) > MAX_SAMPLE_SIZE)
    blockcount = (MAX_SAMPLE_SIZE - (int64_t)inout_sample->size) / blocksize;
  if (blockcount <= 0) return true;
  char *newbytes;
  newbytes = (char *)malloc(inout_sample->size + blockcount * blocksize);
  memcpy(newbytes, inout_sample->bytes, blockpos + blocksize);
  for (size_t i = 0; i<blockcount; i++) {
    memcpy(newbytes + blockpos + (i + 1)*blocksize, inout_sample->bytes + blockpos, blocksize);
  }
  memcpy(newbytes + blockpos + (blockcount + 1)*blocksize, 
         inout_sample->bytes + blockpos + blocksize,
         inout_sample->size - blockpos - blocksize);
  if (inout_sample->bytes) free(inout_sample->bytes);
  inout_sample->bytes = newbytes;
  inout_sample->size = inout_sample->size + blockcount * blocksize;
  return true;
}

void InterstingValueMutator::AddInterestingValue(char *data, size_t size) {
  Sample interesting_sample;
  interesting_sample.Init(data, size);
  interesting_values.push_back(interesting_sample);
}

bool InterstingValueMutator::Mutate(Sample *inout_sample, PRNG *prng, std::vector<Sample *> &all_samples) {
  // printf("In InterstingValueMutator::Mutate\n");
  if (interesting_values.empty()) return true;
  Sample *interesting_sample = &interesting_values[prng->Rand(0, (int)interesting_values.size() - 1)];
  size_t blockstart, blocksize;
  if (!GetRandBlock(inout_sample->size, interesting_sample->size, interesting_sample->size, &blockstart, &blocksize, prng)) return true;
  memcpy(inout_sample->bytes + blockstart, interesting_sample->bytes, interesting_sample->size);
  return true;
}

InterstingValueMutator::InterstingValueMutator(bool use_default_values) {
  if (!use_default_values) return;
  uint16_t i_word;
  i_word = 0; AddInterestingValue((char *)(&i_word), sizeof(i_word));
  i_word = 0xFFFF; AddInterestingValue((char *)(&i_word), sizeof(i_word));
  i_word = 1;
  for (int i = 0; i < 16; i++) {
    AddInterestingValue((char *)(&i_word), sizeof(i_word));
    i_word = (i_word << 1);
  }
  uint32_t i_dword;
  i_dword = 0; AddInterestingValue((char *)(&i_dword), sizeof(i_dword));
  i_dword = 0xFFFFFFFF; AddInterestingValue((char *)(&i_dword), sizeof(i_dword));
  i_dword = 1;
  for (int i = 0; i < 16; i++) {
    AddInterestingValue((char *)(&i_dword), sizeof(i_dword));
    i_dword = (i_dword << 1);
  }
  uint64_t i_qword;
  i_qword = 0; AddInterestingValue((char *)(&i_qword), sizeof(i_qword));
  i_qword = 0xFFFFFFFFFFFFFFFF; AddInterestingValue((char *)(&i_qword), sizeof(i_qword));
  i_qword = 1;
  for (int i = 0; i < 16; i++) {
    AddInterestingValue((char *)(&i_qword), sizeof(i_qword));
    i_qword = (i_qword << 1);
  }
}

bool SpliceMutator::Mutate(Sample *inout_sample, PRNG *prng, std::vector<Sample *> &all_samples) {
  if(all_samples.empty()) return true;

  bool displace = false;
  if(prng->RandReal() < displacement_p) {
    displace = true;
  }
  
  Sample *other_sample = all_samples[prng->Rand(0, (int)all_samples.size() - 1)];

  if(inout_sample->size == 0) return false;
  if(other_sample->size == 0) return false;

  if(points == 1) {
    size_t point1, point2;
    char *new_bytes;
    size_t new_sample_size;
    if(displace) {
      point1 = prng->Rand(0, (int)(inout_sample->size - 1));
      point2 = prng->Rand(0, (int)(other_sample->size - 1));
    } else {
      size_t minsize = inout_sample->size;
      if(other_sample->size < minsize) minsize = other_sample->size;
      point1 = prng->Rand(0, (int)(minsize - 1));
      point2 = point1;
    }
    new_sample_size = point1 + (other_sample->size - point2);
    if(new_sample_size == inout_sample->size) {
      memcpy(inout_sample->bytes + point1, other_sample->bytes + point2, other_sample->size - point2);
      return true;
    } else {
      new_bytes = (char *)malloc(new_sample_size);
      memcpy(new_bytes, inout_sample->bytes, point1);
      memcpy(new_bytes + point1, other_sample->bytes + point2, other_sample->size - point2);
      free(inout_sample->bytes);
      inout_sample->bytes = new_bytes;
      inout_sample->size = new_sample_size;
      if (inout_sample->size > MAX_SAMPLE_SIZE) inout_sample->Trim(MAX_SAMPLE_SIZE);
      return true;
    }
  } else if(points != 2) {
    FATAL("Splice mutator can only work with 1 or 2 splice points");
  }
  
  if(displace) {
    size_t blockstart1, blocksize1;
    size_t blockstart2, blocksize2;
    size_t blockstart3, blocksize3;
    if(!GetRandBlock(inout_sample->size, 1, inout_sample->size, &blockstart1, &blocksize1, prng)) return true;
    if(!GetRandBlock(other_sample->size, 1, other_sample->size, &blockstart2, &blocksize2, prng)) return true;
    blockstart3 = blockstart1 + blocksize1;
    blocksize3 = inout_sample->size - blockstart3;
    size_t new_sample_size = blockstart1 + blocksize2 + blocksize3;
    char *new_bytes = (char *)malloc(new_sample_size);
    memcpy(new_bytes, inout_sample->bytes, blockstart1);
    memcpy(new_bytes + blockstart1, other_sample->bytes + blockstart2, blocksize2);
    memcpy(new_bytes + blockstart1 + blocksize2, inout_sample->bytes + blockstart3, blocksize3);
    if(new_sample_size > MAX_SAMPLE_SIZE) {
      new_sample_size = MAX_SAMPLE_SIZE;
      new_bytes = (char *)realloc(new_bytes, MAX_SAMPLE_SIZE);
    }
    free(inout_sample->bytes);
    inout_sample->bytes = new_bytes;
    inout_sample->size = new_sample_size;
    return true;
  } else {
    size_t blockstart, blocksize;
    if(!GetRandBlock(other_sample->size, 2, other_sample->size, &blockstart, &blocksize, prng)) return true;
    if(blockstart > inout_sample->size) {
      blocksize += (blockstart - inout_sample->size);
      blockstart = inout_sample->size;
    }
    if((blockstart + blocksize) <= inout_sample->size) {
      memcpy(inout_sample->bytes + blockstart, other_sample->bytes + blockstart, blocksize);
      return true;
    }
    size_t new_sample_size = blockstart + blocksize;
    char *new_bytes = (char *)malloc(new_sample_size);
    memcpy(new_bytes, inout_sample->bytes, blockstart);
    memcpy(new_bytes + blockstart, other_sample->bytes + blockstart, blocksize);
    free(inout_sample->bytes);
    inout_sample->bytes = new_bytes;
    inout_sample->size = new_sample_size;
    return true;
  }
}
