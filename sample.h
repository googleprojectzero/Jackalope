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

#include <stdio.h>
#include <unordered_map>
#include <string>

#include "mutex.h"

#define MAX_SAMPLE_SIZE 1000000

class Sample {
public:
  char *bytes;
  size_t size;
  std::string filename;

  Sample();
  ~Sample();
  Sample(const Sample &in);
  Sample& operator= (const Sample &in);

  void Clear();

  int Save(const char * filename);

  int Save();
  int Load();

  void FreeMemory();
  void EnsureLoaded();

  void Save(FILE * fp);

  int Load(const char * filename);

  void Init(const char *data, size_t size);
  void Init(size_t size);

  void Append(char *data, size_t size);

  void Trim(size_t new_size);
  
  void Resize(size_t new_size);
  
  void Crop(size_t from, size_t to, Sample* out);

  size_t FindFirstDiff(Sample &other);
};

// a Trie-like structure whose purpose is to be able to
// quickly identify the first byte of a sample
// that differs from the samples seen so far
class SampleTrie {
public:
  SampleTrie() {
    root = NULL;
  }
  
  size_t AddSample(Sample *sample);

protected:
  struct SampleTrieNode {
    SampleTrieNode();
    ~SampleTrieNode();
    
    void InitConstantPart(Sample *sample, size_t from, size_t to);

    char *constant_part;
    size_t constant_part_size;

    std::unordered_map<unsigned char, SampleTrieNode*> children;
    bool leaf;
  };
  
  static Mutex sample_trie_mutex;
  SampleTrieNode *root;
};

