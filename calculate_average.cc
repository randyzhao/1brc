#include <iostream>
#include <fstream>
#include <string>
#include <unordered_map>
#include <algorithm>
#include <vector>
#include <chrono>
#include <functional>
#include <sys/mman.h>

bool DEBUGGING = true;

std::string inputFileName = "./measurements.txt";


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

int fastS2I(std::string& line, int startIndex) {
  int ptr = startIndex;
  bool isPos = true;
  if (line[ptr] == '+') {
    ++ptr;
  } else if (line[ptr] == '-') {
    ++ptr;
    isPos = false;
  }

  int result = 0;
  for (;ptr < line.length(); ++ptr) {
    if (line[ptr] == '.') {
      continue;
    }

    result = result * 10 + (line[ptr] - '0');
  }

  return isPos ? result : -result;
}

void parseLineAndRecord(std::string& line, Stations& stations) {
  std::string name;
  int ptr;
  for (ptr = 0; ptr < line.length(); ++ptr) {
    if (line[ptr] == ';') {
      name = line.substr(0, ptr);
      break;
    }
  }

  int temp = fastS2I(line, ptr + 1);
  // int temp = stoi(line.substr(ptr + 1, line.length() - (ptr + 1)));

  stations.try_emplace(name, Station());
  stations[name].addMeasurement(temp);
}

int main(int argc, char** argv) {
  if (argc > 1) {
    inputFileName = argv[1];
  }

  Stations stations;

  std::ifstream fin(inputFileName);
  std::string line;

  while (std::getline(fin, line)) {
    parseLineAndRecord(line, stations);
  }

  output(stations);

  return 0;
}
