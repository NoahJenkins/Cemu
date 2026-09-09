# SWAP Force Activision FMV findings (in progress)

## Not H.264
- No H264DEC* calls during Activision intro
- No cafeLibs/h264.rpl
- Disc has Bink/VP6 markers (BIKb, vp6F, On2)

## Presentation path
- Three linear R8 planes: 2048x1024 (Y) + 1024x512 (U) + 1024x512 (V)
- guestPitch == addrPitch (2048 / 1024)
- No GX2CopySurface / DMAE large copies during FMV → CPU stores from in-game Bink
- PR #2051, pitch hacks, force-reload R8: still banded

## Guest memory already corrupt
- Dumped Y/U/V from phys RAM before GPU upload
- Y shows Activision logo shape with horizontal banding (same class of artifact as display)
- Pitch reinterpret tests: only pitch=2048 yields recognizable logo → stride metadata OK; decode content wrong

## Bink write path
- No SFCopyLog / SFDMAELog during Activision → pure CPU stores from in-game Bink
- Late dumps: Y phys=0x1fe28c00, U=0x200a8e00, V=0x1fd28a00
- Y dump at pitch 2048 shows recognizable Activision with horizontal striping (not a pitch-misread)
- Alternate pitch reinterpret (1280/1920/etc.) does not yield a clean logo

## CPU mode A/B (decisive)
- Multicore recompiler: banded
- Singlecore recompiler (`cpuMode = 1`): **still banded** → not a multicore race
- `--force-interpreter` on stock Cemu 2.6: Activision logo **clean** (no RGB banding), reproduced twice

## Implication
Corruption is produced by in-title Bink under the **PPC recompiler** (even singlecore). Interpreter is correct. Guest R8 Y/U/V planes are already wrong before GPU upload when using the recompiler.

## Evidence files
- `baseline-banded.png` — Activision intro on Cemu 2.6 recompiler (Vulkan)
- `display-still-banded.png` — still banded after R8 force-reload experiment
- `binktrace-display-t30.png` — display during Bink-trace run
- `singlecore-still-banded.png` — still banded under singlecore recompiler
- `interpreter-activision-clean.png` / `interpreter-activision-clean-2.png` — clean under `--force-interpreter`
- `yplane-guest-autocontrast.png` / `yplane-late-guest.png` — guest Y plane dumps (recompiler)
- `uplane-late-guest.png` — guest U plane dump (autocontrasted)

## Next
Isolate which PPC recompiler codegen path Bink hits (float / paired-single / load-store / cache ops).

## FPU recompiler isolation
- Patched `PPCRecompiler_recompileFunction` to refuse recompiling any function with `hasFPUInstruction` (FPU/PS stay on interpreter; integer stays recompiled)
- Activision intro renders **clean** (`skipfpu-rec-activision-clean.png`)
- Therefore the bug is in **PPC recompiler FPU/paired-single codegen**, not integer recompiler or GPU upload

## PSQ-only recompiler isolation (narrower)
- Marked `hasPSQInstruction` only on primary forms **PSQ_L (op 56)** and **PSQ_ST (op 60)** — not PSQ_LU / PSQ_STU
- Refused recompile when `hasPSQInstruction` (those funcs fall back to interpreter; other FPU/PS still recompiled)
- Log shows `PPCRecSkipPSQ` hits (CPU-Mode 3 / recompiler); Activision intro renders **clean**
  - `skippsq-rec-activision-clean-t30.png` / `skippsq-rec-activision-clean.png` / `skippsq-rec-activision-clean-2.png`
  - raw run: `psq-skip/psq-t{30,40,50}.png` + `psq-skip/cemu-log.txt`
- Contrast: baseline recompiler Activision is heavily RGB-banded (`baseline-banded.png`)
- **Implication:** defect is in **PSQ_L / PSQ_ST recompiler codegen** (or shared helpers those use), not general FPU arithmetic and not PSQ_*U alone (those were still recompiled and FMV stayed clean)

## Main still affected (post scaler fix)
- Upstream `f456235` ("CPU: Fix PSQ_L/PSQ_ST scaler calculation") is in `main` / `5ead580` but **does not** clear SWAP Force Activision banding
- `main-5ead580` AppImage Activision frames match baseline banding metrics (`main-5ead580/main-intro-t35.png`, `main-intro-t40.png`)
- PR #1894 AppImage also still banded (`pr1894-intro-t30.png`) — H.264 path irrelevant here
- Diagnostic tree on Deck is still v2.6 (pre-rework PSQ_GENERIC backend); skip result there proves class, not the final main-line patch site

## PSQ_L-only vs PSQ_ST-only (v2.6 diagnostic)
- **PSQ_L-only skip** (stores still recompiled): Activision at t30/t40 **clean** (`skippsql-rec-activision-clean.png`, `psql-skip/psql-t{30,40}.png`); log `PPCRecSkipPSQL`
- t50 in same run looked banded again (`skippsql-rec-activision-t50-banded.png`) — treat as unresolved anomaly / possible second pass; do not over-claim from one late frame
- **PSQ_ST-only skip**: in progress

## Next
1. Finish **PSQ_ST-only** A/B on v2.6 diagnostic build
2. Reproduce PSQ skip on a **main**-based Deck build (post-`f456235` IML PSQ path)
3. Inspect PSQ_L recompiler path vs interpreter; craft smallest upstream fix
