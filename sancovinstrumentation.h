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

#include <inttypes.h>
#include <list>
#include <string>
#include "coverage.h"
#include "runresult.h"
#include "instrumentation.h"

class SanCovInstrumentation : public Instrumentation {
public:
  SanCovInstrumentation(int thread_id);
  ~SanCovInstrumentation();

  void Init(int argc, char **argv) override;

  RunResult Run(int argc, char** argv, uint32_t init_timeout, uint32_t timeout) override;

  void CleanTarget() override;

  bool HasNewCoverage() override;
  void GetCoverage(Coverage &coverage, bool clear_coverage) override;
  void ClearCoverage() override;
  void IgnoreCoverage(Coverage &coverage) override;

  uint64_t GetReturnValue() override { return return_value; }

  std::string GetCrashName() override;

protected:
  struct coverage_shmem_data {
    uint32_t num_edges;
    uint8_t edges[];
  };

  inline int edge(const uint8_t* bits, uint64_t index) {
    return (bits[index / 8] >> (index % 8)) & 0x1;
  }

  inline void clear_edge(uint8_t* bits, uint64_t index) {
    bits[index / 8] &= ~(1u << (index % 8));
  }

  void SetUpShmem();
  void ComputeEnvp(std::list<std::string> &additional_env);

  void StartTarget(int argc, char** argv);
  void Kill();
  void CleanupChild();
  
  RunResult GetStatus(uint32_t timeout, int expected_status);
  
  uint64_t return_value;
  std::string crash_description; 
  
  int pid;
  int thread_id;
  
  std::string sample_shm_name;
  std::string coverage_shm_name;
  
  char **envp;
  
  int ctrl_in;
  int ctrl_out;
  
  int cov_shm_fd;
  coverage_shmem_data* cov_shm;
  
  uint8_t* virgin_bits;
  
  std::string module_name;
  
  int num_iterations;
  int cur_iteration;
  
  bool mute_child;
};

