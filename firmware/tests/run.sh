#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
bin=$(mktemp)
trap 'rm -f "$bin"' EXIT
g++ -DUSE_API -std=c++17 -Wall -Wextra -I tests/stubs -I MACS/components/macs_hob tests/test_control.cpp MACS/components/macs_hob/macs_hob.cpp -o "$bin"
"$bin"
node --check MACS/dashboard.js
