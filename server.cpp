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

#define _CRT_SECURE_NO_WARNINGS

#include "server.h"
#include "directory.h"
#include "common.h"
#include "thread.h"

int ServerCommon::Read(socket_type sock, void *buf, size_t size) {
  int ret;
  size_t to_read = size;
  char *p = (char *)buf;
  //printf("starting read\n");
  while (to_read) {
    int read_chunk;
    if (to_read > 0x100000) {
      read_chunk = 0x100000;
    } else {
      read_chunk = (int)to_read;
    }
    ret = recv(sock, p, read_chunk, 0);
    //printf("read returned %ld\n", ret);
    if (ret > 0) {
      to_read -= ret;
      p += ret;
    } else {
      return 0;
    }
  }
  return 1;
}

int ServerCommon::Write(socket_type sock, const char *buf, size_t size) {
  int ret;
  size_t to_write = size;
  const char *p = (char *)buf;
  while (to_write) {
    int write_chunk;
    if (to_write > 0x100000) {
      write_chunk = 0x100000;
    } else {
      write_chunk = (int)to_write;
    }
    ret = send(sock, p, write_chunk, 0);
    //printf("read returned %ld\n", ret);
    if (ret > 0) {
      to_write -= ret;
      p += ret;
    } else {
      return 0;
    }
  }
  return 1;
}


int ServerCommon::SendSample(socket_type sock, Sample &sample) {
  uint64_t sample_size = sample.size;
  Write(sock, (const char *)(&sample_size), sizeof(sample_size));
  Write(sock, sample.bytes, sample.size);
  return 1;
}

int ServerCommon::RecvSample(socket_type sock, Sample &sample) {
  uint64_t sample_size;
  if (!Read(sock, &sample_size, sizeof(sample_size))) {
    return 0;
  }
  char *new_bytes = (char*)malloc(sample_size);
  if (!new_bytes) return 0;
  if (!Read(sock, new_bytes, sample_size)) {
    free(new_bytes);
    return 0;
  }
  sample.Init(new_bytes, (size_t)sample_size);
  free(new_bytes);
  return 1;
}

int ServerCommon::SendString(socket_type sock, std::string &str) {
  uint64_t size = str.size();
  Write(sock, (const char *)(&size), sizeof(size));
  Write(sock, str.data(), size);
  return 1;
}

int ServerCommon::RecvString(socket_type sock, std::string &str) {
  uint64_t size;
  if (!Read(sock, &size, sizeof(size))) {
    return 0;
  }
  char *tmp_data = (char *)malloc(size);
  if (!tmp_data) return 0;
  if (!Read(sock, tmp_data, size)) {
    free(tmp_data);
    return 0;
  }
  str = std::string(tmp_data, size);
  free(tmp_data);
  return 1;
}

int ServerCommon::SendCoverage(socket_type sock, Coverage &coverage) {
  for (auto iter = coverage.begin(); iter != coverage.end(); iter++) {
    uint64_t num_offsets = iter->offsets.size();
    uint64_t *offsets = (uint64_t *)malloc(num_offsets * sizeof(uint64_t));

    size_t i = 0;
    for (auto iter2 = iter->offsets.begin(); iter2 != iter->offsets.end(); iter2++) {
      offsets[i] = *iter2;
      i++;
    }

    send(sock, "C", 1, 0);
    SendString(sock, iter->module_name);
    send(sock, (char *)&num_offsets, sizeof(num_offsets), 0);
    send(sock, (char *)offsets, (int)(num_offsets * sizeof(uint64_t)), 0);

    free(offsets);
  }
  
  send(sock, "N", 1, 0);

  return 1;
}

int ServerCommon::RecvCoverage(socket_type sock, Coverage &coverage) {
  char command;
  std::string module_name;
  uint64_t num_offsets;
  uint64_t *offsets;

  while (1) {
    if (!Read(sock, &command, 1)) {
      return 0;
    }

    if (command == 'N') break;

    if (command != 'C') return 0;

    if (!RecvString(sock, module_name)) {
      return 0;
    }

    ModuleCoverage *module_coverage = GetModuleCoverage(coverage, module_name);
    if(!module_coverage) {
      coverage.push_back({ module_name, {} });
      module_coverage = GetModuleCoverage(coverage, module_name);
    }

    if (!Read(sock, &num_offsets, sizeof(num_offsets))) {
      return 0;
    }

    if (num_offsets > SIZE_MAX / sizeof(uint64_t)) {
      return 0;
    }

    offsets = (uint64_t *)malloc(num_offsets * sizeof(uint64_t));
    if (!offsets) return 0;

    if (!Read(sock, offsets, (int)(num_offsets * sizeof(uint64_t)))) {
      free(offsets);
      return 0;
    }

    for (size_t i = 0; i < num_offsets; i++) {
      module_coverage->offsets.insert(offsets[i]);
    }
  }
  
  return 1;
}

