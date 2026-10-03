# Evidence storage and restart

This checkpoint commits notes, findings, next steps, diagnostic source snapshots,
build/launch/analysis helpers, logs, small metadata summaries, and selected images.
It does not commit full videos, raw plane snapshots, full event streams, duplicate
merged traces, or all contact sheets. The scoped `.gitignore` keeps those large
local artifacts out of Git without deleting them.

Full local evidence remains here:

`/Users/noahjenkins/Code/cemu-swapforce-workspaces/cemu-evidence-b67ab9e/.codex-evidence/upstream-trace-20261002/`

`SHA256SUMS` covers that full local evidence set, excluding itself and Python
bytecode. It intentionally lists some files absent from a fresh Git clone.
The Deck retains original captures and diagnostic outputs under:

`/home/deck/CemuSwapForceTest/upstream-0a2b6ff-*/evidence/`

Use the checked-in launch helpers and `run.txt`/hash metadata to identify each
Deck run. In particular:

- Pacing runs: `upstream-0a2b6ff-pacing-<variant>-20261002`.
- Final command run: `upstream-0a2b6ff-command-nested-enabled-20261002`.
- Audit runs of the installed binary:
  `upstream-0a2b6ff-installed-audit-<singlecore|multicore>-20261002`.

The separate source worktree on Mac is:

`/Users/noahjenkins/Code/cemu-swapforce-workspaces/cemu-upstream-0a2b6ff`

Its Deck counterpart is:

`/home/deck/.local/share/cemu-native-main-build/upstream-0a2b6ff`

Both remain at base `0a2b6ff7db61871b0dd028ac47dc72eda94351a1` with local
diagnostic changes. The latest complete source artifact is
`light-trace/command-nested-trace-sites.patch` plus
`light-trace/command-nested-SwapForceVideoTrace.h`. The patch contains all tracked
diagnostic changes relative to that base; the header is the untracked addition.
Put the header at `src/Cafe/HW/Latte/Core/SwapForceVideoTrace.h` when restoring
the experiment in a new isolated worktree. Earlier stages remain separately
named. Do not apply cumulative stage patches on top of each other.

Immutable diagnostic binaries remain in the Deck source worktree's `bin/`.
Hashes and build logs identify them. The working installed AppImage is separate
and unchanged. Do not copy experimental binaries over it.

Private game code and disassembly remain outside all Git worktrees under
`/Users/noahjenkins/Code/cemu-swapforce-workspaces/swapforce-private-20261002`.
They are not part of this commit. No ROM, figure dump, keys, or full core dump is
included. Recheck current device activity before starting another session.
