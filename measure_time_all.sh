#!/bin/bash

if [ $# -ne 1 ]; then
    echo "Usage: $0 <input_file>"
    exit 1
fi

input_file=$1
targets=("calc_v1" "calc_baseline" "calc_v2" "calc_v3")

# Compile all targets
echo "Compiling all targets..."
make all

for target in "${targets[@]}"; do
    if [ ! -f "$target" ]; then
        echo "Executable $target not found. Please compile it first."
        exit 1
    fi
done

# Run the baseline target first and save its output
echo "Running calc_baseline with input file $input_file..."
baseline_output=$(./calc_baseline $input_file)

# Save the wallclock time of the baseline
baseline_wallclock_time=$( (time ./calc_baseline $input_file) 2>&1 | grep real | awk '{print $2}')
echo "calc_baseline $baseline_wallclock_time"

for target in "${targets[@]}"; do
    if [ "$target" == "calc_baseline" ]; then
        continue
    fi

    echo "Running $target with input file $input_file..."
    output=$(./$target $input_file)
    wallclock_time=$( (time ./$target $input_file) 2>&1 | grep real | awk '{print $2}')
    echo "$target $wallclock_time"

    # Compare output with the baseline
    if [ "$output" != "$baseline_output" ]; then
        echo "Error: Output of $target is not identical to the baseline."
    fi
done
