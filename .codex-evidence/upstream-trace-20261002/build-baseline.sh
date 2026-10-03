#!/usr/bin/env bash
set -euo pipefail

# Run on the Deck as a transient user service. Preserve the installed AppImage.
source_root=/home/deck/.local/share/cemu-native-main-build/upstream-0a2b6ff
configure_unit=swapforce-upstream-configure4-20261002.service
while [[ $(systemctl --user show "$configure_unit" -p ActiveState --value) == active ]]; do
  sleep 5
done
[[ $(systemctl --user show "$configure_unit" -p ExecMainStatus --value) == 0 ]]
[[ $(git -C "$source_root" rev-parse HEAD) == 0a2b6ff7db61871b0dd028ac47dc72eda94351a1 ]]
git -C "$source_root" diff --exit-code
git -C "$source_root" diff --cached --exit-code
mkdir -p "$source_root/evidence"
date -Is > "$source_root/evidence/build-start-time.txt"

# CMake 3.22 does not set LINUX automatically. Set its platform flag explicitly
# so Common/ExceptionHandler/ELFSymbolTable.cpp is included without source edits.
podman run --rm --name swapforce-upstream-baseline-build-20261002 \
  --cpus=4 --memory=6G \
  --volume /home/deck/.local/share/cemu-native-main-build:/home/deck/.local/share/cemu-native-main-build:ro \
  --volume "$source_root:/work:rw" --workdir /work \
  --env CC=/usr/bin/clang-15 --env CXX=/usr/bin/clang++-15 \
  --env VCPKG_MAX_CONCURRENCY=4 localhost/cemu-build-env:v2.6 \
  bash -lc 'set -e; cmake -S . -B build-main -DCMAKE_BUILD_TYPE=release -DCMAKE_C_COMPILER=/usr/bin/clang-15 -DCMAKE_CXX_COMPILER=/usr/bin/clang++-15 -G Ninja -DCMAKE_MAKE_PROGRAM=/usr/bin/ninja -DLINUX=ON; cmake --build build-main --parallel 4' \
  > "$source_root/evidence/build.log" 2>&1
sha256sum "$source_root/bin/Cemu_release" > "$source_root/evidence/baseline-binary.sha256"
git -C "$source_root" diff --exit-code
date -Is > "$source_root/evidence/build-end-time.txt"
