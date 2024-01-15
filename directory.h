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

#include <string>
#include <vector>

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32)
#include <windows.h>
#include <direct.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#endif

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32)
#define DIR_SEPARATOR '\\'
#else
#define DIR_SEPARATOR '/'
#endif

size_t GetFilesInDirectory(std::string directory, std::list<std::string> &list);
size_t GetFilesInDirectorySync(std::string directory, std::list<std::string> &list, uint64_t &offset);
std::string DirJoin(std::string dir1, std::string dir2);
int CreateDirectory(std::string &directory);

