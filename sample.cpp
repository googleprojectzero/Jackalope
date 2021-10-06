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
#include "mutex.h"

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

void Sample::Init(size_t size) {
  if(bytes) free(bytes);
  this->size = size;
  bytes = (char *)malloc(size);
  memset(bytes,0,size);
}

void Sample::Append(char *data, size_t size) {
  size_t oldsize = this->size;
  this->size += size;
  bytes = (char *)realloc(bytes,this->size);
  memcpy(bytes+oldsize,data,size);
}

void Sample::Trim(size_t new_size) {
  if (new_size > this->size) return;
  this->size = new_size;
  if(new_size == 0) {
    free(bytes);
    bytes = NULL;
  } else {
    bytes = (char *)realloc(bytes, this->size);
  }
}

void Sample::Resize(size_t new_size) {
  if(new_size == size) return;
  if(new_size < size) {
    Trim(new_size);
    return;
  } else {
    size_t old_size = size;
    this->size = new_size;
    bytes = (char *)realloc(bytes, this->size);
    memset(bytes + old_size, 0, new_size - old_size);
  }
}

size_t Sample::FindFirstDiff(Sample &other) {
  size_t minsize = size;
  if(other.size < minsize) minsize = other.size;
  for(size_t i=0; i<minsize; i++) {
    if(bytes[i] != other.bytes[i]) return i;
  }
  return minsize;
}

static SampleTrie sample_trie;

SampleTrie::SampleTrieNode::SampleTrieNode() {
  leaf = true;
  constant_part = NULL;
  constant_part_size = 0;
}

SampleTrie::SampleTrieNode::~SampleTrieNode() {
  if(constant_part) free(constant_part);
  for(auto iter = children.begin(); iter != children.end(); iter++) {
    delete iter->second;
  }
}

Mutex SampleTrie::sample_trie_mutex;

void SampleTrie::SampleTrieNode::InitConstantPart(Sample *sample,
                                                  size_t from, size_t to)
{
  if(constant_part) free(constant_part);
  constant_part = NULL;
  constant_part_size = to - from;
  if(!constant_part_size) return;
  constant_part = (char *)malloc(constant_part_size);
  memcpy(constant_part, sample->bytes + from, constant_part_size);
}

size_t SampleTrie::AddSample(Sample *sample) {
  if(sample->size == 0) return 0;
  
  sample_trie_mutex.Lock();
  
  if(root == NULL) {
    root = new SampleTrieNode;
    root->InitConstantPart(sample, 0, sample->size);
    
    sample_trie_mutex.Unlock();
    return 0;
  }
  
  SampleTrieNode *cur_node = root;
  size_t cur_sample_pos = 0;
  size_t cur_constant_pos = 0;
  
  while(1) {
    
   if(cur_sample_pos >= sample->size) {
      // normally, we'd need to split the current node
      // and mark it as leaf
      // but for the purpose of this trie there is no need
      // as we just want to know where one sample differs
      // from the rest
     sample_trie_mutex.Unlock();
      return sample->size;
    }
      
    unsigned char cur_char = (unsigned char)sample->bytes[cur_sample_pos];
    if(cur_constant_pos >= cur_node->constant_part_size) {
      auto iter = cur_node->children.find(cur_char);
      if(iter != cur_node->children.end()) {
        cur_node = iter->second;
        cur_sample_pos++;
        cur_constant_pos = 0;
        continue;
      } else {
        size_t ret = cur_sample_pos;
        SampleTrieNode *new_node = new SampleTrieNode;
        new_node->InitConstantPart(sample, cur_sample_pos+1, sample->size);
        cur_node->children[cur_char] = new_node;
        
        sample_trie_mutex.Unlock();
        return cur_sample_pos;
      }
    }
      
    unsigned char trie_char = (unsigned char)cur_node->constant_part[cur_constant_pos];
    if(trie_char == cur_char) {
      cur_sample_pos++;
      cur_constant_pos++;
      continue;
    } else {
      SampleTrieNode *new_node1 = new SampleTrieNode;
      new_node1->constant_part_size = cur_node->constant_part_size - cur_constant_pos - 1;
      if(new_node1->constant_part_size) {
        new_node1->constant_part = (char *)malloc(new_node1->constant_part_size);
        memcpy(new_node1->constant_part, cur_node->constant_part + cur_constant_pos + 1, new_node1->constant_part_size);
      }
      new_node1->children = cur_node->children;
      cur_node->children.clear();
      cur_node->constant_part_size = cur_constant_pos;
      cur_node->constant_part = (char *)realloc(cur_node->constant_part, cur_constant_pos);
      cur_node->children[trie_char] = new_node1;
        
      SampleTrieNode *new_node2 = new SampleTrieNode;
      new_node2->InitConstantPart(sample, cur_sample_pos+1, sample->size);
      cur_node->children[cur_char] = new_node2;
      
      sample_trie_mutex.Unlock();
      return cur_sample_pos;
    }
  }
  
  sample_trie_mutex.Unlock();
}
