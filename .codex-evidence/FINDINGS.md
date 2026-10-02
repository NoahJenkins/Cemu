# SWAP Force movie corruption investigation

## Gameplay and crash follow-up — 2026-10-02

After the controlled tests, the user reported completing the first level without any issues using the installed workaround. This is user-reported full-level validation; the automated tests below cover only early gameplay. It does not establish that the multicore or display-session defects are fixed.

The user reported a gaming-session restart after selecting Dark Stealth Elf in the Load dialog. The journal confirms an Xwayland segmentation fault followed by Gamescope aborting; the OS did not reboot. Cemu had loaded the single-core profile. Subsequent isolated Desktop Mode and Game Mode tests both passed figure loading, early gameplay, and removal/reload with copied data. The original crash was not reproduced and its trigger remains unresolved. See `crash-20261002/README.md` for evidence, test limits, and final device state.

## Verified status — 2026-10-01

A per-game single-core recompiler workaround is verified and installed on the Deck. The underlying multicore defect is not fixed. The reproducible failure is a **moving part of the Vicarious Visions startup movie**, where red/green copies of the scene separate from the visible objects. Held Activision and Vicarious Visions logos can look clean in a failing run.

| Run | Result in moving scene | Saved frame |
| --- | --- | --- |
| Installed Cemu 2.6, recompiler | Color ghosts and horizontal breaks | `retest-20261001/installed-2.6-corrupt.png` (20 seconds) |
| Main `8cf3997`, recompiler | Same failure | `retest-20261001/main-corrupt.png` (25 seconds) |
| Main, whole-function PSQ fallback | Same failure; fallback log confirmed | `retest-20261001/main-psq-skip-corrupt.png` (25 seconds) |
| Main, direct-float-copy optimizer disabled | Same failure | `retest-20261001/main-no-floatcopy-corrupt.png` (25 seconds) |
| Main, `--force-interpreter` | Inspected moving frames are clean | `retest-20261001/main-interpreter-clean.png` (92 seconds) |
| Main, all-FPU whole-function fallback | Color ghosts absent in sampled moving frames; top horizontal breaks remain | `retest-20261001/main-fpu-skip.png` (28.5 seconds) |
| Main, single-core recompiler | Color ghosts absent in sampled moving frames | `retest-20261001/main-singlecore.png` (29 seconds) |
| Main, multicore with GX2DrawDone full sync enabled | Color ghosts remain | `retest-20261001/main-fullsync-corrupt.png` (28 seconds) |
| Main, forced refresh of large R8 movie textures | Color ghosts remain; upload probe log confirmed | `retest-20261001/main-r8-reload-corrupt.png` (27 seconds) |
| Main, multicore interpreter | Sampled moving frames are clean | `retest-20261001/main-multicore-interpreter.png` (67 seconds) |
| Installed Cemu 2.6, single-core recompiler | Moving startup scene is clean | `retest-20261001/installed-2.6-singlecore.png` (20.5 seconds) |
| Installed Cemu 2.6, copied save and single-core recompiler | Story sequence reaches the portal prompt; sampled moving frames are clean | `retest-20261001/singlecore-story.mp4` |
| Installed Cemu 2.6, final profile copied from live config | Log confirms `CPU-Mode: 1 (gameprofile)`; sampled moving scene has no prominent color ghosts | `retest-20261001/final-profile-moving.png` (18.5 seconds) |

These are different capture times in the same animation, not pixel-identical game frames. Interpreter mode and FPU fallback also change timing. Single-core recompiler removes the color ghosts without changing generated instruction semantics, so the current evidence points toward timing or concurrency. It does not isolate a floating point instruction bug. `GX2DrawDone` already forces full sync for Vulkan in the source, which explains why its config switch did not help.

Recordings are in `/home/deck/CemuSwapForceTest/<variant>/evidence/intro.mp4` on the Deck. Variants are `installed-2.6-video-20261001`, `main-baseline-video-20261001`, `main-skip-video-20261001`, `main-no-floatcopy-20261001`, and `main-interpreter-video-20261001`. Recompiler recordings are 42 seconds at 12 FPS. The interpreter recording is 139.5 seconds at 4 FPS. `record-deck-intro.sh` records the actual Xwayland Cemu window using isolated settings and MLC copies.

