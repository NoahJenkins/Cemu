# Skylander load crash — 2026-10-02

The user selected Dark Stealth Elf in Cemu's Load dialog in Steam Deck Game Mode. The gaming session then restarted. The user confirmed that this happened after file selection, not when opening the file picker.

## Verified evidence

- The OS boot began on September 28 at 21:57:47. It did not reboot during this incident.
- At 12:23:55 CDT, Xwayland PID 1802 reported a segmentation fault at `0x25200000056` and aborted.
- At 12:23:56, Gamescope PID 1672 reported `X11 I/O error! This is fatal. Aborting...` and also aborted. The gaming session restarted afterward.
- The journal lists core dumps for Xwayland and Gamescope. It does not list a Cemu core dump for this incident.
- The inspected incident window contains no GPU reset or out-of-memory kill.
- Cemu 2.6 loaded SWAP Force USA with `CPU-Mode: 1 (gameprofile)`. The Portal Stability Fix graphic pack was active.
- Installed packages: `xorg-xwayland 24.1.10-1.2`, `gamescope 3.16.23.6-1`.
- `/run/media/deck/sdCardOnDeck/Emulation/saves/skylanders-figures/Dark Stealth Elf.sky` is 1024 bytes, with figure ID 26 and variant `0x2c02`. These match the size and character entry in the inspected Cemu source. This is not a full figure integrity check.
- A second Dark Stealth Elf file exists under `Complete Sets/Swap Force/`. The exact selected path has not been confirmed.

## Initial interpretation and test plan

The immediate failure is in Xwayland, followed by Gamescope. The logs do not identify whether file-dialog teardown, the subsequent UI update, or game rendering triggered it. They do not prove that the figure file or single-core workaround caused the crash.

The initial plan was to repeat loading in Desktop Mode with copied save and figure files, keeping Cemu version and CPU mode unchanged. The results of that comparison are recorded below. The incident alone did not justify changing the graphics driver or CPU settings.

The crash excerpts are in `display-crash.txt`; selected Cemu settings are in `cemu-summary.txt`. Full core dumps remain on the Deck and were not exported.

## Desktop Mode test

On October 2, the user authorized SSH control and any needed tests. Switched to the Plasma X11 desktop with `steamosctl switch-to-desktop-mode plasmax11.desktop`.

- Used the installed `/home/deck/Applications/Cemu.AppImage` and the same single-core game profile.
- Used isolated settings, an independent MLC/save copy, and a copy of `Dark Stealth Elf.sky` under `/home/deck/CemuSwapForceTest/desktop-portal-20261002`.
- Loaded the figure through the native file picker. The portal manager displayed `Dark Stealth Elf`.
- Entered the level after the opening story and moved the visible character with keyboard input. The sampled title-bar rates were near 30 FPS; this was a short functional check, not a performance benchmark.
- Cleared the figure during gameplay. The game returned to the portal prompt. Loaded the copied file again and returned to the visible playable character without a crash.
- No new systemd-coredump entry appeared during this test. The live figure hash remained `0c420adfedb92ca5b793179cc7c309bb4aa1eb634226347d178f6e68fc95e8ea`.
- Screenshots are stored beside this note. The full 320-second recording remains on the Deck at `desktop-portal-20261002/evidence/portal-test.mp4`.

The installed build includes an F9 portal shortcut, confirmed in its build source and by opening the portal manager in the live test. F9 is the direct shortcut for this installation.

Desktop Mode passed the figure-load and early-gameplay test. This does not establish a source fix for the original Xwayland failure.

## Game Mode comparison

Returned to Game Mode with `steamosctl switch-to-game-mode`. Repeated the test on Xwayland display `:1`, matching the display used by the process that crashed earlier. Used the same installed Cemu binary and another isolated copy at `/home/deck/CemuSwapForceTest/gamemode-portal-20261002`.

- Loaded Dark Stealth Elf from the native file picker at the title screen. The portal manager showed the character name.
- Entered the saved game, reached the visible character, and moved it.
- Cleared the character and selected the copied figure again during gameplay. The character returned and continued to respond to movement. The final sampled window title reported 30.04 FPS.
- Xwayland PIDs 273152 and 273153 remained running throughout the comparison. No new systemd-coredump entry appeared during either controlled test.
- The full Game Mode gameplay recording is 245 seconds. A short ending clip and the post-reload screenshot are saved with this note.

This comparison used SSH/systemd launch, not the Steam shortcut's complete launch environment. The file picker was operated through AT-SPI actions. To route keyboard input to the test window, its temporary `STEAM_GAME` property was set to the preferred app ID already present in Gamescope's root properties. These differences limit equivalence to the user's original controller-driven session. No permanent launcher or Gamescope configuration was changed.

## Final state and conclusion

Both controlled tests passed figure loading, early gameplay, and removal/reload. The original crash was not reproduced and its exact trigger remains unresolved. Do not label it fixed or infer that Dark Stealth Elf is defective. Desktop Mode is a tested fallback for loading and early gameplay.

All test Cemu and capture services were stopped. The Deck was left in Game Mode with Steam focused. The live single-core profile and original Dark Stealth Elf file retained their pre-test hashes. Gameplay changes were confined to the isolated save and figure copies. The source, installed executable, graphics driver, live controller settings, and launcher were not changed.

After these tests, the user reported completing the first level without any issues. This extends validation beyond the short controlled tests, but it is user-reported and does not resolve the original display-session crash.
