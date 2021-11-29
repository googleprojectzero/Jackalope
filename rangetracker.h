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

// sufficient for 1000-1 ranges
#define RANGE_SHM_SIZE 4096

#include <vector>

#include "range.h"
#include "shm.h"

class RangeTracker {
public:
  RangeTracker() {}
  virtual ~RangeTracker() {}
  virtual void ExtractRanges(std::vector<Range>* ranges) {}
};


class ConstantRangeTracker : public RangeTracker {
public:
  ConstantRangeTracker(size_t from, size_t to) {
    this->from = from;
    this->to = to;
  }

  virtual void ExtractRanges(std::vector<Range>* ranges) override;

protected:
  size_t from;
  size_t to;
};

class SHMRangeTracker : public RangeTracker {
public:
  SHMRangeTracker(char* name, size_t size);
  ~SHMRangeTracker();

  virtual void ExtractRanges(std::vector<Range>* ranges) override;

protected:
  void ConsolidateRanges(std::vector<Range>& inranges, std::vector<Range>& outranges);

  SharedMemory shm;
  uint32_t* data;
  size_t max_ranges;
};
