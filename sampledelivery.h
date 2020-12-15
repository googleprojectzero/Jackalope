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
#include "sample.h"

class SampleDelivery {
public:
  virtual ~SampleDelivery() {}
  virtual void Init(int argc, char **argv) {}

  // returns nonzero on success
  virtual int DeliverSample(Sample *sample) = 0;
};

class FileSampleDelivery : public SampleDelivery {
public:

  void SetFilename(std::string filename) {
    this->filename = filename;
  }

  int DeliverSample(Sample *sample);
  
protected:

  std::string filename;
};

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32)
#include "windows.h"
#endif

class SHMSampleDelivery : public SampleDelivery {
public:
  SHMSampleDelivery(char *name, size_t size);
  ~SHMSampleDelivery();

  int DeliverSample(Sample *sample);

protected:
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32)
  HANDLE shm_handle;
#else
  int fd;
#endif
  char *name;
  size_t size;
  unsigned char *shm;
};

