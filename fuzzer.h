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
#include <list>
#include <vector>
#include <queue>
#include <unordered_map>
#include "prng.h"
#include "mutex.h"
#include "coverage.h"
#include "instrumentation.h"
#include "minimizer.h"

#ifdef linux
#include "sancovinstrumentation.h"
#else
#include "tinyinstinstrumentation.h"
#endif

class PRNG;
class Mutator;
class Instrumentation;
class SampleDelivery;
class MutatorSampleContext;
class Sample;
class CoverageClient;

#define DEFAULT_CRASH_REPRODUCE_RETRIES 10
#define DEFAULT_COVERAGE_REPRODUCE_RETRIES 3

#define DELIVERY_RETRY_TIMES 100

#define MAX_IDENTICAL_CRASHES 4

// save state every 5 minutes
#define FUZZER_SAVE_INERVAL (5 * 60)

#define MIN_SAMPLES_TO_GENERATE 10

class Fuzzer {
public:
  void Run(int argc, char **argv);

  class ThreadContext {
  public:
    int thread_id;
    Fuzzer *fuzzer;
    SampleDelivery *sampleDelivery;
    PRNG *prng;
    Mutator *mutator;
    Instrumentation * instrumentation;
    Minimizer* minimizer;

    //std::string target_cmd;
    int target_argc;
    char **target_argv;
    
    // a thread-local copy of all samples vector
    std::vector<Sample *> all_samples_local;
    
    bool coverage_initialized;

    ~ThreadContext();
  };

  void RunFuzzerThread(ThreadContext *tc);

protected:

  enum FuzzerState {
    RESTORE_NEEDED,
    INPUT_SAMPLE_PROCESSING,
    SERVER_SAMPLE_PROCESSING,
    GENERATING_SAMPLES,
    FUZZING,
  };

  enum JobType {
    PROCESS_SAMPLE,
    FUZZ,
    WAIT,
  };

  class SampleQueueEntry {
  public:
    SampleQueueEntry() : sample(NULL), context(NULL),
      priority(0), sample_index(0), num_runs(0),
      num_crashes(0), num_hangs(0), num_newcoverage(0),
      discarded(0) {}

    void Save(FILE *fp);
    void Load(FILE *fp);
    
    Sample *sample;
    std::string sample_filename;
    MutatorSampleContext *context;
 
    double priority;
    uint64_t sample_index;
    uint64_t num_runs;
    uint64_t num_crashes;
    uint64_t num_hangs;
    uint64_t num_newcoverage;
    int32_t discarded;
  };
  
  struct CmpEntryPtrs
  {
    bool operator()(const SampleQueueEntry* lhs, const SampleQueueEntry* rhs) const {
      if(lhs->priority == rhs->priority) {
        // prefer newer samples
        return lhs->sample_index < rhs->sample_index;
      }
      return lhs->priority < rhs->priority;
    }
  };

  std::vector<Sample *> all_samples;
  std::vector<SampleQueueEntry *> all_entries;
  std::priority_queue<SampleQueueEntry *, std::vector<SampleQueueEntry *>, CmpEntryPtrs> sample_queue;
  
  struct FuzzerJob {
    JobType type;
    union {
      Sample* sample;
      SampleQueueEntry* entry;
    };
    bool discard_sample;
  };

  void PrintUsage();
  void ParseOptions(int argc, char **argv);

  void SetupDirectories();

  ThreadContext *CreateThreadContext(int argc, char **argv, int thread_id);
  
  virtual Mutator *CreateMutator(int argc, char **argv, ThreadContext *tc) = 0;
  virtual PRNG *CreatePRNG(int argc, char **argv, ThreadContext *tc);
  virtual Instrumentation *CreateInstrumentation(int argc, char **argv, ThreadContext *tc);
  virtual SampleDelivery* CreateSampleDelivery(int argc, char** argv, ThreadContext* tc);
  virtual Minimizer* CreateMinimizer(int argc, char** argv, ThreadContext* tc);
  virtual bool OutputFilter(Sample *original_sample, Sample *output_sample, ThreadContext* tc);
  virtual void AdjustSamplePriority(ThreadContext *tc, SampleQueueEntry *entry, int found_new_coverage);

  // by default, all return values are interesting
  virtual bool IsReturnValueInteresting(uint64_t return_value) { return true; }
  
  virtual bool TrackHotOffsets() { return false; }

  void ReplaceTargetCmdArg(ThreadContext *tc, const char *search, const char *replace);
  
  bool MagicOutputFilter(Sample *original_sample, Sample *output_sample, const char *magic, size_t magic_size);

  RunResult RunSample(ThreadContext *tc, Sample *sample, int *has_new_coverage, bool trim, bool report_to_server, uint32_t init_timeout, uint32_t timeout);
  RunResult RunSampleAndGetCoverage(ThreadContext* tc, Sample* sample, Coverage* coverage, uint32_t init_timeout, uint32_t timeout);
  RunResult TryReproduceCrash(ThreadContext* tc, Sample* sample, uint32_t init_timeout, uint32_t timeout);
  void MinimizeSample(ThreadContext *tc, Sample *sample, Coverage* stable_coverage, uint32_t init_timeout, uint32_t timeout);

  int InterestingSample(ThreadContext *tc, Sample *sample, Coverage *stableCoverage, Coverage *variableCoverage);

  void SynchronizeAndGetJob(ThreadContext* tc, FuzzerJob* job);
  void JobDone(FuzzerJob* job);
  void FuzzJob(ThreadContext* tc, FuzzerJob* job);
  void ProcessSample(ThreadContext* tc, FuzzerJob* job);

  uint64_t num_crashes;
  uint64_t num_unique_crashes;
  uint64_t num_hangs;
  uint64_t num_samples;
  uint64_t num_samples_discarded;
  uint64_t num_threads;
  uint64_t total_execs;
  
  void SaveState(ThreadContext *tc);
  void RestoreState(ThreadContext *tc);

  std::string in_dir;
  std::string out_dir;
  std::string sample_dir;
  std::string crash_dir;
  std::string hangs_dir;

  //std::string target_cmd;
  int target_argc;
  char **target_argv;
  uint32_t timeout;
  uint32_t init_timeout;
  uint32_t corpus_timeout;

  Mutex queue_mutex;
  Mutex output_mutex;
  Mutex coverage_mutex;

  Coverage fuzzer_coverage;

  Mutex server_mutex;
  CoverageClient *server;
  uint64_t last_server_update_time_ms;
  uint64_t server_update_interval_ms;

  std::list<std::string> input_files;
  std::list<Sample> server_samples;
  FuzzerState state;
  size_t samples_pending;

  bool save_hangs;
  double acceptable_hang_ratio;
  double acceptable_crash_ratio;

  bool minimize_samples;

  int coverage_reproduce_retries;
  int crash_reproduce_retries;
  bool clean_target_on_coverage;
  
  bool should_restore_state;
  
  Mutex crash_mutex;
  std::unordered_map<std::string, int> unique_crashes;
  
  uint64_t last_save_time;
  
  SampleTrie sample_trie;
};
