# Copyright 2020 Google LLC
# 
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
# 
#     https://www.apache.org/licenses/LICENSE-2.0
# 
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

cmake_minimum_required(VERSION "3.1")

set(BUILD_SHARED_LIBS OFF)
set(LIBXSLT_WITH_TESTS OFF)
set(LIBXML2_WITH_TESTS OFF)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -fsanitize=address,undefined -fno-sanitize=float-divide-by-zero,pointer-overflow -fno-sanitize-recover=all -fsanitize-coverage=trace-pc-guard")

add_subdirectory(libxml2)
add_subdirectory(libxslt)

project("harness")

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/libxml2/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/libxslt)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../)

add_executable(harness
  harness.c
  sancovclient.c
)

target_link_libraries(harness LibXml2::LibXml2)
target_link_libraries(harness LibXslt)
target_link_libraries(harness LibExslt)

