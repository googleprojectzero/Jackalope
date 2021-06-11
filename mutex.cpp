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

#include "mutex.h"

Mutex::Mutex() {
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32)
  InitializeCriticalSection(&cs);
#endif
}

void Mutex::Lock() {
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32)
  EnterCriticalSection(&cs);
#else
  mutex.lock();
#endif
}

void Mutex::Unlock() {
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32)
  LeaveCriticalSection(&cs);
#else
  mutex.unlock();
#endif
}

ReadWriteMutex::ReadWriteMutex() {
  no_writers = new Mutex();
  no_readers = new Mutex();
  counter_mutex = new Mutex();
}

ReadWriteMutex::~ReadWriteMutex() {
  delete no_writers;
  delete no_readers;
  delete counter_mutex;
}

//lock data for writing, no other readers or writers possible
void ReadWriteMutex::LockWrite() {
  no_writers->Lock();
  no_readers->Lock();
  no_readers->Unlock();
}

//unlocks data after LockWrite
void ReadWriteMutex::UnlockWrite() {
  no_writers->Unlock();
}

//lock data for reading only, other readers possible, but no writers
void ReadWriteMutex::LockRead() {
  int prev;

  no_writers->Lock();
  counter_mutex->Lock();
  prev = nreaders;
  nreaders = nreaders + 1;
  counter_mutex->Unlock();
  if (prev == 0) no_readers->Lock();
  no_writers->Unlock();
}

//unlocks data after LockRead
void ReadWriteMutex::UnlockRead() {
  int current;

  counter_mutex->Lock();
  nreaders = nreaders - 1;
  current = nreaders;
  counter_mutex->Unlock();
  if (current == 0) no_readers->Unlock();
}
