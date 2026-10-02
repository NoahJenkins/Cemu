# Handoff: SWAP Force multicore video corruption

## Objective

Investigate the underlying multicore video corruption in Cemu. First reproduce it on current upstream. The first milestone is to identify where frame data becomes corrupt or inconsistent, then test a correction supported by that evidence. Preserve the working single-core setup throughout.

## Repository and starting point

- Local repository: `/Users/noahjenkins/Code/cemu-swapforce-fix`.
- Evidence branch: `codex/swap-force-cutscene-corruption`.
- Fork: `https://github.com/NoahJenkins/Cemu` (`origin`).
- Upstream: `https://github.com/cemu-project/Cemu` (`upstream`).
- Workaround commit: `b92d27c`. Read the latest branch head for subsequent test evidence and this handoff.
- Last fetched upstream was `0a2b6ff` (October 1, 2026). Fetch again before selecting the test revision.
- Use a separate worktree based on current upstream for source experiments. Keep the evidence branch intact. It contains historical diagnostics and media that do not belong in an upstream code PR.

Read `.codex-evidence/FINDINGS.md` and `.codex-evidence/crash-20261002/README.md` first. Older claims before the October 1 corrections are unreliable. The named source build used for comparisons was `8cf3997`, not current upstream.

## Working workaround and evidence

- Title: Skylanders SWAP Force USA, `00050000-10139200`.
- Profile: `bin/gameProfiles/default/0005000010139200.ini` sets `[CPU] cpuMode = 1`.
- Live Deck profile: `/home/deck/.config/Cemu/gameProfiles/0005000010139200.ini`.
- Installed executable: `/home/deck/Applications/Cemu.AppImage`, Cemu 2.6 with a custom F9 portal shortcut. Do not replace it for experiments.
- The installed build ignored the text form `Singlecore-Recompiler`; numeric `1` was verified in its log as `CPU-Mode: 1 (gameprofile)`.
- The prominent failure is red/green color-plane separation during the moving Vicarious Visions startup movie. Held logos can look clean in a failing run. Compare matching scene content, not elapsed time alone.
- Multicore recompilation failed on installed 2.6 and the tested main build. Single-core recompilation, single-core interpretation, and multicore interpretation removed the prominent color ghosts in sampled moving scenes.
- Whole-function PSQ fallback, disabling direct float-copy optimization, full GPU sync, and forced large-R8 texture reload did not fix it. All-FPU fallback removed the ghosts but changed timing. This does not isolate an FPU instruction bug.
- Small horizontal breaks and invisible save-slot icons remain separate limitations.
- Controlled Desktop Mode and Game Mode tests loaded Dark Stealth Elf, entered early gameplay, moved the character, and removed/reloaded the figure successfully. Sampled rates were near 30 FPS, not a benchmark.
- The user subsequently reported completing the first level without issues. Label this as user-reported evidence.

Before/after clips and frames are under `.codex-evidence/retest-20261001/`. Figure-load and gameplay results are under `.codex-evidence/crash-20261002/`. The full recordings remain under `/home/deck/CemuSwapForceTest/` on the Deck.

## Investigation direction

1. Build current upstream in isolation. Establish matched multicore and single-core reference recordings before instrumenting anything.
2. Trace completion of guest writes to movie planes relative to texture decoding and draw submission. Determine whether inconsistency exists in guest memory or appears during host upload/rendering.
3. Change one variable at a time and require the same moving-scene reproduction before claiming a fix.
4. Keep instruction correctness, thread ordering, frame-buffer reuse, and texture-cache behavior as hypotheses until evidence separates them.

`LatteTextureLoader_UpdateTextureSliceData` has suspicious hash/decode ordering, but the failed forced-reload probe does not justify a fix there. Recompiler SYNC/ISYNC handling is another observation, not a proven cause. Do not repeat broad speculative patches without new evidence.

## Deck access and test boundaries

