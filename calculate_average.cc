#include <iostream>
#include <fstream>
#include <string>
#include <unordered_map>
#include <algorithm>
#include <vector>
#include <chrono>
#include <functional>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <thread>
#include <mutex>

bool DEBUGGING = true;

size_t CHUNK_SIZE = 1024 * 1024 * 50; // 50 MB chunks

std::string inputFileName = "./measurements.txt";

int THREADS_COUNT = 5;
int LEFT_OVER_BUFFER_SIZE = 1000;
int HANDLE_LEFT_OVER_BUFFER_THREASHOLD = 700;

std::mutex StationsMutex;

/**
 * Temperatures are multiplied by 10 as stored as int
*/
class Station {
public:
  int minTemp;
  int maxTemp;
  int totalTemp;
  int measurementCount;
public:
  void addMeasurement(int temp) {
    totalTemp += temp;
    ++measurementCount;

    if (measurementCount == 1) {
      minTemp = maxTemp = temp;
    }

    if (temp < minTemp) minTemp = temp;
    if (temp > maxTemp) maxTemp = temp;
  }

  float averageTemp() {
    return (float) totalTemp / measurementCount;
  }
};

using Stations = std::unordered_map<std::string, Station>;

void output(Stations& stations) {
  std::vector<std::string> names;
  names.reserve(stations.size());

  for (const auto& pair: stations) {
    names.push_back(pair.first);
  }
  std::sort(names.begin(), names.end());

  std::cout << "{";
  std::cout << std::fixed << std::setprecision(1);

  for (int i = 0; i < names.size(); ++i) {
    std::string& name = names[i];
    Station& station = stations[name];

    std::cout << name << "="
      << (float)station.minTemp / 10 << "/"
      << station.averageTemp() / 10 << "/"
      << (float) station.maxTemp / 10;
    if (i != names.size() - 1) {
      std::cout << ", ";
    }
  }

  std::cout << "}" << std::endl;
}

int fastS2I(char *data, int startIndex, int& result) {
  int ptr = startIndex;
  bool isPos = true;
  if (data[ptr] == '+') {
    ++ptr;
  } else if (data[ptr] == '-') {
    ++ptr;
    isPos = false;
  }

  for (;data[ptr] != '\n'; ++ptr) {
    if (data[ptr] == '.') {
      continue;
    }

    result = result * 10 + (data[ptr] - '0');
  }

  if (isPos) {
    result = -result;
  }

  return ptr;
}

/**
 * Handle the chunk in data with range [startIdx, endIdx)
*/
void handleChunk(
    char * data,
    int startIdx,
    int endIdx,
    std::function<void(std::string&, int)> addMeasurement)
{
  int ptr = startIdx;
  while (ptr < endIdx) {
    // get name
    std::string name;
    char *nameStart = data + ptr;
    for (;ptr < endIdx; ++ptr) {
      if (data[ptr] == ';') {
        name = std::string(nameStart, data + ptr);
        break;
      }
    }

    ++ptr; // consume ";"
    int temperature10 = -1000;
    ptr = fastS2I(data, ptr, temperature10);

    addMeasurement(name, temperature10);
  }
}

/**
 * Find the index of last \n in data, in the range of [startIdx, endIdx)
*/
int findLastRowEnd(char* data, int startIdx, int endIdx) {
  int ptr = endIdx - 1;
  for (;ptr >= startIdx;--ptr) {
    if (data[ptr] == '\n') {
      return ptr;
    }
  }
  return ptr;
}

/**
 * Find the index of first \n in data, in the range of [startIdx, endIdx)
*/
int findFirstRowEnd(char* data, int startIdx, int endIdx) {
  for (int ptr = startIdx; ptr < endIdx; ++ptr) {
    if (data[ptr] == '\n') {
      return ptr;
    }
  }
  return endIdx;
}

int main(int argc, char** argv) {
  if (argc > 1) {
    inputFileName = argv[1];
  }

  Stations stations;

  int fd = open(inputFileName.c_str(), O_RDONLY);
  if (fd == -1) {
    std::cerr << "Error opening file: " << inputFileName << std::endl;
    return 1;
  }

  struct stat sb;
  if (fstat(fd, &sb)) {
    std::cerr << "Error getting file size" << std::endl;
    close(fd);
    return 1;
  }

  size_t fileSize = sb.st_size;
  size_t ptr = 0;

  char leftOverRowBuffer[LEFT_OVER_BUFFER_SIZE];
  memset(leftOverRowBuffer, 0, sizeof(char) * LEFT_OVER_BUFFER_SIZE);
  int leftOverRowBufferPtr = 0;

  std::vector<std::thread> threads;

  while (ptr < fileSize) {
    size_t mapSize = std::min(CHUNK_SIZE, fileSize - ptr);
    char* fileData = (char*)mmap(nullptr, mapSize, PROT_READ, MAP_PRIVATE, fd, ptr);
    if (fileData == MAP_FAILED) {
      std::cerr << "Error mapping file!" << std::endl;
      close(fd);
      return 1;
    }

    int firstEndOfRow = findFirstRowEnd(fileData, 0, mapSize);
    int lastEndOfRow = findLastRowEnd(fileData, 0, mapSize);

    memcpy(leftOverRowBuffer + leftOverRowBufferPtr, fileData, firstEndOfRow);
    leftOverRowBufferPtr += firstEndOfRow;

    auto addMeasurement = [&stations](std::string& name, int temperature10) {
      std::lock_guard<std::mutex> lock(StationsMutex);
      stations.try_emplace(name, Station());
      stations[name].addMeasurement(temperature10);
    };

    // TODO: unordered_map is not thread safe
    threads.emplace_back(handleChunk, fileData, firstEndOfRow + 1, mapSize, addMeasurement);

    if (leftOverRowBufferPtr >= HANDLE_LEFT_OVER_BUFFER_THREASHOLD) {
      handleChunk(
        leftOverRowBuffer,
        0,
        leftOverRowBufferPtr,
        addMeasurement
      );
      memset(leftOverRowBuffer, 0, sizeof(char) * LEFT_OVER_BUFFER_SIZE);
      leftOverRowBufferPtr = 0;
    }
    memcpy(leftOverRowBuffer + leftOverRowBufferPtr, fileData + lastEndOfRow + 1, mapSize - lastEndOfRow - 1);
    leftOverRowBufferPtr += mapSize - lastEndOfRow - 1;

    if (threads.size() >= THREADS_COUNT) {
      for (auto& t: threads) {
        t.join();
      }
      threads.clear();
    }

    ptr += mapSize;
    munmap(fileData, mapSize);
  }

  output(stations);

  return 0;
}
