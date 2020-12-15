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
#include "coverage.h"
#include "client.h"

using namespace std;

void CoverageClient::Init(int argc, char **argv) {
  char *option = GetOption("-server", argc, argv);
  if (!option) {
    have_server = false;
    return;
  }

  have_server = true;
  string host_port = option;

  // extract server host and port
  size_t delimiter = host_port.rfind(":");
  if (delimiter == string::npos) {
    // consider ip only, use the default port
    server_ip = host_port;
  } else {
    server_ip = host_port.substr(0, delimiter);
    server_port = atoi(host_port.c_str() + delimiter + 1);
  }

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32)
  WSADATA wsaData;
  if (WSAStartup(MAKEWORD(2, 2), &wsaData)) {
    FATAL("WSAStartup failed");
  }
#endif
}

CoverageClient::~CoverageClient() {
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32)
  WSACleanup();
#endif
}

int CoverageClient::TryConnectToServer() {
  struct sockaddr_in server;

  sock = INVALID_SOCKET;

  printf("Connecting to server.\n");

  //Create a socket
  if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET)
  {
    printf("Could not create socket\n");
    return 0;
  }

  // printf("Socket created.\n");

  server.sin_addr.s_addr = inet_addr(server_ip.c_str());
  server.sin_family = AF_INET;
  server.sin_port = htons(server_port);

  //Connect to remote server
  if (connect(sock, (struct sockaddr *)&server, sizeof(server)) < 0)
  {
    return 0;
  }

  return 1;
}

int CoverageClient::ConnectToServer(char command) {
  int sleeptime = 10000;
  int maxsleeptime = 5 * 60 * 1000;
  char reply;
  while (1) {
    if (TryConnectToServer()) {
      send(sock, &command, 1, 0);
      if (!Read(sock, &reply, 1)) {
        DisconnectFromServer();
      } else {
        if (reply == 'K') {
          break;
        }
        DisconnectFromServer();
      }
    }
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32)
    Sleep(sleeptime);
#else
    usleep(sleeptime * 1000);
#endif
    sleeptime *= 2;
    if (sleeptime > maxsleeptime) sleeptime = maxsleeptime;
  }
  return 1;
}

int CoverageClient::DisconnectFromServer() {
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32)
  closesocket(sock);
#else
  close(sock);
#endif
  return 1;
}

int CoverageClient::ReportCrash(Sample *crash, std::string &crash_desc) {
  ConnectToServer('X');
  send(sock, "S", 1, 0);
  if (!SendSample(sock, *crash)) {
    DisconnectFromServer();
    return 0;
  }
  if (!SendString(sock, crash_desc)) {
    DisconnectFromServer();
    return 0;
  }
  send(sock, "N", 1, 0);
  DisconnectFromServer();
  return 1;
}

int CoverageClient::ReportNewCoverage(Coverage *new_coverage, Sample *new_sample) {
  ConnectToServer('S');
  
  SendCoverage(sock, *new_coverage);

  char reply;
  if (!Read(sock, &reply, 1)) {
    DisconnectFromServer();
    return 0;
  }

  if (reply == 'N') {
    DisconnectFromServer();
    return 1;
  }

  if (new_sample) {
    send(sock, "S", 1, 0);
    if (!SendSample(sock, *new_sample)) {
      DisconnectFromServer();
    }
  }
  send(sock, "N", 1, 0);

  DisconnectFromServer();
  return 1;
}

int CoverageClient::GetUpdates(std::list<Sample> *new_samples, uint64_t total_execs) {
  uint64_t server_timestamp;
  string module_name;

  ConnectToServer('U');

  send(sock, (char *)&client_id, sizeof(client_id), 0);
  send(sock, (char *)&total_execs, sizeof(total_execs), 0);

  send(sock, (char *)&last_timestamp, sizeof(last_timestamp), 0);
  if (!Read(sock, &server_timestamp, sizeof(server_timestamp))) {
    DisconnectFromServer();
    return 0;
  }

  while (1) {
    char reply;
    if (!Read(sock, &reply, 1)) {
      DisconnectFromServer();
      return 0;
    }

    if (reply == 'N') {
      break;
    } else if (reply == 'S') {
      Sample sample;

      if (!RecvSample(sock, sample)) {
        DisconnectFromServer();
        return 0;
      }

      new_samples->push_back(sample);
    } else {
      DisconnectFromServer();
      return 0;
    }
  }

  last_timestamp = server_timestamp;

  DisconnectFromServer();
  return 1;
}
