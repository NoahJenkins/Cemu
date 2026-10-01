#!/usr/bin/env bash
set -euo pipefail

# Run on the Steam Deck. Keep each variant's settings and saves isolated.
variant=${1:?variant name required}
binary=${2:?Cemu binary required}
source /home/deck/.local/share/cemu-swapforce/plasma.env
if [[ -n "${XAUTHORITY:-}" && ! -e "$XAUTHORITY" ]]; then
  unset XAUTHORITY
fi

test_root="/home/deck/CemuSwapForceTest/$variant"
baseline=/home/deck/CemuSwapForceTest/main-5ead580
rom='/home/deck/Emulation/roms/wiiu/Skylanders - Swap Force (USA) (EnFrEsPt)/Skylanders - Swap Force (USA) (En,Fr,Es,Pt).wux'
mkdir -p "$test_root/xdg/config" "$test_root/xdg/data" "$test_root/xdg/cache" "$test_root/evidence/shots"
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
: > "$XDG_DATA_HOME/Cemu/log.txt"

"$binary" --mlc "$test_root/mlc01" --game "$rom" > "$test_root/evidence/console.log" 2>&1 &
cemu_pid=$!
echo "Cemu PID: $cemu_pid"
trap 'kill "$cemu_pid" 2>/dev/null || true' EXIT

for second in $(seq 0 60); do
  if ! kill -0 "$cemu_pid" 2>/dev/null; then
    echo "Cemu exited at ${second}s"
    break
  fi
  if (( second % 2 == 0 )); then
    shot="$test_root/evidence/shots/$(printf '%03d' "$second").png"
    window_id=$(DISPLAY=:0 xwininfo -root -tree 2>/dev/null | awk '/"Cemu .*TitleId:/{print $1; exit}')
    if [[ -n "$window_id" ]]; then
      timeout 3 ffmpeg -hide_banner -loglevel error -f x11grab -window_id "$window_id" -i :0 -frames:v 1 -y "$shot" >/dev/null 2>&1 || true
    fi
    if [[ -s "$shot" ]]; then
      echo "Captured ${second}s"
    fi
  fi
  sleep 1
done

rg -n 'CPU-Mode|PPCRecSkipPSQ|Run title' "$XDG_DATA_HOME/Cemu/log.txt" | head -40 || true
