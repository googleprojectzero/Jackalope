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

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32)
#include "windows.h"
#endif

class SharedMemory {
public:
  SharedMemory();
  SharedMemory(char* name, size_t size);
  ~SharedMemory();

  void Open(char* name, size_t size);
  void Close();

  size_t GetSize() { return size; }
  unsigned char* GetData() { return shm; }

protected:
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32)
  HANDLE shm_handle;
#else
  int fd;
#endif
  size_t size;
  unsigned char* shm;
};
