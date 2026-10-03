# Current-upstream investigation — 2026-10-02

## Result

The moving startup video defect reproduces on unmodified upstream
`0a2b6ff7db61871b0dd028ac47dc72eda94351a1` with multicore recompilation.
Dense review found brief ghosts in the unmodified single-core reference too.
Fresh isolated runs of installed Cemu 2.6 show bursts in both CPU modes under
Desktop X11 capture. The installed working setup is preserved. Read [AUDIT.md](AUDIT.md)
for the corrected assessment; sparse clean samples are not clean-run controls.

The diagnostic multicore run locates an observed transition from a clean guest
binding to mismatched guest plane data at texture-upload input. The actual
texture decoder output contains the mismatch before Vulkan staging. This is
an observed boundary in the instrumented run, not proof of the underlying
writer or synchronization fault. A trace-disabled submission-wait test now
suppresses the sampled startup ghosts, with both disabled controls failing.
This supports a buffer reuse/consumption race, but does not identify the faulty
emulated contract. No installed fix or upstream submission exists.

## Follow-up after the audit

[Submission pacing tests](PACING.md) show that fixed 1 ms and 2 ms delays also
suppress ghosts in reviewed frames. Both disabled controls fail. The 1 ms delay
has similar aggregate cost to the retirement wait but often returns while the
list is still pending. This weakens a retirement-specific interpretation; GPU
texture reads can finish before list retirement. The [command-list check](COMMAND-LIFETIME.md) found no changes across the
recorded 480 main and 960 nested executions while ghosts remained visible.
Prioritize plane lifetime and texture publication/cache behavior next.
The user requested a committed/pushed checkpoint and a break after these tests.

## Reproduction and isolation

Read the handoff, findings, and separate crash note at evidence commit `b67ab9e`
first. Fetched upstream on October 2; its head is the revision above. Created
independent source worktrees on Mac and Deck. Original main and the older Deck
PSQ diagnostic tree were not edited.

The user saved and authorized closing the live game and switching modes. Sent
`WM_DELETE_WINDOW` to the observed title window and verified process exit.
Ran isolated tests in Plasma X11 with fresh display/auth environment. Each test
has independent config/data/cache, a copy of the current four-file save, and a
copy of the current figure under `/home/deck/CemuSwapForceTest/`. The live MLC
path was overridden in settings and with `--mlc`. Both modes used the same
Portal Stability Fix graphic pack. No copied figure was loaded during these
startup-only tests.

Baseline built with clang-15/Ninja in `localhost/cemu-build-env:v2.6`. Updated
upstream wxWidgets dependencies were built, cubeb nested dependencies initialized,
and `-DLINUX=ON` supplied for CMake 3.22 without editing source. The baseline
binary SHA-256 is
`ac1a4dc767e45858c34ae0443b3b3c702b6997051e8009b15aaa1531b4a2fc10`.
It is preserved on the Deck as `bin/Cemu_baseline_0a2b6ff`.

The baseline pair and diagnostic pair each have valid 50-second, 12 FPS screen
recordings. Logs confirm numeric profile modes 3 (multicore) and 1 (single-core).
The diagnostic multicore recording still has prominent red/green ghosts; the
single-core recording is clean at the sampled scene times. Held logos and some
moving frames can be clean even in the failing mode. Scene samples are not
claimed to have identical movie frame numbers.

Reference files:

- `multicore-baseline/intro.mp4`, `singlecore-reference/intro.mp4`, and Cemu logs.
- `multicore-corrupt.png`, `singlecore-clean.png`: corresponding scene samples.
- `{multicore,singlecore}-trace/intro.mp4`, `events.jsonl`, and analysis JSON.
- Trace source: `trace-sites.patch` plus `SwapForceVideoTrace.h`.
- Deck build logs: isolated source worktree `evidence/` directory.
- Test launch helper: `record-intro.sh`; analysis helpers beside this note.

## Observed boundary

Instrumentation preserves original decode/cache/upload order and CPU semantics.
It records guest texture publication, sparse guest rows before and after decode,
the actual decoded upload buffer, and each plane's texture pointer/reload
number at the draw. Copies precede asynchronous file writes. All raw copies
can race with guest writes and add timing cost. Metadata is not a guest-thread
synchronization trace. No events were dropped.

