# System Development Workflow

Status: active — this is the source of truth for how work moves through this
repo from now on. It replaces the ad-hoc "dated handoff doc" pattern used
during board bring-up.

## Why this changed

Board bring-up was single-threaded: one person, one bench, one branch
(`main`), progress tracked with dated `HANDOFF_*.md` files. That worked for
debugging one board at a time, but it doesn't scale to system development,
where firmware, the two display paths, and hardware revisions move in
parallel and need to be reviewed before they land on `main`.

The earlier direct-commit workflow left unreviewed branches and unclear
ownership. The one-time repository cleanup is now complete; ongoing branch
and worktree lifecycle rules are documented under "Branch cleanup" below.

## Branching model

- `main` is always buildable and reflects the current bench-validated state.
- Do all new work on a branch, never commit directly to `main`.
- One branch per PR, scoped to a subsystem:
  - `firmware/<topic>` — ESP32 control firmware in `src/`
  - `display/<topic>` — CrowPanel or custom front-panel work
  - `hw/<topic>` — KiCad/hardware changes under `hardware/`
  - `docs/<topic>` — documentation-only changes
- Branch names are kebab-case and describe the change, not the date
  (`firmware/rail-telemetry-map`, not `2026-08-14-work`).
- Open a pull request into `main` when the branch is ready for review, even
  if you are the only reviewer. The PR description is where "what changed
  and why" now lives — not a new dated handoff file.
- Merge only when:
  1. The firmware/hardware claim in the PR has been bench-verified (or
     explicitly marked "not yet bench-tested" if merging design-only work).
  2. Any doc that the change makes stale has been updated in the same PR
     (README.md, `docs/display-project/README.md`, `docs/GPIO_PINOUT.md`,
     tracker docs, etc.).
  3. For firmware PRs, the PR description names the relevant scope bucket
     from `docs/FIRMWARE_DEVELOPMENT_PLAN.md` and includes the matching
     exit-criteria evidence.
- Head branches auto-delete on merge. Remove the associated worktree after
  preserving any remaining local changes; see "Branch cleanup" below.

### Minimal git cheat-sheet

```powershell
git checkout main
git pull
git checkout -b firmware/rail-telemetry-map

# ... make changes, test on bench ...
git add -A
git commit -m "Add per-rail telemetry mapping"
git push -u origin firmware/rail-telemetry-map
gh pr create --fill
# after review/approval:
gh pr merge --squash --delete-branch
```

## Where things live going forward

| Kind of content | Location |
|---|---|
| Current, still-true project status | `README.md` (top-level) and the per-topic README under `docs/` |
| Subsystem reference (pinouts, interface standards, trackers) | `docs/*.md`, kept updated in place — not re-created per session |
| One-off historical record of a completed investigation | `docs/handoff-archive/` (see below) |
| Design source | `hardware/`, `src/`, `crowpanel-43-bringup/`, `stm32-bluepill-bringup/` |

Stop creating new root-level `HANDOFF_*.md` / `*_SUMMARY.md` / `*_HANDOFF.md`
files. If a session produces a durable finding, put it in the relevant
tracker doc (e.g. `docs/REGULATOR_BOARD_CHANGE_TRACKER.md`,
`docs/USB_HUB_CHANGE_TRACKER.md`) or the PR description. If it's genuinely
just a point-in-time snapshot with no lasting value once merged, it belongs
in `docs/handoff-archive/`, not the repo root.

### Root-level doc cleanup (follow-on task, not done yet)

The repo root currently has ~10 dated handoff/summary files left over from
bring-up (`BREAKTHROUGH_ROOT_CAUSE_FOUND.md`, `HANDOFF_2026-07-28_*.md`,
`RB-011_CLOSURE_SUMMARY_2026-08-12.md`, etc.), while `docs/handoff-archive/`
already holds the earlier round of these. Recommended next step: move the
still-current ones into the matching tracker doc's content, then archive the
files themselves under `docs/handoff-archive/root-handoffs/`. Do this as its
own `docs/cleanup-root-handoffs` PR so it doesn't get mixed with feature
work — ask if you want this done now.

## Branch cleanup

The one-time repository cleanup is complete as of 2026-10-12. PRs
[#113](https://github.com/Phil-CIA/Development-Station-Power-Supply/pull/113)
through [#119](https://github.com/Phil-CIA/Development-Station-Power-Supply/pull/119)
record the preservation and recovery work (#115 remains open for live
STM32 flash-headroom work). Obsolete branches and worktrees were retired
after preserving unmerged ideas; active worktrees remain in use.

Going forward:

- Use one branch per PR. GitHub's "Automatically delete head branches"
  setting is enabled, so remote PR head branches auto-delete on merge.
- Never delete a branch without first checking its content against the
  current `origin/main`. Before merging, ensure the PR covers the branch's
  current tip and any unmerged ideas have been preserved.
- Squash merges break ancestry checks: `git branch --merged` or
  `git merge-base --is-ancestor` alone cannot establish that work was merged.
  Compare tree IDs for identical snapshots, verify the branch tip against
  a merged PR's head SHA, or inspect `git cherry` / `git range-diff` and the
  content diff against `origin/main` to account for remaining changes.
- Keep live work in a PR. Before deleting a branch with unmerged ideas,
  preserve its tip as an `archive/<branch>` tag and verify that the tag on
  origin resolves to that commit. Archive tags are retained, not moved or
  deleted.
- Remove worktrees once their PR merges, then remove the obsolete local
  branch. Inspect tracked and untracked changes first; preserve any unique
  work before removing a worktree. Do not force-remove a dirty worktree.

List the remote archive tags:

```powershell
git ls-remote --tags origin 'refs/tags/archive/*'
```

Restore an archived idea to a new working branch (replace `<b>` with the
original branch name; the local branch name must be available):

```powershell
git fetch origin tag archive/<b>
git switch -c <b> archive/<b>
```

## Firmware build check (recommended, not yet added)

There is currently no CI. Recommended minimal addition once the workflow
above is in place: a GitHub Actions workflow that runs
`pio run` for each PlatformIO environment in `platformio.ini` on every PR,
so a PR that doesn't compile is caught before merge. This is scoped as a
separate task — flag if you want it added next.

## Definition of done for a merged PR

- [ ] Builds (firmware) or passes ERC/DRC (hardware), or is explicitly
      marked WIP/design-only in the PR description.
- [ ] Bench-tested claim is stated true or false in the PR description —
      don't leave it implied.
- [ ] Any doc this change makes stale is updated in the same PR.
- [ ] No new root-level dated handoff file was added.
