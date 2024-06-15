CXX = g++
CXXFLAGS = -std=c++20 -O2

# Target and source files
TARGETS = calc_v1 calc_baseline calc_v2 calc_v3
SOURCES = calculate_average_v1.cc calculate_average_baseline.cc calculate_average_v2.cc calculate_average_v3.cc

# Pattern rule to compile each source file
%: %.cc
	$(CXX) $(CXXFLAGS) -o $@ $<

# All target
all: $(TARGETS)

# Individual targets
calc_v1: calculate_average_v1.cc
	$(CXX) $(CXXFLAGS) -o $@ $<

calc_baseline: calculate_average_baseline.cc
	$(CXX) $(CXXFLAGS) -o $@ $<

calc_v2: calculate_average_v2.cc
	$(CXX) $(CXXFLAGS) -o $@ $<

calc_v3: calculate_average_v3.cc
	$(CXX) $(CXXFLAGS) -o $@ $<

# Clean target
clean:
	rm -f $(TARGETS)