uint64_t CoverageServer::GetIndex(std::vector<TimestampIndex> &timestamps, uint64_t timestamp, uint64_t last_index) {
  if (timestamp == 0) return 0;
  if (timestamps.empty()) return 0;
  if (timestamp >= timestamps[timestamps.size() - 1].timestamp) return last_index;

  int64_t l = 0;
  int64_t r = timestamps.size() - 1;
  int64_t m;

  while (l <= r) {
    m = (l + r) / 2;
    if (timestamps[m].timestamp < timestamp) {
      l = m + 1;
    } else if (timestamps[m].timestamp > timestamp) {
      r = m - 1;
    } else {
      break;
    }
  }

  if (timestamps[m].timestamp > timestamp) {
    while ((m > 0) && (timestamps[m - 1].timestamp > timestamp)) m--;
  } else {
    while ((m < (int64_t)(timestamps.size() - 1)) && (timestamps[m].timestamp <= timestamp)) m++;
  }

  if (timestamps[m].timestamp <= timestamp) {
    FATAL("Error in GetIndex");
  }

  return timestamps[m].index;
}

int CoverageServer::ServeUpdates(socket_type sock) {
  uint64_t timestamp;
  uint64_t client_id, client_execs;

  if (!Read(sock, &client_id, sizeof(client_id))) {
    return 0;
  }

  if (!Read(sock, &client_execs, sizeof(client_execs))) {
    return 0;
  }

  printf("Client %016llx reported %llu total execs\n", client_id, client_execs);

  if (!Read(sock, &timestamp, sizeof(timestamp))) {
    return 0;
  }

  mutex.LockRead();

  Write(sock, (const char *)(&server_timestamp), sizeof(server_timestamp));

  if (timestamp >= server_timestamp) {
    send(sock, "N", 1, 0);
    mutex.UnlockRead();
    return 1;
  }

  uint64_t first_index = GetIndex(corpus.timestamps, timestamp, corpus.samples.size());
  if (first_index >= corpus.samples.size()) {
    send(sock, "N", 1, 0);
    mutex.UnlockRead();
    return 1;
  }

  for (size_t i = first_index; i < corpus.samples.size(); i++) {
    Sample &sample = corpus.samples[i];
    send(sock, "S", 1, 0);

    if (!SendSample(sock, sample)) {
      mutex.UnlockRead();
      return 0;
    }
  }

  send(sock, "N", 1, 0);

  mutex.UnlockRead();

  return 1;
}

bool CoverageServer::HasNewCoverage(Coverage *client_coverage, Coverage *new_coverage) {
  CoverageDifference(total_coverage, *client_coverage, *new_coverage);
  return (!new_coverage->empty());
}

bool CoverageServer::OnNewCoverage(Coverage *client_coverage) {
  Coverage new_client_coverage;
  CoverageDifference(total_coverage, *client_coverage, new_client_coverage);
  if (new_client_coverage.empty()) {
    return false;
  }
  server_timestamp++;
  MergeCoverage(total_coverage, new_client_coverage);
  return true;
}

int CoverageServer::ReportNewCoverage(socket_type sock) {
  char command;
  
  Coverage client_coverage;
  Coverage new_client_coverage;

  if(!RecvCoverage(sock, client_coverage)) {
    return 0;
  }
  
  mutex.LockRead();
  if (!HasNewCoverage(&client_coverage, &new_client_coverage)) {
    mutex.UnlockRead();
    send(sock, "N", 1, 0);
    return 1;
  }
  mutex.UnlockRead();

  send(sock, "Y", 1, 0);

  std::list<Sample> new_samples;

  // read samples
  while (1) {
    if (!Read(sock, &command, 1)) {
      return 0;
    }

    if (command == 'N') break;

    if (command != 'S') return 0;

    Sample sample;
    if (!RecvSample(sock, sample)) {
      return 0;
    }

    new_samples.push_back(sample);
  }

  mutex.LockWrite();

  // we need to check coverage twice as another thread could have
  // updated it just before lock
  if (!OnNewCoverage(&new_client_coverage)) {
    mutex.UnlockWrite();
    return 1;
  }

  if (!new_samples.empty()) {
    corpus.timestamps.push_back({ server_timestamp, corpus.samples.size() });
  }

  for(auto iter = new_samples.begin(); iter != new_samples.end(); iter++) {
    char fileindex[20];
    sprintf(fileindex, "%05zu", corpus.samples.size());
    std::string sample_file = DirJoin(sample_dir, std::string("sample_") + fileindex);
    iter->Save(sample_file.c_str());

    corpus.samples.push_back(*iter);
  }

  num_samples = corpus.samples.size();

  mutex.UnlockWrite();

  return 1;
}

