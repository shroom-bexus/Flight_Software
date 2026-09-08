#!/usr/bin/env bash
set -euo pipefail
# LeakSanitizer cannot inspect processes in some containers.
export ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=0}"
cd "$(dirname "$0")/../.."
test_dir=$(mktemp -d)
trap 'rm -rf "$test_dir"' EXIT
for primary in 0 1; do
    g++ -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined \
        -DFLIGHT_PRIMARY="$primary" -DFLIGHT_SECONDARY="$((1-primary))" \
        -Itest/host -Iinclude src/teensy_link.cpp src/airdos.cpp \
        test/host/test_link.cpp -o "$test_dir/test-$primary"
    "$test_dir/test-$primary"
done
