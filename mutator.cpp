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

#include "stdlib.h"
#include "string.h"
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

bool ArithmeticMutator::Mutate(Sample *inout_sample,
                               PRNG *prng,
                               std::vector<Sample *> &all_samples)
{
  int flip_endian = prng->Rand(0, 1);
  int size = prng->Rand(0, 2);
  switch(size) {
    case 0:
      return MutateArithmeticValue<uint16_t>(inout_sample, prng, flip_endian);
    case 1:
      return MutateArithmeticValue<uint32_t>(inout_sample, prng, flip_endian);
    case 2:
      return MutateArithmeticValue<uint64_t>(inout_sample, prng, flip_endian);
  }
  return true;
}

template<typename T>
bool ArithmeticMutator::MutateArithmeticValue(Sample *inout_sample,
                                              PRNG *prng,
                                              int flip_endian)
{
  T value;
  size_t blockstart, blocksize;
  if (!GetRandBlock(inout_sample->size,
                    sizeof(T), sizeof(T),
                    &blockstart, &blocksize,
                    prng))
    return true;
  value = *(T *)(inout_sample->bytes + blockstart);
  if(flip_endian) value = FlipEndian(value);
  int change = prng->Rand(-256, 256);
  value += change;
  if(flip_endian) value = FlipEndian(value);
  *(T *)(inout_sample->bytes + blockstart) = value;
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
  if (old_size >= Sample::max_size) return true;
  size_t append = prng->Rand(min_append, max_append);
  if ((old_size + append) > Sample::max_size) {
    append = Sample::max_size - old_size;
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
  if (old_size >= Sample::max_size) return true;
  size_t to_insert = prng->Rand(min_insert, max_insert);
  if ((old_size + to_insert) > Sample::max_size) {
    to_insert = Sample::max_size - old_size;
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
  if (inout_sample->size >= Sample::max_size) return true;
  size_t blockpos, blocksize;
  if (!GetRandBlock(inout_sample->size, min_block_size, max_block_size, &blockpos, &blocksize, prng)) return true;
  int64_t blockcount = prng->Rand(min_duplicate_cnt, max_duplicate_cnt);
  if ((inout_sample->size + blockcount * blocksize) > Sample::max_size)
    blockcount = (Sample::max_size - (int64_t)inout_sample->size) / blocksize;
  if (blockcount <= 0) return true;
  char *newbytes;
  newbytes = (char *)malloc(inout_sample->size + blockcount * blocksize);
  memcpy(newbytes, inout_sample->bytes, blockpos + blocksize);
  for (int64_t i = 0; i<blockcount; i++) {
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

void Mutator::AddInterestingValue(char *data, size_t size, std::vector<Sample>& interesting_values) {
  Sample interesting_sample;
  interesting_sample.Init(data, size);
  interesting_values.push_back(interesting_sample);
}

bool InterestingValueMutator::Mutate(Sample *inout_sample, PRNG *prng, std::vector<Sample *> &all_samples) {
  // printf("In InterestingValueMutator::Mutate\n");
  if (interesting_values.empty()) return true;
  Sample *interesting_sample = &interesting_values[prng->Rand(0, (int)interesting_values.size() - 1)];
  size_t blockstart, blocksize;
  if (!GetRandBlock(inout_sample->size, interesting_sample->size, interesting_sample->size, &blockstart, &blocksize, prng)) return true;
  memcpy(inout_sample->bytes + blockstart, interesting_sample->bytes, interesting_sample->size);
  return true;
}

InterestingValueMutator::InterestingValueMutator(bool use_default_values) {
  if (use_default_values) {
    AddDefaultInterestingValues<uint16_t>(interesting_values);
    AddDefaultInterestingValues<uint32_t>(interesting_values);
    // AddDefaultInterestingValues<uint64_t>(interesting_values);
  }
}

template<typename T> void Mutator::AddDefaultInterestingValues(std::vector<Sample>& interesting_values) {
  uint32_t M[] = {2, 3, 4, 6, 8, 10, 12, 16, 24, 32, 40, 48,
                  56, 64, 72, 80, 88, 96, 104, 112, 120, 128,
                  136, 144, 152, 160, 168, 176, 184, 192, 200,
                  208, 216, 224, 232, 240, 248, 256 };

  int32_t N[] = {1, 2, 3, 4, 6, 8, 10, 12, 16, 32, 64, 128, 256};

  T value;
  value = 0;
  AddInterestingValue((char *)(&value), sizeof(value), interesting_values);

  value = 1;
  for (uint32_t i = 0; i < (sizeof(value) * 8); i++) {
    AddInterestingValue((char *)(&value), sizeof(value), interesting_values);
    value = (value << 1);
  }

  for (uint32_t i = 0; i < (sizeof(M)/sizeof(M[0])); i++) {
    int32_t m = M[i];
    value = (T)(-1) / m + 1;
    AddInterestingValue((char *)(&value), sizeof(value), interesting_values);
    value = FlipEndian(value);
    AddInterestingValue((char *)(&value), sizeof(value), interesting_values);
  }
    
  for (uint32_t j = 0; j < (sizeof(N)/sizeof(N[0])); j++) {
    int32_t n = N[j];
    value = (T)(0) - n;
    AddInterestingValue((char *)(&value), sizeof(value), interesting_values);
    value = FlipEndian(value);
    AddInterestingValue((char *)(&value), sizeof(value), interesting_values);
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
      if (inout_sample->size > Sample::max_size) inout_sample->Trim(Sample::max_size);
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
    if(new_sample_size > Sample::max_size) {
      new_sample_size = Sample::max_size;
      new_bytes = (char *)realloc(new_bytes, Sample::max_size);
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

void BaseDeterministicContext::AddHotOffset(size_t offset) {
  mutex.Lock();

  // in any case, restart scan
  cur_region = 0;
  
  MutateRegion new_region;
  new_region.cur_progress = 0;

  size_t newregion_start = offset;
  if(newregion_start < DETERMINISTIC_MUTATE_BYTES_PREVIOUS) newregion_start = 0;
  else newregion_start -= DETERMINISTIC_MUTATE_BYTES_PREVIOUS;
  size_t newregion_end = offset + DETERMINISTIC_MUTATE_BYTES_NEXT;
  
  for(auto iter = regions.begin(); iter != regions.end(); iter++) {
    if(newregion_start < iter->start) {
      new_region.start = newregion_start;
      new_region.cur = new_region.start;
      if(iter->start > newregion_end) {
        new_region.end = newregion_end;
      } else {
        new_region.end = iter->start;
      }
      regions.insert(iter, new_region);
      mutex.Unlock();
      return;
    }
    if(newregion_start <= iter->end) {
      if(newregion_end <= iter->end) {
        mutex.Unlock();
        return;
      }
      // extend an existing region
      iter->end = newregion_end;
      mutex.Unlock();
      return;
    }
  }
  new_region.start = newregion_start;
  new_region.cur = new_region.start;
  new_region.end = newregion_end;
  regions.push_back(new_region);
  mutex.Unlock();
  return;
}

bool BaseDeterministicContext::GetNextByteToMutate(size_t *pos, size_t *progress, size_t max_progress) {
  MutateRegion *region = NULL;
  
  while(cur_region < regions.size()) {
    region = &(regions[cur_region]);

    if(region->cur_progress >= max_progress) {
      region->cur_progress = 0;
      region->cur++;
    }

    if(region->cur >= region->end) {
      cur_region++;
      continue;
    }

    *pos = region->cur;
    *progress = region->cur_progress;
    region->cur_progress++;
    return true;
  }
  return false;
}


MutatorSampleContext *BaseDeterministicMutator::CreateSampleContext(Sample *sample) {
  BaseDeterministicContext *context = new BaseDeterministicContext;
  return context;
}

bool DeterministicByteFlipMutator::Mutate(Sample *inout_sample, PRNG *prng, std::vector<Sample *> &all_samples) {
  size_t pos;
  size_t value;
  
  if(!context->GetNextByteToMutate(&pos, &value, 256)) {
    return false;
  }
  
  if(pos >= inout_sample->size) {
    inout_sample->Resize(pos + 1);
  }
  inout_sample->bytes[pos] = (char)(value);
  
  return true;
}

DeterministicInterestingValueMutator::DeterministicInterestingValueMutator(bool use_default_values) {
  if (use_default_values) {
    AddDefaultInterestingValues<uint16_t>(interesting_values);
    AddDefaultInterestingValues<uint32_t>(interesting_values);
    // AddDefaultInterestingValues<uint64_t>(interesting_values);
  }
}

bool DeterministicInterestingValueMutator::Mutate(Sample *inout_sample, PRNG *prng, std::vector<Sample *> &all_samples) {
  size_t pos;
  size_t value_index;
  
  if(!context->GetNextByteToMutate(&pos, &value_index, interesting_values.size())) {
    return false;
  }
  
  Sample *interesting_sample = &interesting_values[value_index];
  if((pos + interesting_sample->size) > inout_sample->size) {
    inout_sample->Resize(pos + interesting_sample->size);
  }
  memcpy(inout_sample->bytes + pos, interesting_sample->bytes, interesting_sample->size);
  
  return true;
}

bool RangeMutator::Mutate(Sample* inout_sample, PRNG* prng, std::vector<Sample*>& all_samples) {
  Mutator* child_mutator = child_mutators[0];

  if (ranges->empty()) {
    return child_mutator->Mutate(inout_sample, prng, all_samples);
  }

  // pick a range
  Range& range = (*ranges)[prng->Rand() % ranges->size()];

  // printf("Mutating range %zd %zd\n", range.from, range.to);

  // extract the part we want to mutate
  Sample rangesample;
  inout_sample->Crop(range.from, range.to, &rangesample);

  // mutate the cropped sample (if not empty)
  if (inout_sample->size == 0) {
    return child_mutator->Mutate(inout_sample, prng, all_samples);
  } else {
    child_mutator->Mutate(&rangesample, prng, all_samples);
  }

  // put the cropped part back where it belongs
  if (range.from + rangesample.size > inout_sample->size) {
    inout_sample->Resize(range.from + rangesample.size);
  }
  memcpy(inout_sample->bytes + range.from, rangesample.bytes, rangesample.size);

  return true;
}

