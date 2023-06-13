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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>

#include "common.h"
#include "coverage.h"
#include "sancovinstrumentation.h"
#include "directory.h"

#define ASAN_EXIT_STATUS 42

#define FUZZ_CHILD_CTRL_IN 1000
#define FUZZ_CHILD_CTRL_OUT 1001

extern char **environ;

#define COVERAGE_SHM_SIZE 0x100000
#define MAX_EDGES ((SHM_SIZE - 4) * 8)

#define unlikely(cond) __builtin_expect(!!(cond), 0)

SanCovInstrumentation::SanCovInstrumentation(int thread_id) {
  this->thread_id = thread_id;
  cov_shm = NULL;
  pid = 0;
  return_value = 0;
  module_name = "target";
}

SanCovInstrumentation::~SanCovInstrumentation() {
#ifdef __ANDROID__
  FATAL("SanCovInstrumentation is not implemented on Android");
#else
  if(cov_shm) {
    munmap(cov_shm, COVERAGE_SHM_SIZE);
    shm_unlink(coverage_shm_name.c_str());
    close(cov_shm_fd);
  }
#endif
}

void SanCovInstrumentation::Init(int argc, char **argv) {
  // create directory for ASAN reports
  std::string out_dir = GetOption("-out", argc, argv);
  std::string asan_report_dir = DirJoin(out_dir, "ASAN");
  CreateDirectory(asan_report_dir);
  asan_report_file = DirJoin(asan_report_dir, "log");

  // compute shm names
  sample_shm_name = std::string("/shm_fuzz_") + std::to_string(getpid()) + "_" + std::to_string(thread_id);
  coverage_shm_name = std::string("/shm_fuzz_coverage_") + std::to_string(getpid()) + "_" + std::to_string(thread_id);

  // set up child environment variables
  std::list<std::string> additional_env;
  additional_env.push_back(std::string("SAMPLE_SHM_ID=") + sample_shm_name);
  additional_env.push_back(std::string("COV_SHM_ID=") + coverage_shm_name);
  additional_env.push_back(std::string("ASAN_OPTIONS=exitcode=") + std::to_string(ASAN_EXIT_STATUS) + ":log_path=" + asan_report_file);
  ComputeEnvp(additional_env);
  
  // set up shmem for coverage
  SetUpShmem();
  
  virgin_bits = (uint8_t *)malloc(COVERAGE_SHM_SIZE);
  memset(virgin_bits, 0xff, COVERAGE_SHM_SIZE);
  
  num_iterations = GetIntOption("-iterations", argc, argv, 1);
  
  mute_child = GetBinaryOption("-mute_child", argc, argv, false);
  
}