Multicore: 4,912 events, 127 sampled uploads, 47 draws with all three sampled
upload generations. Single-core: 4,771 events, 124 sampled uploads, 43 complete
sampled draws. Guest/decoded/after bytes differ in 44 multicore and 64 single-core
sampled uploads. Thus overlap alone does not distinguish the CPU modes.
Some reconstructed upload samples also show transient mismatch in single-core;
the 12 FPS recording cannot exclude short artifacts between sampled times.

The retained `multicore-trace/boundary-pair/` is tied to emulated frame 720,
draw 3306:

| Stage | Events | Relative trace time (microseconds) | Observation |
| --- | --- | --- | --- |
| Guest bindings units 0/1/2 | 3602/3603/3604 | 26412675/26413506/26413705 | Reconstructed movie scene has aligned color planes. Each plane's two binding copies is byte-identical. |
| Upload inputs | 3605/3607/3609 | 26414552/26416838/26417196 | Same physical plane addresses now contain mismatched scene versions. |
| Actual decoder output | same upload events | captured after decode | Reconstructed color ghosts are already present. |

The first upload event begins 1,877 microseconds after the first binding event.
Event timestamps precede each copy; they are not copy completion timestamps.
The luma plane changes by 32,203 bytes over 64 rows from binding to upload input,
then by 34,957 bytes over 72 rows from upload input to decoder output. Chroma
planes change between publication and upload but remain byte-identical across
their upload-before/decoded/after samples. The three exact texture reload
numbers are 186, 131, and 136. They are the generations recorded at that draw.

`guest-bind.png`, `upload-before.png`, and `actual-decoded.png` show this
transition. `comparison.json` contains event metadata, hashes, and differing
byte/row counts. `raw/` retains all 15 selected plane files, so this example can
be reviewed without the Deck. Other raw snapshots remain on the Deck (about
693 MiB multicore and 678 MiB single-core).

Reconstruction uses units 0/2/1 as Y/U/V, verified by the displayed scene colors.
It uses a diagnostic ffmpeg YUV conversion, not Cemu's exact shader or sampler.
The scene is cropped to avoid unused texture padding. A reconstructed triplet
is not an atomic read of guest memory. Upload generation association is exact;
guest publication association uses the immediately preceding same-address
bindings and does not assert a completion fence.

## Follow-up: trace controls, writers, and command ownership

A full metadata trace with 512-byte sampled plane hashes adds timing cost.
Its multicore **and single-core** recordings show color ghosts. The single-core
run is not a clean control, but baseline single-core also fails; trace causation
is not established. Do not use this pair as causal evidence for a mode-specific bug.
The trace does identify callers and observed ordering, but it is not a safe
performance or correctness reference.

To separate recording cost from trace cost, ran the preserved, unmodified
baseline in both modes at 24 FPS for 50 seconds. The sampled single-core moving
scene initially appeared clean in sparse contacts; dense review now finds brief
ghosts in that same recording. Multicore also has visible ghosts. Then restricted the trace
to relative times 25,000,000–25,250,000 microseconds. The first two short-window pairs
show no ghosts in sampled single-core scenes and show multicore ghosts. The
later queue-ring selector trace also shows single-core ghosts; see below.
The audit does not establish that tracing introduced these artifacts.
All movie captures and their logs are retained under `light-trace/`. The full
multicore interpreter capture lasts 70 seconds because startup is slower; its
sampled moving scene is clean.

| Run | Actual uploads | Input/decoded sampled hash changes | Decoded/after changes | Matched waits | Early wait returns |
| --- | ---: | ---: | ---: | ---: | ---: |
| Full metadata, multicore recompiler | 1,424 | 242 | 592 | 1,171 | 0 |
| Full metadata, single-core recompiler; failed control | 1,435 | 33 | 329 | 1,137 | 0 |
| Full metadata, multicore interpreter | 1,243 | 2 | 7 | 1,020 | 0 |
| 250 ms window, multicore recompiler | 23 | 4 | 11 | 16 | 0 |
| 250 ms window, single-core recompiler | 20 | 0 | 4 | 14 | 0 |
| Window plus command/mutex tags, multicore | 20 | 5 | 8 | 16 | 0 |
| Window plus command/mutex tags, single-core | 21 | 0 | 1 | 14 | 0 |

No trace events were dropped. Input, decoded, and after hashes each sample 512
active bytes; these are neither full-plane hashes nor atomic snapshots. These
counts describe observations, not the incidence of displayed corruption.
The last window loses one retirement at its end boundary; all 16 waits still
have matching retirement records. Video contact sheets sample 2 FPS from the
24 FPS captures. They cannot exclude brief single-core artifacts.

