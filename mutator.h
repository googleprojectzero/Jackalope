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

#include <vector>
#include "prng.h"
#include "sample.h"
#include "runresult.h"
#include "mutex.h"
#include "range.h"

#include <vector>
#include <set>

#define DETERMINISTIC_MUTATE_BYTES_NEXT 20
#define DETERMINISTIC_MUTATE_BYTES_PREVIOUS 3

class MutatorSampleContext {
public:
  virtual ~MutatorSampleContext() {
    for(MutatorSampleContext * child_context : child_contexts) {
      delete child_context;
    }
  }
  std::vector<MutatorSampleContext *> child_contexts;
};

class Mutator {
public:
  virtual ~Mutator() { }
  virtual MutatorSampleContext *CreateSampleContext(Sample *sample) { return NULL; }
  virtual void InitRound(Sample *input_sample, MutatorSampleContext *context) { }
  virtual void AddHotOffset(MutatorSampleContext *context, size_t hot_offset) { }
  virtual void SaveContext(MutatorSampleContext *context, FILE *fp) { };
  virtual void LoadContext(MutatorSampleContext *context, FILE *fp) { };
  virtual bool Mutate(Sample *inout_sample, PRNG *prng, std::vector<Sample *> &all_samples) = 0;
  virtual void NotifyResult(RunResult result, bool has_new_coverage) { }
  virtual bool CanGenerateSample() { return false;  }
  virtual bool GenerateSample(Sample* sample, PRNG* prng) { return false; }
  virtual void AddMutator(Mutator *mutator) { child_mutators.push_back(mutator); }
  virtual void SetRanges(std::vector<Range>* ranges) { }

protected:
  // a helper function to get a random chunk of sample (with size samplesize)
  // chunk size is between minblocksize and maxblocksize
  // blockstart and blocksize are return values
  int GetRandBlock(size_t samplesize, size_t minblocksize, size_t maxblocksize, size_t *blockstart, size_t *blocksize, PRNG *prng);
  
  void AddInterestingValue(char* data, size_t size, std::vector<Sample> &interesting_values);
  template<typename T> void AddDefaultInterestingValues(std::vector<Sample>& interesting_values);

  template<typename T> T FlipEndian(T value) {
    T out = 0;
    for(int i = 0; i<sizeof(T); i++) {
      out <<= 8;
      out |= value & 0xFF;
      value >>= 8;
    }
    return out;
  }
  
  std::vector<Mutator *> child_mutators;
};

class HierarchicalMutator : public Mutator {
public:
  virtual void InitRound(Sample *input_sample, MutatorSampleContext *context) override {
    for (size_t i = 0; i < child_mutators.size(); i++) {
      child_mutators[i]->InitRound(input_sample, context->child_contexts[i]);
    }
  }
  
  void CreateChildContexts(Sample *sample, MutatorSampleContext *context) {
    context->child_contexts.resize(child_mutators.size());
    for (size_t i = 0; i < child_mutators.size(); i++) {
      context->child_contexts[i] = child_mutators[i]->CreateSampleContext(sample);
    }
  }
  
  virtual MutatorSampleContext *CreateSampleContext(Sample *sample) override {
    MutatorSampleContext *context = new MutatorSampleContext;
    CreateChildContexts(sample, context);
    return context;
  }
  
  virtual void NotifyResult(RunResult result, bool has_new_coverage) override {
    for (size_t i = 0; i < child_mutators.size(); i++) {
      child_mutators[i]->NotifyResult(result, has_new_coverage);
    }
  }
  
  virtual void AddHotOffset(MutatorSampleContext *context, size_t hot_offset) override {
    for (size_t i = 0; i < child_mutators.size(); i++) {
      child_mutators[i]->AddHotOffset(context->child_contexts[i], hot_offset);
    }
  }
  
  virtual void SetRanges(std::vector<Range>* ranges) override {
    for (size_t i = 0; i < child_mutators.size(); i++) {
      child_mutators[i]->SetRanges(ranges);
    }
  }

