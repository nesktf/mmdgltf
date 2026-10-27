#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${ROOT_DIR}"

cmake -B build -DMMDGLTF_BUILD_TESTS=ON
cmake --build build --target mmdgltf_test mmdgltf_cpp_test
./build/test/mmdgltf_test "$@"
./build/test/mmdgltf_cpp_test
