#!/usr/bin/env bash
set -euo pipefail

# Run as a transient user service with fresh graphical-session environment.
# Prepare independent XDG, save, and figure copies before calling this script.
test_root=${1:?isolated test root required}
binary=${2:?baseline binary required}
cpu_mode=${3:?numeric CPU mode required}
duration=${4:-50}
frame_rate=${5:-12}
engine=${6:-normal}
extra_args=()
case "$engine" in
  normal) ;;
  multicore-interpreter)
    [[ "$cpu_mode" == 3 ]]
    extra_args+=(--force-multicore-interpreter)
    ;;
  *) echo 'Unknown test engine' >&2; exit 1 ;;
esac
: "${DISPLAY:?fresh DISPLAY required}"
: "${XDG_RUNTIME_DIR:?graphical runtime directory required}"

case "$test_root" in
  /home/deck/CemuSwapForceTest/upstream-0a2b6ff-*) ;;
  *) echo 'Refusing a test root outside this investigation' >&2; exit 1 ;;
esac
case "$cpu_mode" in
  1|3) ;;
  *) echo 'Use numeric CPU mode 1 or 3' >&2; exit 1 ;;
esac
if [[ -n "${XAUTHORITY:-}" && ! -f "$XAUTHORITY" ]]; then
  echo 'The supplied XAUTHORITY file is unavailable' >&2
  exit 1
fi
[[ -x "$binary" && -d "$test_root/mlc01" ]]
[[ -d "$test_root/xdg/config/Cemu" && -d "$test_root/xdg/data/Cemu" ]]
[[ -f "$test_root/figures/Dark Stealth Elf.sky" ]]
if pgrep -x cemu >/dev/null || pgrep -f '^/home/deck/.*[Cc]emu[^ ]*( |$)' >/dev/null; then
  echo 'A Cemu session is already running; preserve it' >&2
  exit 1
fi

export XDG_CONFIG_HOME="$test_root/xdg/config"
export XDG_DATA_HOME="$test_root/xdg/data"
export XDG_CACHE_HOME="$test_root/xdg/cache"
mkdir -p "$XDG_CACHE_HOME" "$test_root/evidence" "$XDG_CONFIG_HOME/Cemu/gameProfiles"
[[ ! -e "$test_root/evidence/intro.mp4" ]]
printf '[CPU]\ncpuMode = %s\n' "$cpu_mode" > "$XDG_CONFIG_HOME/Cemu/gameProfiles/0005000010139200.ini"
rom='/home/deck/Emulation/roms/wiiu/Skylanders - Swap Force (USA) (EnFrEsPt)/Skylanders - Swap Force (USA) (En,Fr,Es,Pt).wux'
[[ -f "$rom" ]]
sha256sum "$binary" "$XDG_CONFIG_HOME/Cemu/gameProfiles/0005000010139200.ini" > "$test_root/evidence/run-hashes.txt"
date -Is > "$test_root/evidence/start-time.txt"
"$binary" "${extra_args[@]}" --mlc "$test_root/mlc01" --game "$rom" > "$test_root/evidence/console.log" 2>&1 &
cemu_pid=$!
cleanup() {
  kill "$cemu_pid" 2>/dev/null || true
  wait "$cemu_pid" 2>/dev/null || true
}
trap cleanup EXIT

window_id=
for ((second=0; second<30; second++)); do
  kill -0 "$cemu_pid" 2>/dev/null || { echo 'Cemu exited before capture' >&2; exit 1; }
  while read -r candidate; do
    title=$(xdotool getwindowname "$candidate" 2>/dev/null || true)
    if [[ "$title" == *TitleId:* ]]; then
      window_id=$candidate
      printf '%s\n' "$title" > "$test_root/evidence/window-title.txt"
      break
    fi
  done < <(xdotool search --onlyvisible --pid "$cemu_pid" 2>/dev/null || true)
  [[ -z "$window_id" ]] || break
  sleep 1
done
[[ -n "$window_id" ]] || { echo 'No game window owned by the test PID' >&2; exit 1; }
printf 'pid=%s window=%s display=%s cpu_profile=%s\n' "$cemu_pid" "$window_id" "$DISPLAY" "$cpu_mode" > "$test_root/evidence/run.txt"
ffmpeg -hide_banner -loglevel error -framerate "$frame_rate" -f x11grab -draw_mouse 0 \
  -window_id "$window_id" -i "$DISPLAY" -t "$duration" -an \
  -c:v libx264 -preset ultrafast -crf 22 "$test_root/evidence/intro.mp4"
ffprobe -v error -show_entries format=duration,size -of default=noprint_wrappers=1 \
  "$test_root/evidence/intro.mp4" > "$test_root/evidence/media-summary.txt"
cp "$XDG_DATA_HOME/Cemu/log.txt" "$test_root/evidence/cemu-log.txt"
rg 'CPU-Mode|Run title|TitleId' "$test_root/evidence/cemu-log.txt"
date -Is > "$test_root/evidence/end-time.txt"
