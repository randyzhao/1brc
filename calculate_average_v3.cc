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

std::string inputFileName = "./src/test/resources/samples/measurements-10.txt";
// std::string inputFileName = "./measurements.txt";

int THREADS_COUNT = 6;
int LEFT_OVER_BUFFER_SIZE = 1000;
int HANDLE_LEFT_OVER_BUFFER_THREASHOLD = 700;

std::mutex StationsMutex;

/**
 * Temperatures are multiplied by 10 as stored as int
*/
class Station {
public:
  int minTemp = 9999999;
  int maxTemp = -9999999;
  int totalTemp = 0;
  int measurementCount = 0;
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

  void merge(Station& other) {
    totalTemp += other.totalTemp;
    measurementCount += other.measurementCount;

    minTemp = std::min(minTemp, other.minTemp);
    maxTemp = std::max(maxTemp, other.maxTemp);
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
      << std::round(station.averageTemp()) / 10 << "/"
      << (float) station.maxTemp / 10;
    if (i != names.size() - 1) {
      std::cout << ", ";
    }
  }

  std::cout << "}" << std::endl;
}

int fastS2I(const char *data, int startIndex, int& result) {
  int temp10 = 0;

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

    temp10 = temp10 * 10 + (data[ptr] - '0');
  }

  result = isPos ? temp10 : -temp10;

  return ptr;
}

/**
 * Find the index of last \n in data, in the range of [startIdx, endIdx)
*/
int findLastRowEnd(const char* data, int startIdx, int endIdx) {
  int ptr = endIdx - 1;
  for (;ptr >= startIdx;--ptr) {
    if (data[ptr] == '\n') {
      return ptr;
    }
  }
  return ptr;
}

/**
 * Similar to findLastRowEnd, but if the last row is incomplete, find its
 * end without exceeding dataSize.
*/
int findLastRowEndExtended(const char* data, int startIdx, int endIdx, int dataSize) {
  int lastRowEnd = findLastRowEnd(data, startIdx, endIdx);
  if (lastRowEnd == endIdx - 1) {
    return lastRowEnd;
  }
  int ptr = lastRowEnd + 1;
  while (ptr < dataSize && data[ptr] != '\n') {
    ++ptr;
  }
  return ptr < dataSize ? ptr : lastRowEnd;
}

/**
 * Find the index of first \n in data, in the range of [startIdx, endIdx)
*/
int findFirstRowEnd(const char* data, int startIdx, int endIdx) {
  for (int ptr = startIdx; ptr < endIdx; ++ptr) {
    if (data[ptr] == '\n') {
      return ptr;
    }
  }
  return endIdx;
}

/**
 * Handle the chunk in data with range [startIdx, endIdx).
 * Skip the incomplete row at beginning (as it should be handled by the previous threads)
 * but handle the incomplete row at the end.
*/
void handleChunk(
    const char * data,
    int startIdx,
    int endIdx,
    Stations& stations)
{
  int ptr = startIdx;

  while (ptr < endIdx) {
    // get name
    std::string name;
    const char *nameStart = data + ptr;
    for (;ptr < endIdx; ++ptr) {
      if (data[ptr] == ';') {
        name = std::string(nameStart, data + ptr);
        break;
      }
    }

    ++ptr; // consume ";"
    int temperature10 = -1000;
    ptr = fastS2I(data, ptr, temperature10);
    ++ptr; // consum "\n"

    stations.try_emplace(name, Station());
    stations[name].addMeasurement(temperature10);
  }
}

void handleMappedMemoryWithThreads(
    char *fileData,
    size_t mapSize,
    std::string& prevLeftOverData,
    std::vector<Stations>& threadStations
) {
  int firstEndOfRow = -1;
  if (prevLeftOverData.size() != 0) {
    firstEndOfRow = findFirstRowEnd(fileData, 0, mapSize);
    std::string row = prevLeftOverData + std::string(fileData, firstEndOfRow + 1);
    handleChunk(row.c_str(), 0, row.length(), threadStations[0]);
    prevLeftOverData = "";
  }

  int lastEndOfRow = findLastRowEnd(fileData, 0, mapSize);
  if (lastEndOfRow != mapSize - 1) {
    prevLeftOverData = std::string(fileData + lastEndOfRow + 1, mapSize - lastEndOfRow - 1);
  }

  std::vector<std::thread> threads;

  int threadBeginIdx = firstEndOfRow + 1;
  for (int i = 0; i < THREADS_COUNT; ++i) {
    int threadEndIdx = std::min(threadBeginIdx+ CHUNK_SIZE, mapSize);
    int threadEndIdxExtended = findLastRowEndExtended(
      fileData,
      threadBeginIdx,
      threadEndIdx,
      mapSize
    );

    // std::cerr << "threadId: " << i << " " << threadBeginIdx << " " << threadEndIdx + 1
    //   << std::endl;

    threads.emplace_back(
      handleChunk,
      fileData,
      threadBeginIdx,
      threadEndIdxExtended + 1,
      std::ref(threadStations[i])
    );

    // handleChunk(
    //   fileData,
    //   threadBeginIdx,
    //   threadEndIdxExtended + 1,
    //   threadStations[i]
    // );

    threadBeginIdx = threadEndIdxExtended + 1;
    if (threadBeginIdx > lastEndOfRow) {
      break;
    }
  }


  for (auto& t: threads) {
    t.join();
  }
}

Stations mergeThreadStations(std::vector<Stations>& threadStations) {
  Stations result;
  for (Stations& st: threadStations) {
    for (auto kv: st) {
      result.try_emplace(kv.first, Station());
      result[kv.first].merge(kv.second);
    }
  }
  return result;
}

int main(int argc, char** argv) {
  if (argc > 1) {
    inputFileName = argv[1];
  }

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

  std::string leftOverData = "";

  std::vector<Stations> threadStations(THREADS_COUNT, Stations());

  while (ptr < fileSize) {
    size_t mapSize = std::min(CHUNK_SIZE * THREADS_COUNT, fileSize - ptr);
    char* fileData = (char*)mmap(nullptr, mapSize, PROT_READ, MAP_PRIVATE, fd, ptr);
    if (fileData == MAP_FAILED) {
      std::cerr << "Error mapping file!" << std::endl;
      close(fd);
      return 1;
    }

    handleMappedMemoryWithThreads(fileData, mapSize, leftOverData, threadStations);

    ptr += mapSize;
    munmap(fileData, mapSize);
  }

  Stations merged = mergeThreadStations(threadStations);
  output(merged);

  return 0;
}
