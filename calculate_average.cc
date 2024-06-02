#include <iostream>
#include <fstream>
#include <string>
#include <unordered_map>
#include <algorithm>
#include <vector>


std::string inputFileName = "./measurements.txt";

class Station {
public:
  double minTemp;
  double maxTemp;
  double totalTemp;
  int measurementCount;
public:
  void addMeasurement(double temp) {
    totalTemp += temp;
    ++measurementCount;

    if (measurementCount == 1) {
      minTemp = maxTemp = temp;
    }

    if (temp < minTemp) minTemp = temp;
    if (temp > maxTemp) maxTemp = temp;
  }

  double averageTemp() {
    return totalTemp / measurementCount;
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
      << station.minTemp << "/"
      << station.averageTemp() << "/"
      << station.maxTemp;
    if (i != names.size() - 1) {
      std::cout << ", ";
    }
  }

  std::cout << "}" << std::endl;
}

int main(int argc, char** argv) {
  if (argc > 1) {
    inputFileName = argv[1];
  }

  // std::cout << "file name: " << inputFileName << std::endl;

  Stations stations;

  std::ifstream fin(inputFileName);
  std::string line;

  while (std::getline(fin, line)) {
    // std::cout << "line: " << line << std::endl;
    std::size_t splitLoc = line.find(';');
    std::string stationName = line.substr(0, splitLoc);
    std::string measurementStr = line.substr(splitLoc + 1, line.length() - splitLoc);
    if (stations.find(stationName) == stations.end()) {
      stations.emplace(stationName, Station());
    }
    stations[stationName].addMeasurement(std::stod(measurementStr));
  }

  output(stations);

  return 0;
}
