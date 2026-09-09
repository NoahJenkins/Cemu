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
