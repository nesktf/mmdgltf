#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${ROOT_DIR}"

cmake -B build -DMMDGLTF_BUILD_TESTS=ON -DMMDGLTF_BUILD_CLI=ON
cmake --build build --target mmdgltf_test mmdgltf_cpp_test mmdgltf-cli
./build/test/mmdgltf_test "$@"
./build/test/mmdgltf_cpp_test

echo "Testing mmdgltf-cli..."
./build/mmdgltf-cli --help > /dev/null
./build/mmdgltf-cli --version > /dev/null

echo "Converting to GLB..."
./build/mmdgltf-cli models/nitori/河城にとり.pmx /tmp/nitori_cli.glb --verbose
test -s /tmp/nitori_cli.glb

echo "Converting to glTF + bin..."
./build/mmdgltf-cli models/nitori/河城にとり.pmx /tmp/nitori_cli.gltf --gltf --unlit --scale 0.1 --verbose
test -s /tmp/nitori_cli.gltf
test -s /tmp/nitori_cli.bin

echo "Testing CLI bone name flags..."
./build/mmdgltf-cli models/nitori/河城にとり.pmx /tmp/nitori_en.glb --english-bone-names --verbose
test -s /tmp/nitori_en.glb
./build/mmdgltf-cli models/nitori/河城にとり.pmx /tmp/nitori_jp.glb --bone-names jp --verbose
test -s /tmp/nitori_jp.glb
./build/mmdgltf-cli models/nitori/河城にとり.pmx /tmp/nitori_en2.glb --bone-names en --verbose
test -s /tmp/nitori_en2.glb

rm -f /tmp/nitori_cli.glb /tmp/nitori_cli.gltf /tmp/nitori_cli.bin /tmp/nitori_en.glb /tmp/nitori_jp.glb /tmp/nitori_en2.glb
echo "All CLI tests passed successfully!"
