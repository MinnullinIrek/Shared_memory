#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <Windows.h>
#include <cstring>
#include <mutex>
#include <atomic>
#include <random>
#include <ctime>

#include "md5.h"


const int hashSize = 16;
const int DATA_SIZE = 1024;
struct SharedData {
  std::atomic<bool> clientStopper = false;
  std::atomic<bool> serverStopper = false;

  char text[DATA_SIZE];
};


unsigned char* getControlSum(char* str);

int checkControlSum(char* str);

std::string createData(char* str);

int server(SharedData* data);

int client(const char* mem, SharedData* data);

int main(int argc, char* argv[]) {

  HANDLE hMemory = CreateFileMappingW(nullptr, nullptr, PAGE_READWRITE, 0, sizeof(SharedData), L"Local\\shared-memory");
  if (!hMemory) {
    std::cerr << "Create fileMapping error : " << ::GetLastError();
    return (GetLastError());
  }

  SharedData* sharedData = (SharedData*)::MapViewOfFile(hMemory, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, 0);
  if (!sharedData) {
    std::cerr << "MapViewOfFile error: " << ::GetLastError();
    return (GetLastError());
  }


  auto isClient = sharedData->clientStopper.exchange(true);
  sharedData->serverStopper = false;

  if (!isClient) {
    server(sharedData);
  } else {
    char mem[DATA_SIZE];
    createData(mem);
    client(mem, sharedData);
  }

  UnmapViewOfFile(sharedData);
  CloseHandle(hMemory);

  return 0;
}

unsigned char* getControlSum(char* str) {
  md5_state_t state;
  md5_byte_t digest[16];

  md5_init(&state);
  md5_append(&state, (const md5_byte_t*)str, DATA_SIZE - 1 - hashSize);
  md5_finish(&state, digest);
  return digest;
}

int checkControlSum(char* str) {
  char* hash = (char*)getControlSum(str);
  for (int i = 0; i < hashSize; ++i) {
    if (str[DATA_SIZE - 1 - hashSize + i] != hash[i]) {
      return 1;
    }
  }

  return 0;
}

std::string createData(char* str) {

  std::mt19937 gen(time(0));
  std::uniform_int_distribution<> uid(0, 253);
  for (int i = 0; i < DATA_SIZE - 1 - hashSize; ++i) {
    str[i] = uid(gen);
  }

  memcpy(str + DATA_SIZE - 1 - hashSize, getControlSum(str), hashSize);

  return str;
}

int server(SharedData* data) {
  static time_t lastTime = time(NULL);
  static double byteCount = 0;
  static const int interval = 3;
  while (true) {
    if (data->serverStopper.load() == true) {
      if (checkControlSum(data->text) != 0) {
        std::cerr << "Wrong control sum" << std::endl;
      }
      data->serverStopper = false;
      data->clientStopper = true;
      byteCount += DATA_SIZE;
      time_t curTime = time(NULL);
      if (curTime - lastTime >= interval) {
        std::cout << byteCount / (curTime - lastTime) << std::endl;
        lastTime = curTime;
        byteCount = 0;
      }
    }
  }
  return 0;
}

int client(const char* mem, SharedData* data) {
  while (true) {
    if (data->clientStopper.exchange(false)) {
      memcpy(data->text, mem, DATA_SIZE);
      data->serverStopper = true;
      Sleep(500);
    }
  }
  return 0;
}