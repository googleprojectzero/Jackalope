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
#include "rangetracker.h"

#include <algorithm>

void ConstantRangeTracker::ExtractRanges(std::vector<Range>* ranges) {
  ranges->push_back({ from, to });
}

SHMRangeTracker::SHMRangeTracker(char* name, size_t size) {
  shm.Open(name, size);
  data = (uint32_t *)shm.GetData();
  data[0] = 0;
  max_ranges = (size - sizeof(uint32_t)) / sizeof(uint32_t);
}

SHMRangeTracker::~SHMRangeTracker() {
  shm.Close();
}

void SHMRangeTracker::ExtractRanges(std::vector<Range>* ranges) {
  uint32_t* buf = data;
  size_t numranges = *buf; buf++;

  if (!numranges) return;

  if (numranges > max_ranges) {
    WARN("Number of ranges exceeds buffer size.");
    numranges = max_ranges;
  }

  std::vector<Range> tmpranges;
  tmpranges.resize(numranges);
  for (size_t i = 0; i < numranges; i++) {
    tmpranges[i].from = *buf; buf++;
    tmpranges[i].to = *buf; buf++;
  }

  ConsolidateRanges(tmpranges, *ranges);
}

void SHMRangeTracker::ConsolidateRanges(std::vector<Range>& inranges, std::vector<Range>& outranges) {
  if (inranges.empty()) return;

  std::sort(inranges.begin(), inranges.end());

  Range* lastrange = NULL;
  Range* currange = NULL;

  outranges.push_back(inranges[0]);

  for (size_t i = 1; i < inranges.size(); i++) {
    lastrange = &outranges[outranges.size() - 1];
    currange = &inranges[i];

    if (currange->from <= lastrange->to) {
      if (currange->to > lastrange->to) {
        lastrange->to = currange->to;
      }
    } else {
      outranges.push_back(*currange);
    }
  }

  // printf("SHMRangeTracker::ConsolidateRanges %zd %zd\n", inranges.size(), outranges.size());
}
