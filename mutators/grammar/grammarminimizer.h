#pragma once

#include "grammar.h"
#include "../../minimizer.h"

class GrammarMinimizerContext : public MinimizerContext {
public:
  GrammarMinimizerContext() : tree(NULL), num_modes_initial(0), num_modes_removed(0) { }
  ~GrammarMinimizerContext() { if (tree) delete tree; }

  Grammar::TreeNode* tree;
  std::vector<Grammar::TreeNode*> minimization_candidates;
  std::vector<Grammar::TreeNode*> removed_children;
  size_t current_candidate;
  size_t current_candidate_pos;

  size_t num_modes_initial;
  size_t num_modes_removed;
};

class GrammarMinimizer : public Minimizer {
public:
  GrammarMinimizer(Grammar* grammar, int minimization_limit) : 
    grammar(grammar), minimization_limit(minimization_limit) 
    { }

  MinimizerContext* CreateContext(Sample* sample) override;

  void GetMinimizationCandidates(Grammar::TreeNode* tree, GrammarMinimizerContext* context);

  int MinimizeStep(Sample* sample, MinimizerContext* context) override;

  void ReportSuccess(Sample* sample, MinimizerContext* context) override;
  void ReportFail(Sample* sample, MinimizerContext* context) override;

  Grammar* grammar;
  int minimization_limit;
};