The first short-window pair changes between binding and upload input in 17 of
23 multicore uploads and 3 of 20 single-core uploads. With command/mutex tags,
those counts are 13 of 20 and 1 of 19; two single-core uploads precede the first
window binding and have no preceding binding to compare.

The latest build records the command write address at texture binding. All 24
multicore and 21 single-core binding commands fall inside subsequently submitted
display lists. The two `GX2DrawDone` waits return **before** submission of the
list containing those movie bindings. This happens in both CPU modes. Therefore
the recorded waits are not a fence for that new movie list; this is not evidence
that `GX2WaitTimeStamp` itself returned too early.

For the first complete triplet in the latest multicore window, relative to its
first texture binding:

| Event | Time after binding (microseconds) |
| --- | ---: |
| First/second earlier wait return | 78 / 124 |
| Submit movie display list, timestamp 2070 | 247 |
| Submit decoder jobs to two worker queues | 383 / 386 |
| Worker queue reads complete on guest cores 2 / 0 | 414 / 417 |
| Luma upload starts | 543 |
| Core 2 worker publishes its result queue | 834 |
| Chroma uploads start | 1,125 / 1,293 |
| Movie list retires | 1,563 |
| Core 0 worker publishes its result queue | 1,820 |

The single-core counterpart submits the list at 154 microseconds, finishes the
worker queue reads at 386 / 450, and retires the movie list at 1,019. Worker
result publications follow at 1,800 / 1,890. This gives a concrete timing
relationship between decoder work and GPU consumption. The queues are inferred
from their actual producer/consumer code and matching mutex addresses. It does
not yet establish which active output buffer is in each job or prove a faulty
emulator API. `first-triplet-timeline.json` retains the event IDs and timestamps.

Hardware watchpoints in a separate, paused diagnostic run found actual movie
plane writes within guest function ranges `0x02bc5ad4`, `0x02bc5dd4`,
`0x02bc6104`, and `0x02bbf918`. The writers run on guest cores 0 and 2. JIT maps
identify function ranges; cached guest registers are not an exact guest PC at
the stopped store. The enclosing worker stack includes `0x02bb6e2c`,
`0x02bb8ea0`, `0x02bb8644`, and `0x02bbabec`. Watchpoint pauses cannot support
normal execution timing conclusions.

Resolved worker imports on the unmodified executable by reading their HLE table
entries, without calling functions from stopped threads:
`0x00e0062c` = OSLockMutex, `0x00e00974` = OSUnlockMutex,
`0x00e0096c` = OSTryLockMutex, `0x00e0060c` = OSInitMutex,
`0x00e00950` = OSSetThreadSpecific, and `0x00e003ac` = OSAddAtomic64.
The worker's state helper uses guest compare-and-exchange with SYNC/LWSYNC;
x64 STWCX generates locked CMPXCHG. No incorrect lock return, atomic result,
or PPC instruction has been demonstrated.

Private code copies and disassembly are outside Git worktrees at
`/Users/noahjenkins/Code/cemu-swapforce-workspaces/swapforce-private-20261002`.
Do not commit game code or memory dumps. GDB scripts and metadata are retained
under `light-trace/`; code dumps remain private.

A bounded GDB run on the unmodified executable then linked both producer
packets and both worker result packets to the same movie job object. Its field
at offset `0xe0` points to a two-slot output descriptor containing all six known
Y/V/U physical addresses and their pitches. The descriptor selector is at
`0x14`; each slot starts at `0x18 + index * 0x30`. Actual decoder code reads that
selector and writes the opposite allocated slot. This establishes the metadata
layout for a low-impact job-to-plane trace; it does not itself establish normal
execution timing. `job-packets3/job-packets.jsonl` retains the selected metadata.
The first packet probe stopped on a GDB unknown-type error and yielded no data;
the corrected probes read the HLE argument register using verified CPU offsets.

