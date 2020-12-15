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

#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sample.h"


Sample::Sample() {
  size = 0;
  bytes = NULL;
}

Sample::~Sample() {
  if(bytes) free(bytes);
}

Sample::Sample(const Sample &in) {
  size = in.size;
  bytes = (char *)malloc(size);
  memcpy(bytes,in.bytes,size);
}

Sample& Sample::operator= (const Sample &in) {
  if(bytes) free(bytes);
  size = in.size;
  bytes = (char *)malloc(size);
  memcpy(bytes,in.bytes,size);
  return *this;
}

int Sample::Save(const char * filename) {
  FILE *fp;
  fp = fopen(filename,"wb");
  if(!fp) {
    return 0;
  }
  fwrite(bytes, size, 1, fp);
  fclose(fp);
  return 1;
}

void Sample::Save(FILE * fp) {
  fwrite(bytes, size, 1, fp);
}

int Sample::Load(const char * filename) {
  FILE *fp;
  fp = fopen(filename,"rb");
  if(!fp) {
    return 0;
  }
  fseek(fp,0,SEEK_END);
  size = ftell(fp);
  fseek(fp,0,SEEK_SET);
  if(bytes) free(bytes);
  bytes = (char *)malloc(size);
  fread(bytes, size, 1, fp);
  fclose(fp);
  return 1;
}

void Sample::Init(const char *data, size_t size) {
  if(bytes) free(bytes);
  this->size = size;
  bytes = (char *)malloc(size);
  memcpy(bytes,data,size);
}

void Sample::Append(char *data, size_t size) {
  size_t oldsize = this->size;
  this->size += size;
  bytes = (char *)realloc(bytes,this->size);
  memcpy(bytes+oldsize,data,size);
}

void Sample::Trim(size_t new_size) {
  if ((new_size < 0) || (new_size > this->size)) new_size = this->size;
  this->size = new_size;
  bytes = (char *)realloc(bytes, this->size);
}
