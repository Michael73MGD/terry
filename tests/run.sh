#!/bin/sh
set -eu
repo_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
json_dir=${ARDUINOJSON_DIR:-"$HOME/Documents/Arduino/libraries/ArduinoJson/src"}
test_dir=$(mktemp -d)
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -fno-omit-frame-pointer -I "$json_dir" -I "$repo_dir" \
  "$repo_dir/tests/data_test.cpp" -o "$test_dir/data_test"
"$test_dir/data_test"
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -fno-omit-frame-pointer -I "$json_dir" -I "$repo_dir/tests/fakes" -I "$repo_dir" \
  "$repo_dir/tests/firmware_test.cpp" -o "$test_dir/firmware_test"
"$test_dir/firmware_test"
"$test_dir/firmware_test" boot-recovery
"$test_dir/firmware_test" offline-boot
"$test_dir/firmware_test" wifi-first
"$test_dir/firmware_test" settings-first
"$test_dir/firmware_test" startup-success
"$test_dir/firmware_test" startup-failure
"$test_dir/firmware_test" startup-timeout