  virtual void SaveContext(MutatorSampleContext *context, FILE *fp) override {
    for (size_t i = 0; i < child_mutators.size(); i++) {
      child_mutators[i]->SaveContext(context->child_contexts[i], fp);
    }
  }

  virtual void LoadContext(MutatorSampleContext *context, FILE *fp) override {
    for (size_t i = 0; i < child_mutators.size(); i++) {
      child_mutators[i]->LoadContext(context->child_contexts[i], fp);
    }
  }
  
  virtual bool CanGenerateSample() override {
    for (size_t i = 0; i < child_mutators.size(); i++) {
      if (child_mutators[i]->CanGenerateSample()) return true;
    }
    return false;
  }

  virtual bool GenerateSample(Sample* sample, PRNG* prng) override {
    for (size_t i = 0; i < child_mutators.size(); i++) {
      if (child_mutators[i]->CanGenerateSample()) {
        return child_mutators[i]->GenerateSample(sample, prng);
      }
    }
    return false;
  }
};

// Mutator that runs another mutator for N rounds
class NRoundMutator : public HierarchicalMutator {
public:
  NRoundMutator(Mutator *child_mutator, size_t num_rounds) {
    AddMutator(child_mutator);
    this->num_rounds = num_rounds;
    current_round = 0;
  }
  
  virtual void InitRound(Sample *input_sample, MutatorSampleContext *context) override {
    HierarchicalMutator::InitRound(input_sample, context);
    current_round = 0;
  }

  virtual bool Mutate(Sample *inout_sample, PRNG *prng, std::vector<Sample *> &all_samples) override {
    if (current_round == num_rounds) return false;
    child_mutators[0]->Mutate(inout_sample, prng, all_samples);
    current_round++;
    return true;
  }

protected:
  size_t current_round;
  size_t num_rounds;
};

class MutatorSequenceContext : public MutatorSampleContext {
public:
  uint64_t current_mutator_index;
};

// runs input mutators in sequence
class MutatorSequence : public HierarchicalMutator {
public:
  MutatorSequence(bool restart_each_round = true, bool restart_on_hot_offset = false) {
    this->restart_each_round = restart_each_round;
    this->restart_on_hot_offset = restart_on_hot_offset;
  }

  virtual void InitRound(Sample *input_sample, MutatorSampleContext *context) override {
    HierarchicalMutator::InitRound(input_sample, context);
    this->context = (MutatorSequenceContext *)context;
    if(restart_each_round) {
      this->context->current_mutator_index = 0;
    }
  }

  virtual MutatorSampleContext *CreateSampleContext(Sample *sample) override {
    MutatorSequenceContext *context = new MutatorSequenceContext;
    CreateChildContexts(sample, context);
    context->current_mutator_index = 0;
    return context;
  }
  
  virtual void AddHotOffset(MutatorSampleContext *context, size_t hot_offset) override {
    HierarchicalMutator::AddHotOffset(context, hot_offset);
    if(restart_on_hot_offset) {
      ((MutatorSequenceContext *)context)->current_mutator_index = 0;
    }
  }
  
  virtual void SaveContext(MutatorSampleContext *context, FILE *fp) override {
    uint64_t current_mutator_index = ((MutatorSequenceContext *)context)->current_mutator_index;
    fwrite(&current_mutator_index, sizeof(current_mutator_index), 1, fp);

    HierarchicalMutator::SaveContext(context, fp);
  }

  virtual void LoadContext(MutatorSampleContext *context, FILE *fp) override {
    uint64_t current_mutator_index;
    fread(&current_mutator_index, sizeof(current_mutator_index), 1, fp);
    ((MutatorSequenceContext *)context)->current_mutator_index = current_mutator_index;
    
    HierarchicalMutator::LoadContext(context, fp);
  }

  virtual bool Mutate(Sample *inout_sample, PRNG *prng, std::vector<Sample *> &all_samples) override {
    while (context->current_mutator_index < child_mutators.size()) {
      Mutator *current_mutator = child_mutators[context->current_mutator_index];
      bool ret = current_mutator->Mutate(inout_sample, prng, all_samples);
      if (ret) {
        return true;
      }
      context->current_mutator_index++;
    }
    return false;
  }

