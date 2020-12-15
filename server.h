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
#include <string>
#include <unordered_map>
#include "sample.h"
#include "mutex.h"

#include "coverage.h"

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32)
#include <windows.h>
#include "winsock.h"

typedef SOCKET socket_type;
#else

#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

typedef int socket_type;

#define INVALID_SOCKET (-1)

#define closesocket close

#endif 

#define MAX_CONNECTIONS 8

#define DEFAULT_SERVER_PORT 8000

// save state every 20 minutes
#define SERVER_SAVE_INERVAL (5 * 60)

#define MAX_SERVER_IDENTICAL_CRASHES 4

class CoverageServer;

class ClientSocket {
public:
  socket_type client_socket;
  CoverageServer *server;
};

class ServerCommon {
protected:
  int Read(socket_type sock, void *buf, size_t size);
  int Write(socket_type sock, const char *buf, size_t size);
  int SendSample(socket_type sock, Sample &sample);
  int RecvSample(socket_type sock, Sample &sample);
  int SendString(socket_type sock, std::string &str);
  int RecvString(socket_type sock, std::string &str);
  int SendCoverage(socket_type sock, Coverage &coverage);
  int RecvCoverage(socket_type sock, Coverage &coverage);
};

class CoverageServer : public ServerCommon {
public:
  CoverageServer() : server_timestamp(0), server_port(DEFAULT_SERVER_PORT), num_samples(0), num_crashes(0), num_unique_crashes(0) { }

  // for incremental updates
  struct TimestampIndex {
    uint64_t timestamp;
    uint64_t index;
  };

  struct ServerCorpus {
    std::vector<Sample> samples;
    std::vector<TimestampIndex> timestamps;
  };

  ServerCorpus corpus;

  Coverage total_coverage;

  std::string out_dir;

  size_t num_crashes;
  size_t num_unique_crashes;
  std::string crash_dir;

  std::string sample_dir;
  size_t num_samples;

  Mutex connection_mutex;
  size_t num_connections;

  int ServeUpdates(socket_type sock);
  int ReportCrash(socket_type sock);
  int ReportNewCoverage(socket_type sock);
  uint64_t GetIndex(std::vector<TimestampIndex> &timestamps, uint64_t timestamp, uint64_t last_index);

  void SaveState();
  void RestoreState();

  bool OnNewCoverage(Coverage *client_coverage);
  void UpdateModuleCoverage(ModuleCoverage *client_coverage);
  bool HasNewCoverage(Coverage *client_coverage, Coverage *new_coverage);

  void RunServer();
  int HandleConnection(socket_type sock);

  void StatusThread();

  void Init(int argc, char **argv);
  void SetupDirectories();

  bool CheckFilename(std::string& filename);

  uint64_t server_timestamp;

  ReadWriteMutex mutex;

  // separate mutex for writing crashes
  Mutex crash_mutex;
  std::unordered_map<std::string, int> unique_crashes;

  std::string server_ip;
  uint16_t server_port;
};
