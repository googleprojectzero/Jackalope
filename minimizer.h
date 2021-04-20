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

#include "sample.h"

class MinimizerContext {
public:
  virtual ~MinimizerContext() { }
};

class Minimizer {
public:
  virtual MinimizerContext* CreateContext(Sample* sample) { return NULL; };
  // should return 0 when minimizing done
  virtual int MinimizeStep(Sample* sample, MinimizerContext *context) { return 0; };
  virtual void ReportSuccess(Sample* sample, MinimizerContext* context) { };
  virtual void ReportFail(Sample* sample, MinimizerContext* context) { };
};

#define TRIM_STEP_INITIAL 16

class SimpleTrimmerContext : public MinimizerContext {
public:
  SimpleTrimmerContext() { trim_step = TRIM_STEP_INITIAL; }
  int trim_step;
};

class SimpleTrimmer : public Minimizer {
public:
  virtual MinimizerContext* CreateContext(Sample* sample);
  virtual int MinimizeStep(Sample* sample, MinimizerContext* context);
  virtual void ReportFail(Sample* sample, MinimizerContext* context);
};