  virtual void NotifyResult(RunResult result, bool has_new_coverage) override {
    child_mutators[context->current_mutator_index]->NotifyResult(result, has_new_coverage);
  }

protected:
  MutatorSequenceContext *context;
  bool restart_each_round;
  bool restart_on_hot_offset;
};

// mutates using random child mutator
class SelectMutator : public HierarchicalMutator {
  virtual bool Mutate(Sample *inout_sample, PRNG *prng, std::vector<Sample *> &all_samples) override {
    int mutator_index = prng->Rand() % child_mutators.size();
    Mutator *current_mutator = child_mutators[mutator_index];
    last_mutator_index = mutator_index;
    return current_mutator->Mutate(inout_sample, prng, all_samples);
  }

  virtual void NotifyResult(RunResult result, bool has_new_coverage) override {
    child_mutators[last_mutator_index]->NotifyResult(result, has_new_coverage);
  }

  virtual bool GenerateSample(Sample* sample, PRNG* prng) override {
    int mutator_index = prng->Rand() % child_mutators.size();
    for (size_t i = 0; i < child_mutators.size(); i++) {
      if (child_mutators[(i + mutator_index) % child_mutators.size()]->CanGenerateSample()) {
        return child_mutators[(i + mutator_index) % child_mutators.size()]->GenerateSample(sample, prng);
      }
    }
    return false;
  }

protected:
  int last_mutator_index;
};

// like SelectMutator but each child mutator
// has an associated probability
class PSelectMutator : public HierarchicalMutator {
public:
  PSelectMutator() {
    psum = 0;
  }

  virtual void AddMutator(Mutator *mutator) override {
    child_mutators.push_back(mutator);
    probabilities.push_back(1);
    psum += 1;
  }

  void AddMutator(Mutator *mutator, double p) {
    child_mutators.push_back(mutator);
    probabilities.push_back(p);
    psum += p;
  }

  virtual bool Mutate(Sample *inout_sample, PRNG *prng, std::vector<Sample *> &all_samples) override {
    double p = prng->RandReal() * psum;
    double sum = 0;
    for (int i = 0; i < child_mutators.size(); i++) {
      sum += probabilities[i];
      if ((p < sum) || (i == (child_mutators.size() - 1))) {
        last_mutator_index = i;
        Mutator *current_mutator = child_mutators[i];
        return current_mutator->Mutate(inout_sample, prng, all_samples);
      }
    }
    // unreachable
    return false;
  }

  virtual void NotifyResult(RunResult result, bool has_new_coverage) override {
    child_mutators[last_mutator_index]->NotifyResult(result, has_new_coverage);
  }

  virtual bool GenerateSample(Sample* sample, PRNG* prng) override {
    double psum = 0;
    size_t last_generator = 0;
    for (size_t i = 0; i < child_mutators.size(); i++) {
      if (child_mutators[i]->CanGenerateSample()) {
        psum += probabilities[i];
        last_generator = i;
      }
    }
    double p = prng->RandReal() * psum;
    double sum = 0;
    for (int i = 0; i < child_mutators.size(); i++) {
      if (!child_mutators[i]->CanGenerateSample()) continue;
      sum += probabilities[i];
      if ((p < sum) || (i == last_generator)) {
        last_mutator_index = i;
        Mutator* current_mutator = child_mutators[i];
        return current_mutator->GenerateSample(sample, prng);
      }
    }
    return false;
  }

protected:
  double psum;
  int last_mutator_index;
  std::vector<double> probabilities;
};

// mutator that runs child mutators repeatedly
// (in the same Mutate() call)
class RepeatMutator : public HierarchicalMutator {
public:
  RepeatMutator(Mutator *mutator, double repeat_p) {
    AddMutator(mutator);
    this->repeat_p = repeat_p;
  }

  virtual bool Mutate(Sample *inout_sample, PRNG *prng, std::vector<Sample *> &all_samples) override {
    // run the mutator at least once
    bool ret = child_mutators[0]->Mutate(inout_sample, prng, all_samples);
    if (!ret) return false;
    while (prng->RandReal() < repeat_p) {
      child_mutators[0]->Mutate(inout_sample, prng, all_samples);
    }
    return true;
  }

public:
  double repeat_p;
};

