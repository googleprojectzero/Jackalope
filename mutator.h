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

class MutatorSampleContext { };

class SampleContextVector : public MutatorSampleContext {
public:
  std::vector<MutatorSampleContext *> contexts;
};

class Mutator {
public:
  virtual ~Mutator() { }
  virtual MutatorSampleContext *CreateSampleContext(Sample *sample) { return NULL; }
  virtual void InitRound(Sample *input_sample, MutatorSampleContext *context) { }
  virtual bool Mutate(Sample *inout_sample, PRNG *prng, std::vector<Sample *> &all_samples) = 0;
  virtual void NotifyResult(RunResult result, bool has_new_coverage) { }
  virtual bool CanGenerateSample() { return false;  }
  virtual bool GenerateSample(Sample* sample, PRNG* prng) { return false; }
protected:
  // a helper function to get a random chunk of sample (with size samplesize)
  // chunk size is between minblocksize and maxblocksize
  // blockstart and blocksize are return values
  int GetRandBlock(size_t samplesize, size_t minblocksize, size_t maxblocksize, size_t *blockstart, size_t *blocksize, PRNG *prng);
};

// Mutator that runs another mutator for N rounds
class NRoundMutator : public Mutator {
public:
  NRoundMutator(Mutator *child_mutator, size_t num_rounds) {
    this->child_mutator = child_mutator;
    this->num_rounds = num_rounds;
    current_round = 0;
  }

  virtual MutatorSampleContext *CreateSampleContext(Sample *sample) override {
    return child_mutator->CreateSampleContext(sample);
  }

  virtual void InitRound(Sample *input_sample, MutatorSampleContext *context) override {
    child_mutator->InitRound(input_sample, context);
    current_round = 0;
  }

  virtual bool Mutate(Sample *inout_sample, PRNG *prng, std::vector<Sample *> &all_samples) override {
    if (current_round == num_rounds) return false;
    child_mutator->Mutate(inout_sample, prng, all_samples);
    current_round++;
    return true;
  }

  virtual void NotifyResult(RunResult result, bool has_new_coverage) override {
    child_mutator->NotifyResult(result, has_new_coverage);
  }

  virtual bool CanGenerateSample() { return child_mutator->CanGenerateSample(); }

  virtual bool GenerateSample(Sample* sample, PRNG* prng) { return child_mutator->GenerateSample(sample, prng); }

protected:
  size_t current_round;
  size_t num_rounds;
  Mutator * child_mutator;
};

// runs input mutators in sequence
class MutatorSequence : public Mutator {
public:
  MutatorSequence() {
    current_mutator_index = 0;
  }

  void AddMutator(Mutator *mutator) {
    child_mutators.push_back(mutator);
  }

  virtual void InitRound(Sample *input_sample, MutatorSampleContext *context) override {
    for (size_t i = 0; i < child_mutators.size(); i++) {
      child_mutators[i]->InitRound(input_sample, ((SampleContextVector *)context)->contexts[i]);
    }
    current_mutator_index = 0;
  }

  virtual MutatorSampleContext *CreateSampleContext(Sample *sample) override {
    SampleContextVector *context = new SampleContextVector;
    context->contexts.resize(child_mutators.size());
    for (size_t i = 0; i < child_mutators.size(); i++) {
      context->contexts[i] = child_mutators[i]->CreateSampleContext(sample);
    }
  }

  virtual bool Mutate(Sample *inout_sample, PRNG *prng, std::vector<Sample *> &all_samples) override {
    while (current_mutator_index < child_mutators.size()) {
      Mutator *current_mutator = child_mutators[current_mutator_index];
      bool ret = current_mutator->Mutate(inout_sample, prng, all_samples);
      if (ret) {
        return true;
      }
      current_mutator_index++;
    }
    return false;
  }

  virtual void NotifyResult(RunResult result, bool has_new_coverage) override {
    child_mutators[current_mutator_index]->NotifyResult(result, has_new_coverage);
  }

  virtual bool CanGenerateSample() {
    for (size_t i = 0; i < child_mutators.size(); i++) {
      if (child_mutators[i]->CanGenerateSample()) return true;
    }
    return false;
  }

  virtual bool GenerateSample(Sample* sample, PRNG* prng) {
    for (size_t i = 0; i < child_mutators.size(); i++) {
      if (child_mutators[i]->CanGenerateSample()) {
        return child_mutators[i]->GenerateSample(sample, prng);
      }
    }  
  }

protected:
  int current_mutator_index;
  std::vector<Mutator *> child_mutators;
};

// mutates using random child mutator
class SelectMutator : public Mutator {
  void AddMutator(Mutator *mutator) {
    child_mutators.push_back(mutator);
  }

  virtual void InitRound(Sample *input_sample, MutatorSampleContext *context) override {
    for (size_t i = 0; i < child_mutators.size(); i++) {
      child_mutators[i]->InitRound(input_sample, ((SampleContextVector *)context)->contexts[i]);
    }
  }

  virtual MutatorSampleContext *CreateSampleContext(Sample *sample) override {
    SampleContextVector *context = new SampleContextVector;
    context->contexts.resize(child_mutators.size());
    for (size_t i = 0; i < child_mutators.size(); i++) {
      context->contexts[i] = child_mutators[i]->CreateSampleContext(sample);
    }
  }

  virtual bool Mutate(Sample *inout_sample, PRNG *prng, std::vector<Sample *> &all_samples) override {
    int mutator_index = prng->Rand() % child_mutators.size();
    Mutator *current_mutator = child_mutators[mutator_index];
    last_mutator_index = mutator_index;
    return current_mutator->Mutate(inout_sample, prng, all_samples);
  }

