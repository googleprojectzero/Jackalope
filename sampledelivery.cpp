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
#include "sampledelivery.h"

int FileSampleDelivery::DeliverSample(Sample *sample) {
  return sample->Save(filename.c_str());
}

SHMSampleDelivery::SHMSampleDelivery(char *name, size_t size) {
  shmobj.Open(name, size);
  shm = shmobj.GetData();
}

SHMSampleDelivery::~SHMSampleDelivery() {
  shmobj.Close();
}

int SHMSampleDelivery::DeliverSample(Sample *sample) {
  uint32_t *size_ptr = (uint32_t *)shm;
  unsigned char *data_ptr = shm + 4;
  *size_ptr = (uint32_t)sample->size;
  memcpy(data_ptr, sample->bytes, sample->size);
  return 1;
}