The visible symptom resembles the movie attached to [Cemu issue #1147](https://github.com/cemu-project/Cemu/issues/1147). That issue also reports icon corruption. The copied-save test still showed invisible save-slot icons.

## Correction to the earlier evidence

The earlier filenames and conclusions below are not reliable proof of the fault location. `baseline-banded.png`, `display-still-banded.png`, and `singlecore-still-banded.png` show the title screen. `interpreter-activision-clean.png` shows a loading screen. `poisonfloat-psq-destroys-fmv.png` shows the desktop. The PSQ fallback does **not** fix the newly captured moving scene on main.

The old notes remain in Git history at `aba43cc`. Recheck their claims with matching moving scenes before using them. In particular, the prior claims that PSQ or FPU code generation was isolated are superseded. The old single-core conclusion is also contradicted by the fresh recordings.

## Delivered workaround

- Repository profile: `bin/gameProfiles/default/0005000010139200.ini`.
- Installed profile: `/home/deck/.config/Cemu/gameProfiles/0005000010139200.ini`.
- Setting: `[CPU]` with `cpuMode = 1` (single-core recompiler), scoped to the tested USA title `00050000-10139200`.
- The installed Cemu 2.6 ignored the text `Singlecore-Recompiler` in the first final check. The numeric form was then loaded successfully. Use the numeric form for this installation.
- No profile existed at the live path before this change. Rollback is removal of this one new file.
- The test and live profile hashes matched after installation. Test launches used isolated MLC paths. The gameplay test used a copy of the user's save and a keyboard controller only inside the isolated test config.
- Final verification: local and live profile SHA-256 is `db8f88edb400fb93ac73753cd36c83bd3f8af9b5f4b1989e6dcc211b665c7bc0`. The Deck source diff exactly matches its saved original patch. No Cemu or capture process remained running. Shell syntax checks and `git diff --check` passed.

## Verification and limits

`retest-20261001/multicore-before.mp4` and `singlecore-after.mp4` show the moving startup scene on the same installed Cemu 2.6 binary. `singlecore-story.mp4` shows part of the subsequent story sequence.

The story test reached the prompt to place a Skylander on the portal. Ten sampled window-title rates over about 20 seconds were 27.44–30.36 FPS, with most near 30.05 FPS. These samples cover the end of the story sequence and the portal prompt; they are not a gameplay benchmark. Gameplay beyond the portal prompt was not verified because no Skylander was loaded.

The save-slot icons remain invisible. Small horizontal breaks can appear near the top in the captured startup frames, including interpreter and FPU fallback runs. The workaround is verified for the prominent color ghosts; it is not a claim that all visual defects are fixed.

The source builds used main commit `8cf3997`, not the latest upstream commit. Diagnostic builds completed after a local CMake 3.22 compatibility adjustment (`CMAKE_SYSTEM_NAME` instead of `LINUX`). That build adjustment and the diagnostic changes were removed from the Deck source tree after testing, restoring its original three-file PSQ diagnostic patch. Test binaries and recordings were retained. The installed Cemu executable was not replaced.

## Remaining source investigation

Single-core recompilation and both interpreter modes remove the color ghosts in the sampled scene. Whole-function PSQ fallback, disabling direct float copies, full GPU sync, and forced R8 reload do not. All-FPU fallback also removes the ghosts but changes timing. These observations do not justify a PSQ code-generation patch.

The next source investigation should trace when the guest finishes each movie plane relative to draw submission under multicore execution. A fresh matched reference is required before treating any guest-memory dump as corrupt. There is also a suspicious hash/decode ordering in `LatteTextureLoader_UpdateTextureSliceData`, but the failed forced-reload probe gives no basis to claim that changing it fixes this issue.

For future visual tests, record the full moving sequence and identify the frame by its contents. A fixed delay or a clean held logo is insufficient evidence.
