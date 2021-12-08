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

#include <list>
#include <string>
#include "sample.h"
#include "server.h"
#include "prng.h"

class CoverageClient : public ServerCommon {
public:
  CoverageClient() : last_timestamp(0), num_samples(0),
    have_server(false), server_port(DEFAULT_SERVER_PORT)
  {
    PRNG::SecureRandom(&client_id, sizeof(client_id));
  }
  ~CoverageClient();

  void Init(int argc, char **argv);

  int ReportNewCoverage(Coverage *new_coverage, Sample *new_sample);
  int GetUpdates(std::list<Sample *> &new_samples, uint64_t total_execs);
  int ReportCrash(Sample *crash, std::string &crash_desc);

  void SaveState(FILE* fp);
  void LoadState(FILE* fp);

private:
  int TryConnectToServer();
  int ConnectToServer(char command);
  int DisconnectFromServer();

  uint64_t last_timestamp;
  uint64_t client_id;
  uint64_t num_samples;

  std::string server_ip;
  uint16_t server_port;

  bool have_server;

  socket_type sock;

  bool keep_samples_in_memory;
  std::string sample_dir;
};
