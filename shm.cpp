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
#include "shm.h"

SharedMemory::SharedMemory() {
  size = 0;
  shm = NULL;
}

SharedMemory::SharedMemory(char* name, size_t size) {
  Open(name, size);
}

SharedMemory::~SharedMemory() {
  Close();
}


#if defined(WIN32) || defined(_WIN32) || defined(__WIN32)

void SharedMemory::Open(char* name, size_t size) {
  shm_handle = CreateFileMapping(
    INVALID_HANDLE_VALUE,
    NULL,
    PAGE_READWRITE,
    0,
    (DWORD)size,
    name);

  if (shm_handle == NULL) {
    FATAL("CreateFileMapping failed, %x", GetLastError());
  }

  shm = (unsigned char*)MapViewOfFile(
    shm_handle,          // handle to map object
    FILE_MAP_ALL_ACCESS, // read/write permission
    0,
    0,
    size
  );

  if (!shm) {
    FATAL("MapViewOfFile failed");
  }
}

void SharedMemory::Close() {
  UnmapViewOfFile(shm);
  CloseHandle(shm_handle);
}

#else

#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>

void SharedMemory::Open(char* name, size_t size) {
#if defined(__ANDROID__) && !defined(ANDROID_VM)
  FATAL("Shared memory is not implemented on Android");
#else
  int res;

  this->size = size;
  size_t name_size = strlen(name);
  this->name = (char*)malloc(name_size + 1);
  strcpy(this->name, name);

  // get shared memory file descriptor (NOT a file)
#ifdef ANDROID_VM
  fd = open(name, O_RDWR|O_CREAT, 0666);
#else  
  fd = shm_open(name, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
#endif
  if (fd == -1)
  {
    FATAL("Error creating shared memory");
  }

  // extend shared memory object as by default it's initialized with size 0
  res = ftruncate(fd, size);
  if (res == -1)
  {
    FATAL("Error creating shared memory");
  }

  // map shared memory to process address space
  shm = (unsigned char*)mmap(NULL, size, PROT_WRITE, MAP_SHARED, fd, 0);
  if (shm == MAP_FAILED)
  {
    FATAL("Error creating shared memory");
  }
#endif
}

void SharedMemory::Close() {
#if defined(__ANDROID__) && !defined(ANDROID_VM)
  FATAL("Shared memory is not implemented on Android");
#else
  munmap(shm, size);
#ifndef ANDROID_VM 
  shm_unlink(name);
#endif
  close(fd);
  free(name);
#endif
}

#endif
