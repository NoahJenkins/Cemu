# Evidence audit — 2026-10-02

## Assessment

The CPU frame producer to GPU texture-consumption boundary remains the best
supported area. The exact cause is not established. A multicore-only instruction
fault was too narrow a hypothesis, and single-core was not a valid clean control
under the Desktop capture conditions. No new source patch was made during this audit.

## Corrected controls

Dense review of all captured frames in two-second windows found brief red/green
ghosts in the preserved, unmodified upstream single-core recording. Earlier
sparse contact sheets missed these bursts. Evidence:

- `light-trace/singlecore-baseline24/audit-{22,24,26,28}-contact.png`.
- `light-trace/singlecore-baseline24/audit-corrupt-24.458333.png`.
- Recording SHA-256: `fc1b84adee2373cd71659c2661dc111387b09c2b1ad7df6b9f45bb3ba286c3ca`.
- Its log confirms single-core recompilation. Its executable is the preserved
  unmodified upstream baseline, not a trace build.

Then ran the installed Cemu 2.6 executable with separate copied settings,
saves, figure, and explicit isolated MLC. Both CPU modes have brief ghosts.
The original installation and live data were unchanged. Both 50-second,
24-FPS capture services completed successfully. Logs confirm CPU modes 1 and 3.
Each directory has its video, log, hashes, window title, and media metadata:

- `light-trace/installed-audit-singlecore/`.
- `light-trace/installed-audit-multicore/`.
- Dense contacts cover 16–24 seconds in both runs; 18–20 seconds shows ghosts
  clearly. Multicore also has ghosts near the end of the 16–18-second window.
- `light-trace/launch-installed-audit.py` records the isolation and launch method.

These tests do not contradict the user's successful first level. They concern
the startup movie in Desktop X11 while recording. They do not establish artifact
rates in normal Game Mode. Capturing at 24 FPS also cannot cover every presented
frame. Absence in a contact sheet is weaker evidence than a visible bad frame.

## What the experiments establish

| Test | Supported conclusion | Remaining limit |
| --- | --- | --- |
| Unmodified upstream multicore | Defect still reproduces at `0a2b6ff` | Does not identify its origin |
| Actual texture upload snapshots | In one instrumented example, guest planes and actual decoded bytes are inconsistent before Vulkan staging | Copies race with writes and tracing adds delay |
| Movie-submit retirement wait, disabled/enabled/disabled | Same diagnostic binary fails, has no ghosts in reviewed enabled frames, then fails again | Wait changes CPU scheduling; command-list reuse and plane reuse are both possible |
| Wait/retirement logs | Recorded waits did not return before matching retirement | Does not prove all completion or cache contracts correct |
| Job output descriptors | Queued decode jobs predict the same plane triplet used by a pending movie list | Prediction at queue publication is not an atomic read at actual decoder entry |
| PSQ fallback, float-copy disable, earlier full sync, forced R8 reload | These tested changes did not remove the defect | No reason to repeat without new evidence |
| Interpreter and broad FPU fallback | Some moving scene samples were clean | Slowdown and sparse sampling prevent instruction isolation |

The previous statement that tracing *caused* single-core corruption is withdrawn.
Unmodified single-core can also show it. Trace overhead is still a confound, but
its effect on artifact frequency has not been measured.

## Source review during the audit

A paused, read-only GDB probe resolved 410 game imports on unmodified upstream.
Metadata is in `light-trace/release-contract-x11/`; private game code remains
outside all Git worktrees. The render consumer calls `GX2DirectCallDisplayList`,
then later runs an optional callback and frees a queue message. That message
free has not been proved to release movie planes. The movie controller waits
for decoder results, calls another virtual function, advances the movie, and
queues the next decode. Its complete release contract is still unresolved.

Cemu's direct display-list path submits a guest-memory pointer without copying
the command bytes. The command processor later reads guest memory. Thus command
memory stability is an unchecked alternative to plane overwrite. Texture
invalidation marks cached data for refresh; the texture loader subsequently
reads each plane. Source inspection alone does not establish the hardware's
required cache and completion behavior.

The first GDB attempt in default Desktop Wayland failed during Cemu startup.
The explicit X11 retry succeeded. This failed probe supplies no movie evidence
and does not diagnose the separate historical Xwayland crash.

## Follow-up completed after this audit

[PACING.md](PACING.md) records the completed time-only delay comparison.
[COMMAND-LIFETIME.md](COMMAND-LIFETIME.md) records the root/nested command check
and the next-session plan. Those results refine the test list below.

## Next decisive tests

1. Establish capture effects with matched startup scenes and a recording method
   that covers the presented frames. Compare Game Mode and Desktop conditions
   before attributing the difference from normal play to CPU mode alone.
2. Add bounded metadata at actual decoder entry/completion, plane publication,
   GPU read, and buffer release. Track a frame generation per plane. First
   validate that the probe does not materially change the measured failure rate.
3. Check submitted display-list bytes against bytes read by the command processor.
   Cover direct and nested lists; never publish private command or game dumps.
4. Compare the retirement wait with a matched time-only delay. If either masks
   the failure, the intervention is evidence of timing sensitivity rather than
   a specific missing completion operation.
5. Use the result to choose the next source change: ownership/reuse if a producer
   overwrites pending data, command lifetime if a submitted list changes, or
   texture publication/cache behavior if both remain stable.

Do not add another broad recompiler, barrier, or synchronization patch first.
No root-cause fix, upstream issue, or PR has been prepared. Cemu's contribution
policy was read; local AI-authored diagnostics are not an eligible code submission.

## Preservation

After both audit captures, installed binary, live CPU-mode-1 profile, original
figure, and older Deck source patch hashes match their pre-test values. All four
live save files match the copied pre-test save. No Cemu, GDB, ffmpeg, running test
unit, or build container remained. Desktop DPMS was verified off during review.
Final Game Mode and Magic Black verification are recorded in the adjacent README.

Process improvement: review dense moving transitions before designating a run
as a negative control. Keep a positive bad-frame observation separate from an
absence claim based on limited sampling.
