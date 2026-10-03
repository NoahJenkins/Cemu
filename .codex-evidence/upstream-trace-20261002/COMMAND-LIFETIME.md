# Command lifetime check and stopping point — 2026-10-02

## Result

The video still has clear red/green ghosts with command checking enabled and
all pacing interventions disabled. The last periodic record covers 480 main
movie-list executions and 960 nested-list executions. Their full-buffer hashes
are unchanged at the measured boundaries. There are no reported mismatches,
missing children, unseen children, parse exclusions, or queue/size exclusions
through that record. This shifts priority toward movie-plane ownership and
texture publication/cache behavior.

This is not proof that every command read is race-free. The snapshots are not
atomic. A change and restoration between checks, a hash collision, or state
reached through non-command pointers would not necessarily be detected.

## Method and validation

The first probe checked main lists only and counted two nested calls per list.
It still showed ghosts. The second probe closes that coverage gap by parsing
bounded indirect-list references at submission and matching them to actual
command-processor entries. It hashes:

1. The main list just before TCL submission, on command-processor entry, and
   after that main list finishes processing.
2. Each nested list before main-list submission, on actual nested-list entry,
   and after the main list finishes processing.

The diagnostic retains only hashes, counts, and addresses in logs. It does not
write command contents. Nested parsing checks mapped ranges, size, depth, and
list-count bounds. Root lists are identified through the existing linear-R8
binding filter; this is not a general validation of all game command buffers.

The nested probe uses immutable `Cemu_command_nested_0a2b6ff`, SHA-256
`7c25334ab5f39e3338423346e9733a08c2fb8457b94544e274558e591e919da4`.
All ten diagnostic source-file hashes match between Mac and Deck. The build
passed. The isolated multicore capture completed with exit status 0 and valid
50-second, 24-FPS media. The final captured frame still shows the title screen.
There are no pacing log entries. Dense 24–28-second contacts show the defect.

The last periodic record is at 28.280869 seconds after trace-clock creation:
480 roots, 960 matched children, all mismatch/exclusion counters zero. This is
a lower bound on completed checks, not a final count for the entire recording.
The periodic records include the moving startup scene. No exact video-frame
to command-read join is claimed. Hashing, parsing, and a short metadata mutex
add overhead, but the defect remains visible in this instrumented run.

Artifacts:

- `light-trace/command-nested-enabled/`: dense contacts and final frame; original
  video, Cemu log, launch settings, and binary hash under `evidence/`.
- `light-trace/command-check-enabled/`: the earlier main-list-only probe.
- `light-trace/command-nested-trace-sites.patch` and
  `command-nested-SwapForceVideoTrace.h`: final diagnostic source snapshot.
- `command-nested-{source-hashes,deck-source-hashes}.json`, build log and binary
  hash under `light-trace/`.
- `light-trace/{build,launch}-command-nested.{sh,py}`: build and test helpers
  (build is `.sh`, launch is `.py`).

## Resume here

No more tests are scheduled. The user requested a break after this checkpoint.

1. Read HANDOFF, AUDIT, PACING, and this note before starting another experiment.
2. Recheck current Deck activity and preservation hashes. Keep the installed
   single-core workaround, user saves, original figure, and older PSQ tree.
3. Trace actual decoder start/completion and plane-generation ownership through
   publication, GPU texture read, and reuse. The earlier queue descriptor only
   predicts the output slot at job publication; it is not decoder-entry proof.
4. Follow the game's buffer-release callback and Cemu's texture invalidation
   path. The game's queue-message free has not been proved to release planes.
   Establish which operation permits reuse before changing emulator behavior.
5. Include capture/environment controls before comparing Desktop startup results
   with the user's successful Game Mode level play. Do not treat single-core,
   interpreters, or sparse clean frames as established clean-run controls.

Do not repeat broad PSQ/FPU/barrier/full-sync/reload changes without new evidence.
Do not install the pacing experiment as a fix. The next source change must
follow a demonstrated data-lifetime or publication failure. The separate
Xwayland crash remains unresolved and was not investigated in this batch.

## Preserved state

The installed binary, CPU-mode-1 profile, figure, four live save files, and older
Deck source diff all match their pre-batch hashes. See
`light-trace/pacing-preservation-{before,after}.json`. No Cemu, capture, debugger,
test unit, or build container remains active after the batch. The Deck was
returned to Game Mode; Magic Black mode 1, opacity 1, and its full-viewport
rendered black overlay were verified. `magic-black-break-final.png` records it.

The main checkout remains unchanged at `5ead580`. The experimental source
worktree remains separate at upstream `0a2b6ff` with local diagnostic edits.
Their final patch/header snapshots are retained in this evidence branch.
No upstream code contribution or PR was made. The policy on human-written code
still applies before any submission.
