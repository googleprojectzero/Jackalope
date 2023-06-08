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

#include "string.h"
#include "common.h"
#include "instrumentation.h"

#include <sstream>

std::string Instrumentation::AnonymizeAddress(void* addr) {
  char buf[20];
  sprintf(buf, "%p", addr);

  if(!strcmp(buf, "(nil)")) return std::string("0");

  int addr_start = 0;
  if(buf[0] == '0' && ((buf[1] == 'x') || (buf[1] == 'X'))) addr_start = 2;

  int len = (int)strlen(buf);
  int firstnonzero = len;
  for (int i = addr_start; i < len; i++) {
    if (buf[i] != '0') {
      firstnonzero = i;
      break;
    }
  }
  for (int i = firstnonzero; i < len - 3; i++) {
    buf[i] = 'x';
  }
  return std::string(buf);
}

