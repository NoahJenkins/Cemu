#!/usr/bin/env bash
set -euo pipefail
source_root=/home/deck/.local/share/cemu-native-main-build/upstream-0a2b6ff
[[ $(git -C "$source_root" rev-parse HEAD) == 0a2b6ff7db61871b0dd028ac47dc72eda94351a1 ]]
[[ ! -e "$source_root/bin/Cemu_command_nested_0a2b6ff" ]]
podman run --rm --name swapforce-command-nested-build-20261002 --cpus=4 --memory=6G \
  --volume /home/deck/.local/share/cemu-native-main-build:/home/deck/.local/share/cemu-native-main-build:ro \
  --volume "$source_root:/work:rw" --workdir /work \
  localhost/cemu-build-env:v2.6 cmake --build build-main --parallel 4 \
  > "$source_root/evidence/command-nested-build.log" 2>&1
cp "$source_root/bin/Cemu_release" "$source_root/bin/Cemu_command_nested_0a2b6ff"
sha256sum "$source_root/bin/Cemu_command_nested_0a2b6ff" > "$source_root/evidence/command-nested-binary.sha256"
