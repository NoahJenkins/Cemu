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

## CPU mode A/B
- `--force-interpreter`: too slow; never reached FMV in test window
- `cpuMode = 1` (Singlecore-Recompiler) via game profile: **still banded**
- Not a multicore race

## Implication
Corruption is produced by in-title Bink under PPC (even singlecore recompiler), already present in guest R8 planes before GPU upload.

## Evidence files
- `baseline-banded.png` — Activision intro on Cemu 2.6 (Vulkan)
- `display-still-banded.png` — still banded after R8 force-reload experiment
- `binktrace-display-t30.png` — display during Bink-trace run
- `singlecore-still-banded.png` — still banded under singlecore recompiler
- `yplane-guest-autocontrast.png` / `yplane-late-guest.png` — guest Y plane dumps
- `uplane-late-guest.png` — guest U plane dump (autocontrasted)