bool CoverageServer::CheckFilename(std::string& filename) {
  size_t len = filename.length();
  for (size_t i = 0; i < len; i++) {
    char c = filename[i];
    if ((c >= '0' && c <= '9') ||
        (c >= 'a' && c <= 'z') ||
        (c >= 'A' && c <= 'Z') ||
        (c == '-') ||
        (c == '_'))
    {
      continue;
    } else {
      return false;
    }
  }
  return true;
}


int CoverageServer::ReportCrash(socket_type sock) {
  char command;

  while (1) {
    if (!Read(sock, &command, 1)) {
      return 0;
    }

    if (command == 'N') break;

    if (command != 'S') return 0;

    Sample sample;
    if (!RecvSample(sock, sample)) {
      return 0;
    }

    std::string crash_desc;
    if (!RecvString(sock, crash_desc)) {
      return 0;
    }

    if (!CheckFilename(crash_desc)) {
      WARN("Invalid characters in crash filename");
      continue;
    }
    
    bool should_save_crash = false;
    int duplicates = 0;
    
    crash_mutex.Lock();
    num_crashes++;

    auto crash_it = unique_crashes.find(crash_desc);
    if(crash_it == unique_crashes.end()) {
      should_save_crash = true;
      duplicates = 1;
      unique_crashes[crash_desc] = 1;
      num_unique_crashes++;
    } else {
      if(crash_it->second < MAX_SERVER_IDENTICAL_CRASHES) {
        should_save_crash = true;
        crash_it->second++;
        duplicates = crash_it->second;
      }
    }

    if(should_save_crash) {
      std::string crash_filename = crash_desc + "_" + std::to_string(duplicates);
      std::string outfile = DirJoin(crash_dir, crash_filename);
      sample.Save(outfile.c_str());
    }

    crash_mutex.Unlock();
  }

  return 1;
}

void CoverageServer::SaveState() {
  mutex.LockRead();

  std::string out_file = DirJoin(out_dir, std::string("server_state.dat"));
  FILE *fp = fopen(out_file.c_str(), "wb");
  if (!fp) {
    FATAL("Error saving server state");
  }

  fwrite(&num_samples, sizeof(num_samples), 1, fp);

  fwrite(&server_timestamp, sizeof(server_timestamp), 1, fp);

  WriteCoverageBinary(total_coverage, fp);

  uint64_t size;

  // write corpus
  size = corpus.samples.size();
  fwrite(&size, sizeof(size), 1, fp);
  //corpus timestamps
  size = corpus.timestamps.size();
  fwrite(&size, sizeof(size), 1, fp);
  if(size) fwrite(&corpus.timestamps[0], sizeof(corpus.timestamps[0]), size, fp);

  fclose(fp);

  mutex.UnlockRead();
}


void CoverageServer::RestoreState() {
  mutex.LockWrite();

  std::string out_file = DirJoin(out_dir, std::string("server_state.dat"));
  FILE *fp = fopen(out_file.c_str(), "rb");
  if (!fp) {
    FATAL("Error reading server state");
  }

  fread(&num_samples, sizeof(num_samples), 1, fp);

  fread(&server_timestamp, sizeof(server_timestamp), 1, fp);

  ReadCoverageBinary(total_coverage, fp);

  uint64_t size;

  // read corpus
  fread(&size, sizeof(size), 1, fp);
  for (size_t i = 0; i < size; i++) {
    Sample sample;
    char fileindex[20];
    sprintf(fileindex, "%05zu", i);
    std::string sample_file = DirJoin(sample_dir, std::string("sample_") + fileindex);
    sample.Load(sample_file.c_str());
    corpus.samples.push_back(sample);
  }
  //corpus timestamps
  fread(&size, sizeof(size), 1, fp);
  corpus.timestamps.resize(size);
  fread(&corpus.timestamps[0], sizeof(corpus.timestamps[0]), size, fp);

  fclose(fp);

  mutex.UnlockWrite();
}