A further paused probe captured 64 packets across 16 job cycles, including both
alternating descriptor selectors. The first normal-execution selector probe,
which used shadow nonvolatile guest registers, emitted no job records. Its
1280 by 720 dimension filter excluded the moving movie job, which is 1040 by
620. The earlier logo job is 1280 by 720. A queue-ring probe with the same
filter also emitted no records. This is a selector-filter failure, not evidence
of stale guest registers. Its binary is preserved
as `Cemu_job_window_trace_0a2b6ff` with SHA-256
`45b51ba1cb07dad51538110160d86ea00521449b75c43ea35aae3fe25f7bdff7`.
The next probe reads the published queue-ring entry from guest memory instead.

Removing that dimension filter yields 14 job records in each CPU mode. All
14 queued jobs predict the same entire Y/V/U output triplet currently bound in
the submitted movie list, and each is queued before that list retires. The
prediction uses the descriptor's opposite allocated slot at queue publication;
it is not an atomic snapshot at the later decoder entry. Paused GDB confirmed
the ring payload and both alternating descriptor selectors. Job metadata and
its matching binding/submission/retirement IDs are in
`{multicore,singlecore}-job-ring2-window/job-overlap.json`.

The queue-ring selector trace **also contains visible single-core ghosts**.
Since the unmodified single-core reference also has ghosts, this does not prove
that tracing caused them.
It is a failed control for a mode-specific timing conclusion. Its ownership
metadata remains useful for choosing a causal test, but these event times are
instrumented observations. No faulty mutex, atomic operation, or guest
instruction has been proved. Counts for this pair: multicore 17 sampled uploads,
5 input-versus-decoded changes, 11 decoded-versus-after changes; single-core
21 uploads, 0 and 5 changes. Both have 14 matched waits, none returning before
their matching recorded retirement.

The 512-byte hash grid uses nominal 1280 by 720 / 640 by 360 sampling geometry.
It can include padding for the smaller active movie. These are sampled byte
changes, not whole active-plane equality claims. Guest snapshots are not atomic.

Diagnostic executable hashes:

- Full metadata: `bb111da810665693b02121ec4cf347df702d4da35dbe344ae5bc972ef357b53d`
- 250 ms window: `7e1dd69a0e3dc2a037453fbe72c169928d378557c7c57ca8bf3a46c430a12b0d`
- Window plus command/mutex tags: `d0eff37e1c1549c4ac4f8b8280f01966a701e7365aef7dbb3a502e383dd1848b`

Each diagnostic stage has its own source snapshot under `light-trace/`.
The queue selector source is `job-ring2-window-trace-sites.patch` plus
`job-ring2-window-SwapForceVideoTrace.h`. The later intervention source is
`post-movie-wait-trace-sites.patch` plus `post-movie-wait-SwapForceVideoTrace.h`. Full-trace source remains separately
retained. `analyze-light.py` merges the two JSON records per event, compares
sampled hashes, matches timestamp waits, and maps binding commands to submissions.
These are diagnostic changes only; no behavior fix has been applied.

## Trace-disabled causal intervention

After the actual display-list submission, the diagnostic executable can wait
for that list's CP retirement timestamp before the submitting guest core
continues. It recognizes submitted command-buffer bases that contained linear
R8 texture bindings for this title. This differs from the rejected GX2DrawDone
full-sync experiment: the recorded GX2DrawDone waits finished before this new
movie list was submitted. It performs no Vulkan device-idle wait.

This changes scheduling intentionally and is **not an emulation fix**. The
buffer-base filter can include other linear R8 lists and remembers eight bases;
these tests only cover startup. Polling sleeps 50 microseconds and stops after
five seconds. The switch is accepted only with isolated test settings and the
SWAP Force title. No trace or plane hashing is enabled in this test.

Ran the same immutable diagnostic executable in multicore mode three times:

| Run | Submission wait | Reviewed moving video |
| --- | --- | --- |
| Control before | Disabled | Red/green ghosts |
| Intervention | Enabled | No ghosts in the reviewed captured frames |
| Control after | Disabled | Red/green ghosts return |

Each recording is 50 seconds at 24 FPS. Sparse 3-second/4-FPS samples initially
missed short bursts in the first disabled control. Reviewing the 24-FPS
24–26-second transition reveals them clearly in both controls. The enabled
capture was reviewed at every captured frame from 22–30 seconds via four
contact sheets; it remains aligned. Captured frames cannot exclude artifacts
between screen samples. Review scene content rather than requiring identical
movie frame numbers across runs.

