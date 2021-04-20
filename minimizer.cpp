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
#include "minimizer.h"

MinimizerContext* SimpleTrimmer::CreateContext(Sample* sample) {
  return new SimpleTrimmerContext();
}

int SimpleTrimmer::MinimizeStep(Sample* sample, MinimizerContext* context) {
  SimpleTrimmerContext* trimmer_context = (SimpleTrimmerContext*)context;

  if (sample->size <= 1) return 0;
  while (trimmer_context->trim_step >= sample->size) {
    trimmer_context->trim_step /= 2;
  }
  if (trimmer_context->trim_step == 0) return 0;

  sample->Trim(sample->size - trimmer_context->trim_step);
  return 1;
}

void SimpleTrimmer::ReportFail(Sample* sample, MinimizerContext* context) {
  SimpleTrimmerContext* trimmer_context = (SimpleTrimmerContext*)context;
  trimmer_context->trim_step /= 2;
}