class ByteFlipMutator : public Mutator {
public:
  bool Mutate(Sample *inout_sample, PRNG *prng, std::vector<Sample *> &all_samples) override;
};

class ArithmeticMutator : public Mutator {
public:
  bool Mutate(Sample *inout_sample, PRNG *prng, std::vector<Sample *> &all_samples) override;
private:
  template<typename T>
  bool MutateArithmeticValue(Sample *inout_sample, PRNG *prng, int flip_endian);
};

class BlockFlipMutator : public Mutator {
public:
  BlockFlipMutator(int min_block_size, int max_block_size, bool uniform = false):
    min_block_size(min_block_size), max_block_size(max_block_size), uniform(uniform)
    { }

  bool Mutate(Sample *inout_sample, PRNG *prng, std::vector<Sample *> &all_samples) override;

protected:

  bool uniform;
  int min_block_size;
  int max_block_size;
};

class AppendMutator : public Mutator {
public:
  AppendMutator(int min_append, int max_append) :
    min_append(min_append), max_append(max_append)
    { }

  bool Mutate(Sample *inout_sample, PRNG *prng, std::vector<Sample *> &all_samples) override;

protected:
  int min_append;
  int max_append;
};

class BlockInsertMutator : public Mutator {
public:
  BlockInsertMutator(int min_insert, int max_insert) :
    min_insert(min_insert), max_insert(max_insert)
    { }

  bool Mutate(Sample *inout_sample, PRNG *prng, std::vector<Sample *> &all_samples) override;

protected:
  int min_insert;
  int max_insert;
};

class BlockDuplicateMutator : public Mutator {
public:
  BlockDuplicateMutator(int min_block_size, int max_block_size,
                        int min_duplicate_cnt, int max_duplicate_cnt) :
    min_block_size(min_block_size), max_block_size(max_block_size),
    min_duplicate_cnt(min_duplicate_cnt), max_duplicate_cnt(max_duplicate_cnt)
  { }

  bool Mutate(Sample *inout_sample, PRNG *prng, std::vector<Sample *> &all_samples) override;

protected:
  int min_block_size;
  int max_block_size;
  int min_duplicate_cnt;
  int max_duplicate_cnt;
};

class InterestingValueMutator : public Mutator {
public:
  InterestingValueMutator(bool use_default_values = false);

  bool Mutate(Sample *inout_sample, PRNG *prng, std::vector<Sample *> &all_samples) override;

protected:
  std::vector<Sample> interesting_values;
};

class SpliceMutator : public Mutator {
public:
  SpliceMutator(int points, double displacement_p) : points(points), displacement_p(displacement_p) { }

  bool Mutate(Sample *inout_sample, PRNG *prng, std::vector<Sample *> &all_samples) override;

protected:
  int points;
  double displacement_p;
};

// does x iterations of deterministic and y
// iterations of nondeterministic mutations
// when deterministic mutator is done
// does only nondeterministic mutation
class DtermininsticNondeterministicMutator : public HierarchicalMutator {
public:
  DtermininsticNondeterministicMutator(Mutator *deterministic_mutator, size_t num_rounds_deterministic,
                Mutator *nondeterministic_mutator, size_t num_rounds_nondeterministic)
  {
    AddMutator(deterministic_mutator);
    AddMutator(nondeterministic_mutator);
    this->deterministic_mutator = deterministic_mutator;
    this->nondeterministic_mutator = nondeterministic_mutator;
    this->num_rounds_deterministic = num_rounds_deterministic;
    this->num_rounds_nondeterministic = num_rounds_nondeterministic;
    current_round = 0;
  }

  virtual void InitRound(Sample *input_sample, MutatorSampleContext *context) override {
    HierarchicalMutator::InitRound(input_sample, context);
    current_round = 0;
  }