int CoverageServer::HandleConnection(socket_type sock) {
  int ret = 1;

  char command;

  if (!Read(sock, &command, 1)) {
    return 0;
  }

  size_t cur_n_connections;

  connection_mutex.Lock();
  num_connections++;
  cur_n_connections = num_connections;
  connection_mutex.Unlock();

  if (cur_n_connections > MAX_CONNECTIONS) {
    // tell the client to wait and retry
    send(sock, "W", 1, 0);
  } else {
    send(sock, "K", 1, 0);

    if (command == 'X') {
      ret = ReportCrash(sock);
    } else if (command == 'S') {
      ret = ReportNewCoverage(sock);
    } else if (command == 'U') {
      ret = ServeUpdates(sock);
    } else {
      ret = 0;
    }
  }

  connection_mutex.Lock();
  num_connections--;
  connection_mutex.Unlock();

  return ret;
}

void CoverageServer::StatusThread() {
  int seconds_since_last_save = 0;

  while (1) {

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32)
    Sleep(10000);
#else
    usleep(10000000);
#endif

    seconds_since_last_save += 10;

    printf("Num connections: %zu\n", num_connections);
    printf("Num samples: %zu\n", num_samples);
    printf("Num crashes: %zu (%zu unique)\n", num_crashes, num_unique_crashes);
    printf("\n");

    if (seconds_since_last_save > SERVER_SAVE_INERVAL) {
      SaveState();
      seconds_since_last_save = 0;
    }
  }
}

void *StartServerThread(void *arg) {
  ClientSocket *client_info = (ClientSocket *)arg;
  client_info->server->HandleConnection(client_info->client_socket);
  closesocket(client_info->client_socket);
  delete client_info;
  return NULL;
}

void *StartStatusThread(void *arg) {
  CoverageServer *server = (CoverageServer *)arg;
  server->StatusThread();
  return NULL;
}

void CoverageServer::Init(int argc, char **argv) {
  char *option;

  option = GetOption("-out", argc, argv);
  if (!option) FATAL("No server output dir specified");
  out_dir = option;

  option = GetOption("-start_server", argc, argv);
  if (!option) FATAL("No server output dir specified");
  std::string host_port = option;

  // extract server host and port
  size_t delimiter = host_port.rfind(":");
  if (delimiter == std::string::npos) {
    // consider ip only, use the default port
    server_ip = host_port;
  } else {
    server_ip = host_port.substr(0, delimiter);
    server_port = atoi(host_port.c_str() + delimiter + 1);
  }

  SetupDirectories();

  if (GetBinaryOption("-restore", argc, argv, false) ||
      GetBinaryOption("-resume", argc, argv, false))
  {
    RestoreState();
  }
}

void CoverageServer::SetupDirectories() {
  //create output directories
  CreateDirectory(out_dir);
  crash_dir = DirJoin(out_dir, "server_crashes");
  CreateDirectory(crash_dir);
  sample_dir = DirJoin(out_dir, "server_samples");
  CreateDirectory(sample_dir);
}

void CoverageServer::RunServer() {
  num_connections = 0;

  socket_type listen_socket, client_socket;
  struct sockaddr_in serv_addr;

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32)
  WSADATA wsaData;
  if (WSAStartup(MAKEWORD(2, 2), &wsaData)) {
    FATAL("WSAStartup failed");
  }
#endif

  listen_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (listen_socket == INVALID_SOCKET) {
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32)
  WSACleanup();
#endif
    FATAL("socket failed");
  }

  memset(&serv_addr, '0', sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = inet_addr(server_ip.c_str());
  serv_addr.sin_port = htons(server_port);

  if (bind(listen_socket, (struct sockaddr*)&serv_addr, sizeof(serv_addr))) {
    closesocket(listen_socket);
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32)
    WSACleanup();
#endif
    FATAL("bind failed");
  }

  if (listen(listen_socket, 10)) {
    closesocket(listen_socket);
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32)
    WSACleanup();
#endif
    FATAL("listen failed");
  }

  CreateThread(StartStatusThread, this);

  while (1)
  {
    client_socket = accept(listen_socket, NULL, NULL);
    if (client_socket == INVALID_SOCKET) {
      closesocket(listen_socket);
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32)
      WSACleanup();
#endif
      FATAL("accept failed");
    }

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32)
    DWORD timeout = 10000;
    if (setsockopt(client_socket, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout))) {
#else
    struct timeval tv;
    tv.tv_sec = 10; // 10 Secs Timeout 
    tv.tv_usec = 0;
    if (setsockopt(client_socket, SOL_SOCKET, SO_RCVTIMEO, (char*)&tv, sizeof(struct timeval))) {
#endif
      closesocket(client_socket);
      closesocket(listen_socket);
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32)
      WSACleanup();
#endif
      FATAL("setsockopt failed");
    }

    ClientSocket *client_info = new ClientSocket();
    client_info->client_socket = client_socket;
    client_info->server = this;
    CreateThread(StartServerThread, client_info);
  }

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32)
  WSACleanup();
#endif
}
