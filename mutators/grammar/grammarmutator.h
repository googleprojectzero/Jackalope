#pragma once

#include "../../mutator.h"
#include "../../mutex.h"
#include "grammar.h"

// The mutator context for Grammar mutator
// is the tree structure of the current sample
class GrammarMutatorContext : public MutatorSampleContext {
public:
  GrammarMutatorContext(Sample* sample, Grammar* grammar);

  Grammar::TreeNode* tree;
};

class GrammarMutator : public Mutator {
public:
  // Mutator interface method
  GrammarMutator(Grammar* grammar) : grammar(grammar) { }
  bool CanGenerateSample() override { return true; }
  bool GenerateSample(Sample* sample, PRNG* prng) override;
  void InitRound(Sample* input_sample, MutatorSampleContext* context) override;
  bool Mutate(Sample* inout_sample, PRNG* prng, std::vector<Sample*>& all_samples) override;
  MutatorSampleContext* CreateSampleContext(Sample* sample) override;

protected:
  // MUTATORS:

  // 1) Re-generates a random node
  int ReplaceNode(Grammar::TreeNode* tree, PRNG* prng);

  // 2) Replaces a node from the current sample
  //    With an equivalent node from another sample
  int Splice(Grammar::TreeNode* tree, PRNG* prng);

  // 3) Selects a <repeat> node from the current sample
  //    and adds/potentially removes children from it
  int RepeatMutator(Grammar::TreeNode* tree, PRNG* prng);

  // 4) Selects a <repeat> node from the current sample
  //    and a similar <repeat> node from another sample.
  //    Mixes children from the other node into the current node.
  int RepeatSplice(Grammar::TreeNode* tree, PRNG* prng);

  // repeately attempts to generate a tree until an attempt is successful
  Grammar::TreeNode* GenerateTreeNoFail(Grammar::Symbol* symbol, PRNG* prng);
  Grammar::TreeNode* GenerateTreeNoFail(const char *symbol, PRNG* prng);

  Grammar::TreeNode* current_sample;

  struct MutationCandidate {
    Grammar::TreeNode* node;
    int depth;
    double p;
  };

  // list of candidatate tree nodes for mutation
  // allocated here to avoid allocaing for each iteration
  std::vector<MutationCandidate> candidates;
  std::vector<MutationCandidate> splice_candidates;
  std::vector<MutationCandidate> repeat_candidates;

  // creates a list of mutation candidates based on params
  double GetMutationCandidates(std::vector<MutationCandidate>& candidates, Grammar::TreeNode* node, Grammar::Symbol* filter, int depth, int maxdepth, double p, bool just_repeat = false);

  // selects a node to mutate from a list of candidates based on candidate probability
  MutationCandidate* GetNodeToMutate(std::vector<MutationCandidate> &candidates, PRNG* prng);

  // global vector of other trees with unique coverage
  // used by splice mutator etc.
  static std::vector<Grammar::TreeNode*> interesting_trees;
  Mutex interesting_trees_mutex;

  Grammar *grammar;
};

