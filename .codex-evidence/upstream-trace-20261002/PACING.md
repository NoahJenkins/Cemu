# Submission pacing experiment — 2026-10-02

## Question and result

Does the earlier retirement-wait intervention require checking GPU retirement,
or can a similar time-only delay also suppress the observed startup ghosts?

Both fixed delays (1 ms and 2 ms) suppress ghosts in the reviewed captured
frames. The retirement wait does too. The same binary with all intervention
switches absent shows ghosts before and after the test sequence.

This weakens the claim that the intervention identifies a missing retirement
operation. It does not disprove premature plane reuse: the texture reads may
already have finished before a list's retirement marker. A delay can protect
that earlier boundary without waiting for the marker. No root-cause fix follows
from this result.

## Method

Used one immutable diagnostic executable, `Cemu_pacing_0a2b6ff`, SHA-256
`97eadd91661763520780e4376b5161aad4a8e3a7a6cc703b811f711091ba3589`.
Only the pacing switch changes. Each run has fresh settings/save/figure copies,
empty isolated cache, explicit isolated MLC, multicore recompilation, Plasma X11,
and a 50-second 24-FPS recording. No plane or job trace is enabled. All five
transient services completed with exit status 0, checked before the next run.

The fixed-delay branch sleeps for the requested duration after the same
identified list submissions. It reads the retirement timestamp only afterward
for observation; it does not extend the delay until retirement. The identification
filter and eight remembered buffer bases are inherited from the earlier probe.
They can include other linear R8 lists; these results cover startup only.

Execution order and results:

| Run | Reviewed video | Mean elapsed delay over first 480 identified submissions | Still pending when delay ended, first 480 |
| --- | --- | ---: | ---: |
| Control before | Ghosts, clear at 24–26 seconds | No intervention | Not recorded |
| Fixed 2 ms | No ghosts seen in reviewed 22–30 seconds | 2,080.93 microseconds | 1 |
| Retirement wait | No ghosts seen in reviewed 22–30 seconds | 1,011.04 microseconds | 0 |
| Fixed 1 ms | No ghosts seen in reviewed 22–30 seconds | 1,072.97 microseconds | 84 |
| Control after | Ghosts, clear at 24–26 seconds | No intervention | Not recorded |

Counts and elapsed time are cumulative across every intervention, with the
first and each 120th state logged. The 1 ms run has 58 pending returns by count
240 and 84 by count 480. This is more informative than its later count of 591
by 1080, which includes the title screen. These are not per-frame GPU read
timestamps, and no pending return is asserted to match an exact captured frame.

The first-480 means are similar for 1 ms and retirement, but this is not exact
per-submission duration matching or replay. One run per setting does not measure
a reliable artifact probability. All captured frames in the stated eight-second
interval were reviewed through 24-FPS contacts. A screen recording can still
miss presented frames between samples. Clean static logos are not evidence of
correct moving video.

## Evidence

- `light-trace/pacing-{control-before,delay2000,retirement,delay1000,control-after}/`:
  dense contacts, with original video/log/settings/hash metadata in `evidence/`.
- `light-trace/pacing-results.json`, reproducible with `analyze-pacing.py`.
- `light-trace/pacing-trace-sites.patch`, `pacing-SwapForceVideoTrace.h`,
  `pacing-source-hashes.json`, `pacing-build.log`, and `pacing-binary.sha256`.
- `light-trace/launch-pacing.py` and `build-pacing.sh`.

The working installed binary and per-game workaround were never replaced.
The next observation checks command-buffer lifetime, with pacing disabled.
