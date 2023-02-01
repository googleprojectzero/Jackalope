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

#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <errno.h>

#include "sancovclient.h"

#define FUZZ_CHILD_CTRL_IN 1000
#define FUZZ_CHILD_CTRL_OUT 1001

#define COV_SHM_SIZE 0x100000
#define MAX_EDGES ((COV_SHM_SIZE - 4) * 8)

#define CHECK(cond) if (!(cond)) { fprintf(stderr, "\"" #cond "\" failed\n"); _exit(-1); }

#define SAY(...)    printf(__VA_ARGS__)

#define WARN(...) do { \
    SAY("[!] WARNING: " __VA_ARGS__); \
    SAY("\n"); \
  } while (0)

#define FATAL(...) do { \
    SAY("[-] PROGRAM ABORT : " __VA_ARGS__); \
    SAY("         Location : %s(), %s:%u\n\n", \
         __FUNCTION__, __FILE__, __LINE__); \
    exit(1); \
  } while (0)

struct cov_shmem_data {
    uint32_t num_edges;
    unsigned char edges[];
};

struct cov_shmem_data* cov_shmem;
uint32_t *__edges_start, *__edges_stop;

void __sanitizer_cov_reset_edgeguards() {
    uint64_t N = 0;
    for (uint32_t *x = __edges_start; x < __edges_stop && N < MAX_EDGES; x++)
        *x = ++N;
}

extern char **environ;
bool fuzzer = false;

extern "C" void __sanitizer_cov_trace_pc_guard_init(uint32_t *start, uint32_t *stop) {
    // Avoid duplicate initialization
    if (start == stop || *start)
        return;

    if (__edges_start != NULL || __edges_stop != NULL) {
        fprintf(stderr, "Coverage instrumentation is only supported for a single module\n");
        _exit(-1);
    }

    __edges_start = start;
    __edges_stop = stop;

    // Map the shared memory region
    const char* shm_key = getenv("COV_SHM_ID");
    if (!shm_key) {
        puts("[COV] no shared memory bitmap available, skipping");
        cov_shmem = (struct cov_shmem_data*) malloc(COV_SHM_SIZE);
    } else {
        fuzzer = true;
        int fd = shm_open(shm_key, O_RDWR, S_IREAD | S_IWRITE);
        if (fd <= -1) {
            fprintf(stderr, "Failed to open shared memory region: %s\n", strerror(errno));
            _exit(-1);
        }

        cov_shmem = (struct cov_shmem_data*) mmap(0, COV_SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if (cov_shmem == MAP_FAILED) {
            fprintf(stderr, "Failed to mmap shared memory region\n");
            _exit(-1);
        }
    }

    __sanitizer_cov_reset_edgeguards();

    cov_shmem->num_edges = stop - start;
    printf("[COV] edge counters initialized. Shared memory: %s with %u edges\n", shm_key, cov_shmem->num_edges);
}

extern "C" void __sanitizer_cov_trace_pc_guard(uint32_t *guard) {
    // There's a small race condition here: if this function executes in two threads for the same
    // edge at the same time, the first thread might disable the edge (by setting the guard to zero)
    // before the second thread fetches the guard value (and thus the index). However, our
    // instrumentation ignores the first edge (see libcoverage.c) and so the race is unproblematic.
    uint32_t index = *guard;
    // printf("%d\n", index);
    // If this function is called before coverage instrumentation is properly initialized we want to return early.
    if (!index) return;
    cov_shmem->edges[index / 8] |= 1 << (index % 8);
    *guard = 0;
}

void __pre_fuzz() {
  // printf("__pre_fuzz\n");
  if(!fuzzer) return;
  __sanitizer_cov_reset_edgeguards();
  int ret;
  char status;
  status = 'k';
  ret = write(FUZZ_CHILD_CTRL_OUT, &status, 1);
  if(ret != 1) _exit(0);
  ret = read(FUZZ_CHILD_CTRL_IN, &status, 1);
  if((ret!=1) || (status != 'c')) _exit(0);
}

void __post_fuzz(uint64_t return_value) {
  // printf("__post_fuzz\n");
  if(!fuzzer) {
    printf("Done\n");
    exit(0);
  }
  int ret;
  char status;
  status = 'd';
  ret = write(FUZZ_CHILD_CTRL_OUT, &status, 1);
  if(ret != 1) _exit(0);
  ret = write(FUZZ_CHILD_CTRL_OUT, &return_value, sizeof(return_value));
  if(ret != sizeof(return_value)) _exit(0);
  ret = read(FUZZ_CHILD_CTRL_IN, &status, 1);
  if((ret!=1) || (status != 'c')) _exit(0);
}


