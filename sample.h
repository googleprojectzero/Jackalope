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

#define MAX_SAMPLE_SIZE 1000000

class Sample {
public:
  char *bytes;
  size_t size;

  Sample();
  ~Sample();
  Sample(const Sample &in);
  Sample& operator= (const Sample &in);

  int Save(const char * filename);

  void Save(FILE * fp);

  int Load(const char * filename);

  void Init(const char *data, size_t size);

  void Append(char *data, size_t size);

  void Trim(size_t new_size);
};
