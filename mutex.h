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

// uses CRITICAL_SECTION on Winsows for speed
// std::mutex on other platforms

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32)
#include <windows.h>
#else
#include <mutex>
#endif

class Mutex {
public:
  Mutex();
  void Lock();
  void Unlock();

private:
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32)
  CRITICAL_SECTION cs;
#else
  std::mutex mutex;
#endif
};

//Readers-writers mutex with no thread starvation
//see http://en.wikipedia.org/wiki/Readers-writers_problem
class ReadWriteMutex {
private:
  Mutex *no_writers, *no_readers, *counter_mutex;
  int nreaders;

public:

  ReadWriteMutex();
  ~ReadWriteMutex();

  //lock data for reading only, other readers possible, but no writers
  void LockRead();

  //unlocks data after LockRead
  void UnlockRead();

  //lock data for writing, no other readers or writers possible
  void LockWrite();

  //unlocks data after LockWrite
  void UnlockWrite();
};