void SanCovInstrumentation::SetUpShmem() {
#ifdef __ANDROID__
  FATAL("SanCovInstrumentation is not implemented on Android");
#else
  int res;
  
  // get shared memory file descriptor (NOT a file)
  cov_shm_fd = shm_open(coverage_shm_name.c_str(), O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
  if (cov_shm_fd == -1)
  {
    FATAL("Error creating shared memory");
  }

  // extend shared memory object as by default it's initialized with size 0
  res = ftruncate(cov_shm_fd, COVERAGE_SHM_SIZE);
  if (res == -1)
  {
    FATAL("Error creating shared memory");
  }

  // map shared memory to process address space
  cov_shm = (coverage_shmem_data *)mmap(NULL, COVERAGE_SHM_SIZE, PROT_WRITE, MAP_SHARED, cov_shm_fd, 0);
  if (cov_shm == MAP_FAILED)
  {
    FATAL("Error creating shared memory");
  }
  
  memset(cov_shm, 0, COVERAGE_SHM_SIZE);
#endif
}

void SanCovInstrumentation::ComputeEnvp(std::list<std::string> &additional_env) {
  int environ_size = 0;
  char **p = environ;
  while(*p) {
    environ_size += 1;
    p++;
  }

  int i;
  int envp_size = environ_size + additional_env.size();
  envp = (char**)malloc(sizeof(char*) * (envp_size + 1 ));
  for(i = 0; i < environ_size; ++i) {
    envp[i] = (char*)malloc(strlen(environ[i]) + 1);
    strcpy(envp[i], environ[i]);
  }

  for(auto iter = additional_env.begin(); iter != additional_env.end(); iter++) {
    envp[i] = (char*)malloc(iter->size() + 1);
    strcpy(envp[i], iter->c_str());
    i++;
  }
  
  envp[envp_size] = NULL;
}


void SanCovInstrumentation::StartTarget(int argc, char** argv) {
  int crpipe[2] = { 0, 0 };          // control pipe child -> reprl
  int cwpipe[2] = { 0, 0 };          // control pipe reprl -> child

  if (pipe(crpipe) != 0) {
    FATAL("Error creating pipe");
  }
  if (pipe(cwpipe) != 0) {
    FATAL("Error creating pipe");
  }

  ctrl_in = crpipe[0];
  ctrl_out = cwpipe[1];
  fcntl(ctrl_in, F_SETFD, FD_CLOEXEC);
  fcntl(ctrl_out, F_SETFD, FD_CLOEXEC);
  
  int pid = fork();
  if (pid == 0) {
    if (dup2(cwpipe[0], FUZZ_CHILD_CTRL_IN) < 0 ||
        dup2(crpipe[1], FUZZ_CHILD_CTRL_OUT) < 0)
    {
      FATAL("dup2 failed in the child");
    }

    close(cwpipe[0]);
    close(crpipe[1]);

    if(mute_child) {
      int devnull = open("/dev/null", O_RDWR);
      dup2(devnull, 1);
      dup2(devnull, 2);
      close(devnull);
    }

    // close all other FDs
#ifdef __ANDROID__
    int tablesize = sysconf(_SC_OPEN_MAX);
#else
    int tablesize = getdtablesize();
#endif
    for (int i = 3; i < tablesize; i++) {
      if (i == FUZZ_CHILD_CTRL_IN || i == FUZZ_CHILD_CTRL_OUT) {
        continue;
      }
      close(i);
    }
    

    execve(argv[0], argv, envp);

    FATAL("Failed to execute child process");
  }

  close(crpipe[1]);
  close(cwpipe[0]);
    
  if (pid < 0) {
    FATAL("Failed to fork");
  }
  
  this->pid = pid;
  
  cur_iteration = 0;
}

void SanCovInstrumentation::CleanupChild() {
    if (!pid) return;
    pid = 0;
    close(ctrl_in);
    close(ctrl_out);
}

void SanCovInstrumentation::Kill() {
    if (!pid) return;
    int status;
    kill(pid, SIGKILL);
    waitpid(pid, &status, 0);
    CleanupChild();
}

RunResult SanCovInstrumentation::GetStatus(uint32_t timeout, int expected_status) {
  struct pollfd fds = {.fd = ctrl_in, .events = POLLIN, .revents = 0};
  int res = poll(&fds, 1, timeout);
  if (res == 0) return HANG;
  else if (res != 1) return OTHER_ERROR;
  
  int status = 0;
  ssize_t rv = read(ctrl_in, &status, 1);
  if(rv < 0) return OTHER_ERROR;
  else if(rv != 1) return CRASH;
  
  if(status != expected_status) {
    return OTHER_ERROR;
  }
  
  if(status == 'd') {  
    res = poll(&fds, 1, timeout);
    if (res != 1) return OTHER_ERROR;
  
    uint64_t return_value;
    ssize_t rv = read(ctrl_in, &return_value, sizeof(return_value));
    if(rv != sizeof(return_value)) return OTHER_ERROR;
    
    this->return_value = return_value;
  }

  return OK;  
}

RunResult SanCovInstrumentation::Run(int argc, char **argv, uint32_t init_timeout, uint32_t timeout) {
  if (cur_iteration == num_iterations) {
    Kill();
  }

  RunResult poll_result;

  if(!pid) {
    StartTarget(argc, argv);
  } else {
    write(ctrl_out, "c", 1);
  }
  
  poll_result = GetStatus(init_timeout, 'k');

  if(poll_result != OK) {
    WARN("Target function not reached, retrying with a clean process\n");
    Kill();
    StartTarget(argc, argv);
    poll_result = GetStatus(init_timeout, 'k');
    if(poll_result != OK) {
      FATAL("Repetedly failing to reach target function");
    }
  }
  
  write(ctrl_out, "c", 1);
  
  poll_result = GetStatus(timeout, 'd');
  
  if(poll_result == OK) {
    cur_iteration++;
    return OK;
  } else if(poll_result == CRASH) {
    // try getting the exit status
    // (potentially multiple times)
    size_t retries = (timeout * 10);
    int success = 0;
    int status;
    int crashpid = pid;
    for(size_t i = 0; i < retries; i++) {
       success = waitpid(pid, &status, WNOHANG) == pid;
       if(success) break;
       usleep(100);
    }
    if(!success) {
      crash_description = std::string("unexpected_error_") + GetTimeStr();
      Kill();
      return CRASH;
    }
    CleanupChild();
    if(WIFSIGNALED(status)) {
      int signal = WTERMSIG(status);
      crash_description = std::string("signal_") + std::to_string(signal) + std::string("_") + GetTimeStr();
      return CRASH;
    } else if (WIFEXITED(status) && (WEXITSTATUS(status) == ASAN_EXIT_STATUS)) {
      crash_description = GetAsanCrashDesc(crashpid);
      return CRASH;
    }
    crash_description = std::string("unexpected_exit_") + GetTimeStr();
    return CRASH;
  } else if (poll_result == HANG) {
    Kill();
    return HANG;
  } else {
    crash_description = std::string("unexpected_error_") + GetTimeStr();
    Kill();
    return CRASH;
  }
}

void SanCovInstrumentation::CleanTarget() {
  Kill();
}

std::string SanCovInstrumentation::GetCrashName() {
  return crash_description;
}

std::string SanCovInstrumentation::GetTimeStr() {
  uint64_t time;
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  time = ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
  return std::to_string(time);
}


void SanCovInstrumentation::GetCoverage(Coverage &coverage, bool clear_coverage) {
  std::set<uint64_t> new_offsets;

  uint64_t* current = (uint64_t*)cov_shm->edges;
  uint64_t* end = (uint64_t*)(cov_shm->edges + ((cov_shm->num_edges + 7) / 8));
  uint64_t* virgin = (uint64_t*)virgin_bits;
  
  while (current < end) {
    if (*current && unlikely(*current & *virgin)) {
      // New edge(s) found!
      uint64_t index = ((uintptr_t)current - (uintptr_t)cov_shm->edges) * 8;
      for (uint64_t i = index; i < index + 64; i++) {
        if (edge(cov_shm->edges, i) == 1 && edge(virgin_bits, i) == 1) {
          new_offsets.insert(i);
        }
      }
    }

    current++;
    virgin++;
  }

  if(new_offsets.empty()) return;
  
  ModuleCoverage *target_coverage = GetModuleCoverage(coverage, module_name);
  if(!target_coverage) {
    coverage.push_back({module_name, new_offsets});
  } else {
    target_coverage->offsets.insert(new_offsets.begin(), new_offsets.end());
  }

  if(clear_coverage) ClearCoverage();  
}

bool SanCovInstrumentation::HasNewCoverage() {
  Coverage coverage;
  GetCoverage(coverage, false);
  return !coverage.empty();
}

void SanCovInstrumentation::ClearCoverage() {
  size_t size = (cov_shm->num_edges + 7) / 8;
  memset(cov_shm->edges, 0, size);
}

void SanCovInstrumentation::IgnoreCoverage(Coverage &coverage) {
  ModuleCoverage *target_coverage = GetModuleCoverage(coverage, module_name);
  if(!target_coverage) return;
  
  for(auto iter = target_coverage->offsets.begin(); iter != target_coverage->offsets.end(); iter++) {
    clear_edge(virgin_bits, *iter);
  }
}

std::string SanCovInstrumentation::GetAsanCrashDesc(int crashpid) {
  // very basic parsing of ASAN crash report
  std::string filename = asan_report_file + "." + std::to_string(crashpid);
  FILE *fp = fopen(filename.c_str(), "rb");
  if(!fp) {
    WARN("Error opening ASAN report at %s", filename.c_str());
    return std::string("ASAN_") + GetTimeStr();
  }
  fseek(fp, 0, SEEK_END);
  size_t size = ftell(fp);
  fseek(fp, 0, SEEK_SET);
  char *buf = (char *)malloc(size + 1);
  fread(buf, 1, size, fp);
  buf[size] = 0;
  fclose(fp);
  
  unlink(filename.c_str());
  
  char *pc = strstr(buf, "pc 0x");
  if(!pc) {
    free(buf);
    return std::string("ASAN_") + GetTimeStr();
  }

  char *hex = pc + 3;
  char *hexend = hex + 2;
  while(isalnum(*hexend)) hexend++;
  *hexend = 0;

  unsigned long addres = strtoul(hex, NULL, 16);

  free(buf);  
  return std::string("ASAN_") + AnonymizeAddress((void *)addres);
}



