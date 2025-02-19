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
set (CMAKE_CXX_STANDARD 17)

add_subdirectory(TinyInst)

# Determine whether TinyInst should be build for arm64 or x86
if (WIN32)
  # no support for arm64 on Windows
elseif(ANDROID_ABI)
  set(ANDROID TRUE)
  if(${ANDROID_ABI} MATCHES "arm64")
    set(ARCHITECTURE ${ANDROID_ABI})
    add_definitions(-DARM64)
  endif()
  if(${ANDROID_TARGET} MATCHES "VM")
    add_definitions(-DANDROID_VM)
  endif()  
elseif(NOT DEFINED ${ARCHITECTURE})
  EXECUTE_PROCESS(COMMAND uname -m COMMAND tr -d '\n' OUTPUT_VARIABLE ARCHITECTURE)
  if(${ARCHITECTURE} MATCHES arm64)
    add_definitions(-DARM64)
  endif()
endif()

if(UNIX AND NOT APPLE AND NOT ANDROID)
  set(LINUX TRUE)
endif()

project("fuzzer")

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/third_party/Mersenne)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/TinyInst)

if(LINUX)
  set(platform_specific_sources
    sancovinstrumentation.h
    sancovinstrumentation.cpp
  )
else()
  set(platform_specific_sources
  )
endif()

add_library(fuzzerlib STATIC
  client.cpp
  client.h
  directory.cpp
  directory.h
  fuzzer.cpp
  fuzzer.h
  instrumentation.cpp
  instrumentation.h
  tinyinstinstrumentation.h
  tinyinstinstrumentation.cpp
  mutator.cpp
  mutator.h
  minimizer.cpp
  minimizer.h
  mutex.cpp
  mutex.h
  prng.cpp
  prng.h
  third_party/Mersenne/mersenne.cpp
  third_party/Mersenne/mersenne.h
  runresult.h
  sample.cpp
  sample.h
  sampledelivery.cpp
  sampledelivery.h
  server.cpp
  server.h
  thread.cpp
  thread.h
  shm.cpp
  shm.h
  range.h
  rangetracker.h
  rangetracker.cpp
  mutators/grammar/grammar.h
  mutators/grammar/grammar.cpp
  mutators/grammar/grammarmutator.h
  mutators/grammar/grammarmutator.cpp
  mutators/grammar/grammarminimizer.h
  mutators/grammar/grammarminimizer.cpp
  ${platform_specific_sources}
  )
  
if(LINUX)
  target_link_libraries(fuzzerlib rt)
  target_link_libraries(fuzzerlib pthread)
endif()

add_dependencies(fuzzerlib tinyinst)
target_link_libraries(fuzzerlib tinyinst)

if (WIN32)
  target_link_libraries(fuzzerlib "Ws2_32.lib")
endif()

add_executable(fuzzer
  main.cpp
)

target_link_libraries(fuzzer fuzzerlib)

add_executable(test
  test.cpp
  )
  
if(LINUX)
  target_link_libraries(test rt)
endif()

if(LINUX)
  if (NOT(CMAKE_CXX_COMPILER_ID STREQUAL "Clang"))
    message(FATAL_ERROR "You need to use Clang to compile TinyInst on Linux")
  endif()

  add_executable(sancovtest
    sancovclient.h
    sancovclient.cpp
    sancovtest.cpp
    )
  target_link_libraries(sancovtest rt)
  target_compile_options(sancovtest PRIVATE "-fsanitize-coverage=trace-pc-guard")
  target_compile_options(sancovtest PRIVATE "-fsanitize=address")
  target_link_options(sancovtest PRIVATE "-fsanitize=address")
endif()

add_subdirectory(examples)

