#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <random>
#include <chrono>
#include <iomanip>
#include <ctime>

class WeatherStation {
public:
    WeatherStation(const std::string& id, double meanTemperature)
        : id(id),
        meanTemperature(meanTemperature),
        dist(meanTemperature, 10.0) {}

    std::string getId() const {
        return id;
    }

    double measurement(std::default_random_engine& gen) {
        double m = dist(gen);
        return std::round(m * 10.0) / 10.0;
    }

private:
    std::string id;
    double meanTemperature;
    std::normal_distribution<double> dist;
};

std::vector<WeatherStation> readStationsFromFile(const std::string& filename) {
    std::vector<WeatherStation> stations;
    std::ifstream infile(filename);
    std::string line;
    while (std::getline(infile, line)) {
        std::string id = line;
        double temp;
        if (std::getline(infile, line)) {
            temp = std::stod(line);
            stations.emplace_back(id, temp);
        }
    }
    return stations;
}

int main(int argc, char* argv[]) {
    std::srand(std::time(0));
    if (argc != 2) {
        std::cerr << "Usage: create_measurements <number of records to create>" << std::endl;
        return 1;
    }

    int size;
    try {
        size = std::stoi(argv[1]);
    } catch (const std::invalid_argument&) {
        std::cerr << "Invalid value for <number of records to create>" << std::endl;
        std::cerr << "Usage: create_measurements <number of records to create>" << std::endl;
        return 1;
    }

    std::vector<WeatherStation> stations = readStationsFromFile("station_temperature.conf");
    if (stations.empty()) {
        std::cerr << "No stations found in station_temperature.conf" << std::endl;
        return 1;
    }

    std::ofstream outfile("measurements.txt");
    if (!outfile) {
        std::cerr << "Could not open measurements.txt for writing" << std::endl;
        return 1;
    }

    auto start = std::chrono::high_resolution_clock::now();
    std::default_random_engine generator;

    for (int i = 0; i < size; ++i) {
        if (i > 0 && i % 50000000 == 0) {
            auto now = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double, std::milli> elapsed = now - start;
            std::cout << "Wrote " << i << " measurements in " << elapsed.count() << " ms" << std::endl;
        }
        WeatherStation& station = stations[rand() % stations.size()];
        outfile
          << std::fixed << std::setprecision(1)
          << station.getId() << ";" << station.measurement(generator) << "\n";
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> totalElapsed = end - start;
    std::cout << "Created file with " << size << " measurements in " << totalElapsed.count() << " ms" << std::endl;

    return 0;
}