  virtual bool Mutate(Sample *inout_sample, PRNG *prng, std::vector<Sample *> &all_samples) override {
    bool ret;
    if(current_round < num_rounds_deterministic) {
      ret = deterministic_mutator->Mutate(inout_sample, prng, all_samples);
      if(ret) {
        last_mutator = deterministic_mutator;
        current_round++;
        return ret;
      }
    }
    if(current_round < (num_rounds_deterministic + num_rounds_nondeterministic)) {
      nondeterministic_mutator->Mutate(inout_sample, prng, all_samples);
      last_mutator = nondeterministic_mutator;
      current_round++;
      return true;
    }
    return false;
  }

  virtual void NotifyResult(RunResult result, bool has_new_coverage) override {
    last_mutator->NotifyResult(result, has_new_coverage);
  }

protected:
  size_t current_round;
  size_t num_rounds_deterministic;
  size_t num_rounds_nondeterministic;
  Mutator *deterministic_mutator;
  Mutator *nondeterministic_mutator;
  Mutator *last_mutator;
};

class BaseDeterministicContext : public MutatorSampleContext {
public:
  BaseDeterministicContext() {
    cur_region = 0;
  }
  
  struct MutateRegion {
    uint64_t start;
    uint64_t end;
    uint64_t cur;
    uint64_t cur_progress;
  };
  
  std::vector<MutateRegion> regions;
  uint64_t cur_region;
  
  void AddHotOffset(size_t offset);
  
  bool GetNextByteToMutate(size_t *pos, size_t *progress, size_t max_progress);
  
  Mutex mutex;
};

class BaseDeterministicMutator : public Mutator {
public:
  virtual MutatorSampleContext *CreateSampleContext(Sample *sample) override;
  
  virtual void InitRound(Sample *input_sample, MutatorSampleContext *context) override {
    this->context = (BaseDeterministicContext *)context;
  }
  
  virtual void AddHotOffset(MutatorSampleContext *context, size_t hot_offset) override {
    ((BaseDeterministicContext *)context)->AddHotOffset(hot_offset);
  }
  
  virtual void SaveContext(MutatorSampleContext *context, FILE *fp) override {
    BaseDeterministicContext *current_context = (BaseDeterministicContext *)context;
    current_context->mutex.Lock();
    uint64_t num_regions = current_context->regions.size();
    fwrite(&num_regions, sizeof(num_regions), 1, fp);
    fwrite(&current_context->cur_region, sizeof(current_context->cur_region), 1, fp);
    fwrite(&current_context->regions[0], sizeof(current_context->regions[0]), num_regions, fp);
    current_context->mutex.Unlock();
  }

  virtual void LoadContext(MutatorSampleContext *context, FILE *fp) override {
    BaseDeterministicContext *current_context = (BaseDeterministicContext *)context;
    current_context->mutex.Lock();
    uint64_t num_regions;
    fread(&num_regions, sizeof(num_regions), 1, fp);
    fread(&current_context->cur_region, sizeof(current_context->cur_region), 1, fp);
    current_context->regions.resize(num_regions);
    fread(&current_context->regions[0], sizeof(current_context->regions[0]), num_regions, fp);
    current_context->mutex.Unlock();
  }
  
  BaseDeterministicContext *context;
};

class DeterministicByteFlipMutator : public BaseDeterministicMutator {
public:
  bool Mutate(Sample *inout_sample, PRNG *prng, std::vector<Sample *> &all_samples) override;
};

class DeterministicInterestingValueMutator : public BaseDeterministicMutator {
public:
  DeterministicInterestingValueMutator(bool use_default_values = false);
  
  bool Mutate(Sample *inout_sample, PRNG *prng, std::vector<Sample *> &all_samples) override;

protected:
  std::vector<Sample> interesting_values;
};

// Mutator that mutates only set ranges using a child mutator
class RangeMutator : public HierarchicalMutator {
public:
  RangeMutator(Mutator* child_mutator) {
    AddMutator(child_mutator);
  }

  virtual void SetRanges(std::vector<Range>* ranges) override {
    HierarchicalMutator::SetRanges(ranges);
    this->ranges = ranges;
  }

  virtual bool Mutate(Sample* inout_sample, PRNG* prng, std::vector<Sample*>& all_samples) override;

protected:

  std::vector<Range> *ranges;
};
