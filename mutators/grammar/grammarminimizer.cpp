#include "common.h"
#include "grammarminimizer.h"

MinimizerContext* GrammarMinimizer::CreateContext(Sample* sample) {
  GrammarMinimizerContext* context = new GrammarMinimizerContext();

  context->tree = grammar->DecodeSample(sample);

  GetMinimizationCandidates(context->tree, context);

  context->current_candidate = context->minimization_candidates.size() - 1;

  if (context->current_candidate != ((size_t)-1)) {
    Grammar::TreeNode* current_node = context->minimization_candidates[context->current_candidate];
    context->current_candidate_pos = current_node->children.size();
  }

  context->num_modes_initial = context->tree->NumNodes();

  return context;
}

void GrammarMinimizer::GetMinimizationCandidates(Grammar::TreeNode* tree, GrammarMinimizerContext* context) {
  if (tree->type == Grammar::STRINGTYPE) return;

  Grammar::Symbol* symbol = tree->symbol;

  if ((symbol->can_be_empty || symbol->repeat) && !tree->children.empty()) {
    context->minimization_candidates.push_back(tree);
  }

  for (auto iter = tree->children.begin(); iter != tree->children.end(); iter++) {
    GetMinimizationCandidates(*iter, context);
  }
}

int GrammarMinimizer::MinimizeStep(Sample* sample, MinimizerContext* context) {
  GrammarMinimizerContext* gcontext = (GrammarMinimizerContext*)context;

  if ((gcontext->num_modes_initial - gcontext->num_modes_removed) <= minimization_limit) return 0;

  Grammar::TreeNode* current_node;
  Grammar::Symbol* current_symbol;

  if (gcontext->current_candidate == ((size_t)-1)) return 0;

  current_node = gcontext->minimization_candidates[gcontext->current_candidate];
  current_symbol = current_node->symbol;

  while (gcontext->current_candidate_pos == 0) {
    gcontext->current_candidate--;
    if (gcontext->current_candidate == ((size_t)-1)) return 0;
    current_node = gcontext->minimization_candidates[gcontext->current_candidate];
    current_symbol = current_node->symbol;
    gcontext->current_candidate_pos = current_node->children.size();
  }

  gcontext->removed_children.clear();

  if (current_symbol->repeat) {
    gcontext->current_candidate_pos--;
    auto iter = current_node->children.begin();
    for (size_t i = 0; i < gcontext->current_candidate_pos; i++) {
      iter++;
    }
    gcontext->removed_children.push_back(*iter);
    current_node->children.erase(iter);
  } else if (current_symbol->can_be_empty) {
    for (auto iter = current_node->children.begin(); iter != current_node->children.end(); iter++) {
      gcontext->removed_children.push_back(*iter);
    }
    current_node->children.clear();
    gcontext->current_candidate_pos = 0;
  }

  grammar->EncodeSample(gcontext->tree, sample);

  return 1;
}

void GrammarMinimizer::ReportSuccess(Sample* sample, MinimizerContext* context) {
  GrammarMinimizerContext* gcontext = (GrammarMinimizerContext*)context;

  for (auto iter = gcontext->removed_children.begin(); iter != gcontext->removed_children.end(); iter++) {
    Grammar::TreeNode* child = *iter;
    gcontext->num_modes_removed += child->NumNodes();
    delete child;
  }

  // TODO calculate number of nodes removed
  gcontext->removed_children.clear();
}

void GrammarMinimizer::ReportFail(Sample* sample, MinimizerContext* context) {
  GrammarMinimizerContext* gcontext = (GrammarMinimizerContext*)context;

  Grammar::TreeNode* current_node;
  Grammar::Symbol* current_symbol;
  current_node = gcontext->minimization_candidates[gcontext->current_candidate];
  current_symbol = current_node->symbol;

  auto iter = current_node->children.begin();
  for (size_t i = 0; i < gcontext->current_candidate_pos; i++) {
    iter++;
  }

  current_node->children.insert(iter, gcontext->removed_children.begin(), gcontext->removed_children.end());

  gcontext->removed_children.clear();
}