- SSH alias: `ssh steamdeck`. The user explicitly authorized SSH and needed tests, including mode changes. Recheck the current running apps before disrupting a new session.
- Use isolated XDG config/data/cache, copied MLC saves, and copied figure files for every diagnostic launch.
- Live save: `/home/deck/Emulation/roms/wiiu/mlc01/usr/save/00050000/10139200`.
- Figure: `/run/media/deck/sdCardOnDeck/Emulation/saves/skylanders-figures/Dark Stealth Elf.sky`.
- The owned game path is in `.codex-evidence/record-deck-intro.sh`. Do not commit ROMs, figure dumps, keys, or full core dumps.
- Existing test roots: `desktop-portal-20261002`, `gamemode-portal-20261002`, and `installed-singlecore-gameplay-20261001` under `/home/deck/CemuSwapForceTest/`.
- The original figure SHA-256 at test completion was `0c420adfedb92ca5b793179cc7c309bb4aa1eb634226347d178f6e68fc95e8ea`; the live profile hash was `db8f88edb400fb93ac73753cd36c83bd3f8af9b5f4b1989e6dcc211b665c7bc0`. Recheck; the user may legitimately change figure progress later.
- Tests ended with no test Cemu/capture services running and the Deck back in Game Mode. Verify current state before new tests.

## Operational notes

- Mode commands used successfully: `steamosctl switch-to-desktop-mode plasmax11.desktop` and `steamosctl switch-to-game-mode`.
- Discover fresh display/auth values after each switch. Do not reuse stale XAUTHORITY from `plasma.env`.
- Launch long-lived SSH tests with transient `systemd-run --user` services and explicit environment. Bare detached subprocess launches exited with their SSH session during Desktop Mode testing.
- Desktop capture worked with `ffmpeg -f x11grab -i :0`. Game Mode required `-window_id` for the observed Cemu window. Re-observe IDs before actions.
- Game Mode test ran on Xwayland `:1`. SSH-launched windows may need Gamescope focus handling; plain `xdotool windowfocus` did not route compositor keyboard input. AT-SPI successfully operated the native portal/file-picker controls. Details and equivalence limits are in the crash README.
- Isolated keyboard profile: `x` is gamepad A, Return is Plus, WASD is movement. `xdotool key --delay 150` avoids pulses missed by the game.
- Stop capture and wait for a valid MP4 before closing Cemu. Verify all transient test services are gone afterward.
- Deck diagnostic source: `/home/deck/.local/share/cemu-native-main-build/source`. It contains a pre-existing three-file PSQ diagnostic patch; the original diff is saved at `.codex-evidence-main-psq-skip.patch` there. Preserve that state and use a separate checkout for new work.
- Previous container build used `localhost/cemu-build-env:v2.6`, clang-15, and `cmake --build build-main --parallel 4`. A CMake 3.22 compatibility change from `if(LINUX)` to `if(CMAKE_SYSTEM_NAME STREQUAL "Linux")` was needed for those builds. Reassess on current upstream; it was removed after the tests.

## Separate display-session incident

On October 2, selecting a figure in Game Mode caused Xwayland `:1` to segfault, then Gamescope aborted. The OS did not reboot. Neither subsequent controlled test reproduced the crash. Desktop Mode is a tested fallback; the exact trigger remains unresolved. Do not conflate this display-session incident with the multicore movie defect.

## Upstream contribution boundary

Issue `https://github.com/cemu-project/Cemu/issues/1147` was open at the last check. A workaround PR should address only the supported video symptom and tested USA title; do not claim all icon corruption is fixed.

Read current `CONTRIBUTING.md` before any submission. At the last check, Cemu required human-written and human-understood code from new contributors, with AI allowed for planning/review. Do not submit AI-authored code as human-written or evade that rule by paraphrasing. Clarify whether an AI-prepared configuration-only proposal is accepted. No upstream PR has been created. Submission remains a separate step from this investigation.