All 10 logged waits completed, including the first and every 120th through
count 1080. Their sampled durations are 928–4,398 microseconds. No timeout was
logged; completed counts are sampled, not a log of every wait. Logs confirm
multicore mode. Both disabled controls contain no fence log entries. All runs
used the same binary SHA-256:
`18eb0eca1d437326c78dae0aee3d054f2e7aa6bf11c753acf714ae1f1716f202`.

A fourth run of the preserved unmodified baseline reproduces the ghosts again.
This confirms that the upstream baseline still fails after the intervention
batch. Artifacts are under `light-trace/post-movie-{control-before,enabled,control-after}/`
and `light-trace/baseline-repeat/`. `post-movie-results.json` records settings,
media summaries, hashes, and logged waits. The exact diagnostic source and
build log are retained next to the launch helper.

Taken with the earlier actual-upload snapshots and matching job output slots,
this supports a race between reusing movie output planes and consuming them
for rendering. The wait can also perturb other thread scheduling. It does not
prove an incorrect GX2 timestamp, mutex, atomic, or PPC instruction. The earlier
single-core reference is not a clean control. Command-list reuse and CPU
scheduling remain competing explanations for the intervention result.

The next source review must follow the game's buffer release condition and
Cemu's texture publication/invalidation path. Determine whether Cemu reads
planes too late, observes a completion condition incorrectly, or fails to
preserve the expected cached contents. A further experiment must distinguish
those contracts before a proper fix can be selected. Keep the installed
single-core workaround in place.

Durable investigation improvement: inspect dense frames around moving scene
transitions. Sparse scene contacts can miss this intermittent defect.

## Source review and rejected experiments

Built-in RGBA texture dumping rereads guest memory after actual decode and
reuses address-based filenames. It is unsuitable as proof of actual upload
bytes. The local instrumentation copies actual `pixelData` instead.

Texture decoding precedes `LatteTC_ResetTextureChangeTracker`, contrary to its
adjacent timing comment. This order was not changed. Sampled tracker hashes
and once-per-frame checks do not prove full plane equality.

Both interpreter and recompiler SYNC/ISYNC paths and OSMemoryBarrier are no-ops;
this does not distinguish the multicore interpreter with clean sparse samples. x64
STWCX emits locked CMPXCHG. No instruction or barrier patch was applied.

Do not repeat whole-function PSQ fallback, float-copy disablement, full GPU
sync, or forced R8 reload without new evidence. FPU fallback changes timing and
is not instruction isolation. The separate Xwayland crash was not reproduced
or diagnosed by these video tests.

## Deck and OLED final state

Returned the Deck to Game Mode. No test Cemu or build container remains active;
all completed capture services exited successfully. MagicBlack's existing
Decky plugin is enabled in black-overlay mode at opacity 1. Its rendered
full-viewport black overlay was inspected and a fully black screenshot retained
as `magic-black-break-final.png` after the final command check. The earlier
`magic-black-enabled.png` remains as an earlier restoration record. No audio, touch, or dismissal preferences changed.
During Desktop build/review intervals, used DPMS off and verified Monitor Off.

Installed binary, live single-core profile (cpuMode=1), and original figure
hashes were checked again after tests and match the pre-test values:

- binary: `771c5a10b849d3b64ba5a329e04ca8b3d5e2d279d5d1bac526f593d6fc490a9f`
- profile: `db8f88edb400fb93ac73753cd36c83bd3f8af9b5f4b1989e6dcc211b665c7bc0`
- figure: `a3a47947f13a0ab3fb4f382bd0b287cdc13eca686554c9d081195b635b142d10`
- older Deck source patch: `8df835b63887ed8058218eb3ade930cb4dd0d4b9b62d062f0f41377b5f214995`

All four live save files also still match the preserved pre-test copies.
These checks were repeated after the pacing and nested-command runs, before restoring
Game Mode. No test unit, Cemu process, or build container remained active.

The current figure differs from the earlier handoff after user gameplay. It was
not reverted. The current-upstream diagnostic code exists only in the separate
source branch/worktree. Notes, probe sources, summaries, logs, and selected images are retained on the
evidence branch. Full recordings and raw snapshots remain local; see
[EVIDENCE-STORAGE.md](EVIDENCE-STORAGE.md). This checkpoint supersedes `b67ab9e`.

## Contribution policy

Read current upstream CONTRIBUTING.md. New contributor code must be written
and understood by a human; AI may assist planning/design/review. Paraphrasing
AI code does not satisfy that policy. These are local diagnostic changes, not
an upstream patch. No PR or issue comment was created.
