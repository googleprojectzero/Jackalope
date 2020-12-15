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

#include "thread.h"

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32)
#include <windows.h>


void CreateThread(void *(*start_routine) (void *), void *arg) {
  CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)start_routine, arg, 0, NULL);
}

#else
#include <pthread.h>

void CreateThread(void *(*start_routine) (void *), void *arg) {
  pthread_t thread_id;
  pthread_create(&thread_id, NULL, start_routine, arg);
}

#endif
