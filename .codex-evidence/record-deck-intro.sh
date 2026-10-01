#!/usr/bin/env bash
set -euo pipefail

# Run on the Steam Deck. Record the Cemu window through Xwayland.
variant=${1:?variant name required}
binary=${2:?Cemu binary required}
mode=${3:-recompiler}
duration=${4:-42}
frame_rate=${5:-12}
extra_args=()
if [[ "$mode" == interpreter ]]; then
  extra_args=(--force-interpreter)
elif [[ "$mode" == multicore-interpreter ]]; then
  extra_args=(--force-multicore-interpreter)
elif [[ "$mode" != recompiler && "$mode" != singlecore ]]; then
  echo "Unknown CPU mode: $mode" >&2
  exit 1
fi
source /home/deck/.local/share/cemu-swapforce/plasma.env
if [[ -n "${XAUTHORITY:-}" && ! -e "$XAUTHORITY" ]]; then
  unset XAUTHORITY
fi

test_root="/home/deck/CemuSwapForceTest/$variant"
baseline=/home/deck/CemuSwapForceTest/main-5ead580
rom='/home/deck/Emulation/roms/wiiu/Skylanders - Swap Force (USA) (EnFrEsPt)/Skylanders - Swap Force (USA) (En,Fr,Es,Pt).wux'
mkdir -p "$test_root/xdg/config" "$test_root/xdg/data" "$test_root/xdg/cache" "$test_root/evidence"
if [[ ! -e "$test_root/xdg/config/Cemu" ]]; then
  cp -a "$baseline/xdg/config/Cemu" "$test_root/xdg/config/"
fi
if [[ ! -e "$test_root/xdg/data/Cemu" ]]; then
  cp -a "$baseline/xdg/data/Cemu" "$test_root/xdg/data/"
fi
if [[ ! -e "$test_root/mlc01" ]]; then
  cp -a "$baseline/mlc01" "$test_root/mlc01"
fi

export XDG_CONFIG_HOME="$test_root/xdg/config"
export XDG_DATA_HOME="$test_root/xdg/data"
export XDG_CACHE_HOME="$test_root/xdg/cache"
export XDG_RUNTIME_DIR=/run/user/1000
if [[ "$mode" == singlecore ]]; then
  mkdir -p "$XDG_CONFIG_HOME/Cemu/gameProfiles"
  printf '[CPU]\ncpuMode = 1\n' > "$XDG_CONFIG_HOME/Cemu/gameProfiles/0005000010139200.ini"
fi
: > "$XDG_DATA_HOME/Cemu/log.txt"

"$binary" "${extra_args[@]}" --mlc "$test_root/mlc01" --game "$rom" > "$test_root/evidence/console.log" 2>&1 &
cemu_pid=$!
trap 'kill "$cemu_pid" 2>/dev/null || true' EXIT

window_id=
for second in $(seq 0 15); do
  window_id=$(DISPLAY=:0 xwininfo -root -tree 2>/dev/null | awk '/"Cemu .*TitleId:/{print $1; exit}')
  if [[ -n "$window_id" ]]; then
    break
  fi
  kill -0 "$cemu_pid" 2>/dev/null || { echo 'Cemu exited before window appeared'; exit 1; }
  sleep 1
done
[[ -n "$window_id" ]] || { echo 'Cemu window not found'; exit 1; }
echo "Recording Cemu window $window_id"

DISPLAY=:0 ffmpeg -hide_banner -loglevel error -framerate "$frame_rate" -f x11grab -draw_mouse 0 -window_id "$window_id" -i :0 -t "$duration" -an -c:v libx264 -preset ultrafast -crf 22 -y "$test_root/evidence/intro.mp4"
stat -c 'Recording bytes: %s' "$test_root/evidence/intro.mp4"
rg -n 'CPU-Mode|PPCRecSkipPSQ|Run title' "$XDG_DATA_HOME/Cemu/log.txt" | head -10 || true
