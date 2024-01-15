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

#include <string.h>
#include <string>
#include <list>
#include <regex>
#include <cstdint>
#include <iostream>
#include "directory.h"

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32)

size_t GetFilesInDirectory(std::string directory, std::list<std::string> &list)
{
  std::string dir = directory;

  // append the separator to out_dir
  if (dir.size() == 0) {
    dir = DIR_SEPARATOR;
  } else {
    if (dir.back() != DIR_SEPARATOR) {
      dir += DIR_SEPARATOR;
    }
  }

  // get all files
  std::string search = dir + "*";

  WIN32_FIND_DATAA fd;
  HANDLE h = FindFirstFileA(search.c_str(), &fd);

  if (h == INVALID_HANDLE_VALUE) return 0;

  do {
    if (strcmp(fd.cFileName, ".") == 0) continue;
    if (strcmp(fd.cFileName, "..") == 0) continue;

    list.push_back(dir + fd.cFileName);
  } while (FindNextFileA(h, &fd));

  return list.size();
}

int CreateDirectory(std::string &directory) {
  return(!_mkdir(directory.c_str()));
}


#else

size_t GetFilesInDirectory(std::string directory, std::list<std::string> &list)
{
  struct dirent **listing;
  int n;

  n = scandir(directory.c_str(), &listing, NULL, alphasort);

  for(int i = 0; i < n; i++) {
    if (strcmp(listing[i]->d_name, ".") == 0) continue;
    if (strcmp(listing[i]->d_name, "..") == 0) continue;
    if (strcmp(listing[i]->d_name, ".DS_Store") == 0) continue;

    list.push_back(DirJoin(directory, listing[i]->d_name));
  }

  return list.size();
}

// for sync fuzzers file (afl format), return the "fuzzers_sync/{file}".
size_t GetFilesInDirectorySync(std::string directory, std::list<std::string> &list, uint64_t &offset)
{
  struct dirent **listing;
  int n;

  n = scandir(directory.c_str(), &listing, NULL, alphasort);
  // std::cout << "Synchronizing : " << directory << "\n";

  for(int i = 0; i < n; i++) {
    // only find the queue file (afl format)
    if (strcmp(listing[i]->d_name, ".") == 0) continue;
    if (strcmp(listing[i]->d_name, "..") == 0) continue;
    if (strncmp(listing[i]->d_name, "id:", 2)) continue;

    // match id.
    std::regex id_regex("id:([0-9]+)");
    std::smatch match;
    std::string input(listing[i]->d_name);
    // std::cout << "Synchronizing filename: " << input << "\n";
    if (std::regex_search(input, match, id_regex) && match.size() > 1) {
        std::string id_str = match.str(1);
        uint64_t id = std::stoull(id_str);
        // std::cout << "Matched sync file ID: " << id << "\n";
        if (id > offset)
          offset = id;
        else
          continue;
    } else {
      continue;
    }

    list.push_back(DirJoin("fuzzers_sync", listing[i]->d_name));
  }

  return list.size();
}


int CreateDirectory(std::string &directory) {
  return(!mkdir(directory.c_str(), 0755));
}

#endif

std::string DirJoin(std::string dir1, std::string dir2) {
  if (dir1.empty()) return dir2;

  std::string ret = dir1;

  if (ret.back() != DIR_SEPARATOR) {
    ret += DIR_SEPARATOR;
  }

  ret += dir2;

  return ret;
}