  virtual void NotifyResult(RunResult result, bool has_new_coverage) override {
    child_mutators[last_mutator_index]->NotifyResult(result, has_new_coverage);
  }

  virtual bool CanGenerateSample() {
    for (size_t i = 0; i < child_mutators.size(); i++) {
      if (child_mutators[i]->CanGenerateSample()) return true;
    }
    return false;
  }

  virtual bool GenerateSample(Sample* sample, PRNG* prng) {
    int mutator_index = prng->Rand() % child_mutators.size();
    for (size_t i = 0; i < child_mutators.size(); i++) {
      if (child_mutators[(i + mutator_index) % child_mutators.size()]->CanGenerateSample()) {
        return child_mutators[(i + mutator_index) % child_mutators.size()]->GenerateSample(sample, prng);
      }
    }
  }

protected:
  int last_mutator_index;
  std::vector<Mutator *> child_mutators;
};

// like SelectMutator but each child mutator
// has an associated probability
class PSelectMutator : public Mutator {
public:
  struct ChildMutator {
    Mutator *mutator;
    double p;
  };

  PSelectMutator() {
    psum = 0;
  }

  void AddMutator(Mutator *mutator, double p) {
    child_mutators.push_back({mutator, p});
    psum += p;
  }

  virtual void InitRound(Sample *input_sample, MutatorSampleContext *context) override {
    for (size_t i = 0; i < child_mutators.size(); i++) {
      child_mutators[i].mutator->InitRound(input_sample, ((SampleContextVector *)context)->contexts[i]);
    }
  }

  virtual MutatorSampleContext *CreateSampleContext(Sample *sample) override {
    SampleContextVector *context = new SampleContextVector;
    context->contexts.resize(child_mutators.size());
    for (size_t i = 0; i < child_mutators.size(); i++) {
      context->contexts[i] = child_mutators[i].mutator->CreateSampleContext(sample);
    }
    return context;
  }

  virtual bool Mutate(Sample *inout_sample, PRNG *prng, std::vector<Sample *> &all_samples) override {
    double p = prng->RandReal() * psum;
    double sum = 0;
    for (int i = 0; i < child_mutators.size(); i++) {
      sum += child_mutators[i].p;
      if ((p < sum) || (i == (child_mutators.size() - 1))) {
        last_mutator_index = i;
        Mutator *current_mutator = child_mutators[i].mutator;
        return current_mutator->Mutate(inout_sample, prng, all_samples);
      }
    }
    // unreachable
    return false;
  }

  virtual void NotifyResult(RunResult result, bool has_new_coverage) override {
    child_mutators[last_mutator_index].mutator->NotifyResult(result, has_new_coverage);
  }

  virtual bool CanGenerateSample() {
    for (size_t i = 0; i < child_mutators.size(); i++) {
      if (child_mutators[i].mutator->CanGenerateSample()) return true;
    }
    return false;
  }

  virtual bool GenerateSample(Sample* sample, PRNG* prng) {
    double psum = 0;
    size_t last_generator = 0;
    for (size_t i = 0; i < child_mutators.size(); i++) {
      if (child_mutators[i].mutator->CanGenerateSample()) {
        psum += child_mutators[i].p;
        last_generator = i;
      }
    }
    double p = prng->RandReal() * psum;
    double sum = 0;
    for (int i = 0; i < child_mutators.size(); i++) {
      if (!child_mutators[i].mutator->CanGenerateSample()) continue;
      sum += child_mutators[i].p;
      if ((p < sum) || (i == last_generator)) {
        last_mutator_index = i;
        Mutator* current_mutator = child_mutators[i].mutator;
        return current_mutator->GenerateSample(sample, prng);
      }
    }
    return false;
  }

protected:
  double psum;
  int last_mutator_index;
  std::vector<ChildMutator> child_mutators;
};

// mutator that runs child mutators repeatedly
// (in the same Mutate() call)
class RepeatMutator : public Mutator {
public:
  RepeatMutator(Mutator *mutator, double repeat_p) {
    this->child_mutator = mutator;
    this->repeat_p = repeat_p;
  }

  virtual void InitRound(Sample *input_sample, MutatorSampleContext *context) override {
    child_mutator->InitRound(input_sample, context);
  }

  virtual MutatorSampleContext *CreateSampleContext(Sample *sample) override {
    return child_mutator->CreateSampleContext(sample);
  }

  virtual bool Mutate(Sample *inout_sample, PRNG *prng, std::vector<Sample *> &all_samples) override {
    // run the mutator at least once
    bool ret = child_mutator->Mutate(inout_sample, prng, all_samples);
    if (!ret) return false;
    while (prng->RandReal() < repeat_p) {
      child_mutator->Mutate(inout_sample, prng, all_samples);
    }
    return true;
  }

  virtual void NotifyResult(RunResult result, bool has_new_coverage) override {
    child_mutator->NotifyResult(result, has_new_coverage);
  }

  virtual bool CanGenerateSample() {
    return child_mutator->CanGenerateSample();
  }

  virtual bool GenerateSample(Sample* sample, PRNG* prng) {
    return child_mutator->GenerateSample(sample, prng);
  }

public:
  Mutator *child_mutator;
  double repeat_p;
};

class ByteFlipMutator : public Mutator {
public:
  bool Mutate(Sample *inout_sample, PRNG *prng, std::vector<Sample *> &all_samples) override;
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

class InterstingValueMutator : public Mutator {
public:
  InterstingValueMutator(bool use_default_values = false);

  void AddInterestingValue(char *data, size_t size);

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
