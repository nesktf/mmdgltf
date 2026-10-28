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
./build/mmdgltf-cli models/kyouko_doodle/kyouko_doodle.pmx /tmp/kyouko_cli.glb --verbose
test -s /tmp/kyouko_cli.glb

echo "Converting to glTF + bin..."
./build/mmdgltf-cli models/kyouko_doodle/kyouko_doodle.pmx /tmp/kyouko_cli.gltf --gltf --unlit --scale 0.1 --verbose
test -s /tmp/kyouko_cli.gltf
test -s /tmp/kyouko_cli.bin

echo "Testing CLI bone name flags..."
./build/mmdgltf-cli models/kyouko_doodle/kyouko_doodle.pmx /tmp/kyouko_en.glb --english-bone-names --verbose
test -s /tmp/kyouko_en.glb
./build/mmdgltf-cli models/kyouko_doodle/kyouko_doodle.pmx /tmp/kyouko_jp.glb --bone-names jp --verbose
test -s /tmp/kyouko_jp.glb
./build/mmdgltf-cli models/kyouko_doodle/kyouko_doodle.pmx /tmp/kyouko_en2.glb --bone-names en --verbose
test -s /tmp/kyouko_en2.glb

echo "Testing PMD CLI conversions..."
./build/mmdgltf-cli models/default/Miku_Hatsune.pmd /tmp/miku_cli.glb --verbose
test -s /tmp/miku_cli.glb
./build/mmdgltf-cli models/default/Miku_Hatsune.pmd /tmp/miku_cli.gltf --gltf --scale 0.1 --verbose
test -s /tmp/miku_cli.gltf
test -s /tmp/miku_cli.bin

./build/mmdgltf-cli models/default/Dammy_Bone.pmd /tmp/dammy_cli.glb --verbose
test -s /tmp/dammy_cli.glb

./build/mmdgltf-cli models/default/Luka_Megurine.pmd /tmp/luka_cli.glb --verbose
test -s /tmp/luka_cli.glb

./build/mmdgltf-cli models/default/Miku_Hatsune_metal.pmd /tmp/metal_cli.glb --verbose
test -s /tmp/metal_cli.glb

./build/mmdgltf-cli models/default/Haku_Yowane.pmd /tmp/haku_cli.glb
test -s /tmp/haku_cli.glb

./build/mmdgltf-cli models/default/KAITO.pmd /tmp/kaito_cli.glb
test -s /tmp/kaito_cli.glb

./build/mmdgltf-cli models/default/Len_Kagamine.pmd /tmp/len_cli.glb
test -s /tmp/len_cli.glb

./build/mmdgltf-cli models/default/MEIKO.pmd /tmp/meiko_cli.glb
test -s /tmp/meiko_cli.glb

./build/mmdgltf-cli models/default/Neru_Akita.pmd /tmp/neru_cli.glb
test -s /tmp/neru_cli.glb

./build/mmdgltf-cli models/default/Rin_Kagamine.pmd /tmp/rin_cli.glb
test -s /tmp/rin_cli.glb

rm -f /tmp/kyouko_cli.glb /tmp/kyouko_cli.gltf /tmp/kyouko_cli.bin /tmp/kyouko_en.glb /tmp/kyouko_jp.glb /tmp/kyouko_en2.glb
rm -f /tmp/miku_cli.glb /tmp/miku_cli.gltf /tmp/miku_cli.bin /tmp/dammy_cli.glb /tmp/luka_cli.glb /tmp/metal_cli.glb
rm -f /tmp/haku_cli.glb /tmp/kaito_cli.glb /tmp/len_cli.glb /tmp/meiko_cli.glb /tmp/neru_cli.glb /tmp/rin_cli.glb
echo "All CLI tests passed successfully!"