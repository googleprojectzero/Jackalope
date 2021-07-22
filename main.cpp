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

#include "common.h"
#include "fuzzer.h"
#include "mutator.h"
#include "mutators/grammar/grammar.h"
#include "mutators/grammar/grammarmutator.h"
#include "mutators/grammar/grammarminimizer.h"


class BinaryFuzzer : public Fuzzer {
  Mutator *CreateMutator(int argc, char **argv, ThreadContext *tc) override;
};

Mutator * BinaryFuzzer::CreateMutator(int argc, char **argv, ThreadContext *tc) {
  // a pretty simple non-deterministic mutation strategy

  PSelectMutator *pselect = new PSelectMutator();

  // select one of the mutators below with corresponding
  // probablilities
  pselect->AddMutator(new ByteFlipMutator(), 1);
  pselect->AddMutator(new AppendMutator(1, 128), 0.2);
  pselect->AddMutator(new BlockInsertMutator(1, 128), 0.1);
  pselect->AddMutator(new BlockFlipMutator(2, 16), 0.1);
  pselect->AddMutator(new BlockFlipMutator(16, 64), 0.1);
  pselect->AddMutator(new BlockFlipMutator(1, 64, true), 0.1);
  pselect->AddMutator(new BlockDuplicateMutator(1, 128, 1, 8), 0.1);
  pselect->AddMutator(new InterstingValueMutator(true), 0.1);
  pselect->AddMutator(new SpliceMutator(1, 0.5), 0.1);
  pselect->AddMutator(new SpliceMutator(2, 0.5), 0.1);

  // potentially repeat the mutation
  // (do two or more mutations in a single cycle
  RepeatMutator *repeater = new RepeatMutator(pselect, 0.5);

  // and have 1000 rounds of this per sample cycle
  NRoundMutator *mutator = new NRoundMutator(repeater, 1000);

  return mutator;
}

class GrammarFuzzer : public Fuzzer {
public:
  GrammarFuzzer(const char *grammar_file);
protected:
  Grammar grammar;
  Mutator* CreateMutator(int argc, char** argv, ThreadContext* tc) override;
  Minimizer* CreateMinimizer(int argc, char** argv, ThreadContext* tc) override;
  bool OutputFilter(Sample* original_sample, Sample* output_sample) override;

  bool IsReturnValueInteresting(uint64_t return_value) override;
};

GrammarFuzzer::GrammarFuzzer(const char* grammar_file) {
  if (!grammar.Read(grammar_file)) {
    FATAL("Error reading grammar");
  }
}

Mutator* GrammarFuzzer::CreateMutator(int argc, char** argv, ThreadContext* tc) {
  GrammarMutator* grammar_mutator = new GrammarMutator(&grammar);

  NRoundMutator* mutator = new NRoundMutator(grammar_mutator, 20);

  return mutator;
}

Minimizer* GrammarFuzzer::CreateMinimizer(int argc, char** argv, ThreadContext* tc) {
  return new GrammarMinimizer(&grammar);
}

bool GrammarFuzzer::OutputFilter(Sample* original_sample, Sample* output_sample) {
  uint64_t string_size = *((uint64_t*)original_sample->bytes);
  if (original_sample->size < (string_size + sizeof(string_size))) {
    FATAL("Incorrectly encoded grammar sample");
  }

  output_sample->Init(original_sample->bytes + sizeof(string_size), string_size);
  return true;
}

bool GrammarFuzzer::IsReturnValueInteresting(uint64_t return_value) {
  return (return_value == 0);
}

int main(int argc, char **argv)
{
  Fuzzer* fuzzer;

  char* grammar = GetOption("-grammar", argc, argv);
  if (grammar) {
    fuzzer = new GrammarFuzzer(grammar);
  } else {
    fuzzer = new BinaryFuzzer();
  }

  fuzzer->Run(argc, argv);
  return 0;
}
