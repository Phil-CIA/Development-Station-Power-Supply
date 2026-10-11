# Repo Cleanup Plan (priority-ordered, agent-delegable)

State as of main `89e1021` (after PRs #111 and #107 merged).
Rules for every step: verify with `git`/`gh` first, never commit to main, no destructive action without
the owner's approval, tag any branch with unique work as `archive/<name>` before deleting it.
Commit trailer: `Co-authored-by: Copilot App <223556219+Copilot@users.noreply.github.com>`.

## P0 - Protect work (read-only, do first)

**Task 0.1 - Inspect main checkout** (`C:\Users\forch\GitHub\Development-Station-Power-Supply`)
- It is on `main`, 51 commits behind origin, with uncommitted deletions (`crowpanel-43-bringup/*`,
  `docs/DEV_STATION_HANDOFF_*`) and a modified `README.md`.
- Action: `git diff` / `git status` only. Report whether any change holds unique work.
- Done when: owner decides discard vs. save. If save, `git stash push -u -m "pre-sync backup"`.

**Task 0.2 - Archive tags for unique work** (owner approves list first)
- Branches: `phil-cia-stm32-flash-headroom` (#103, 18 commits), `phil-cia-crowpanel-setup-screen` (#71, 13),
  `phil-cia-star-lighting-feasibility` (#57, 1), `phil-cia-flash-size-optimization` (local, 2),
  `phil-cia-rev-c-bringup-docs-update` (local, 1), `phil-cia-didactic-invention` (local, 1),
  `revert-66-phil-cia-crowpanel-ui-audit` (1), `display/main-scope-ia-refresh` (1),
  `phil-cia-firmware-scope-buckets-alignment` (commits after merged PR).
- Action: `git tag archive/<name> <tip>` and `git push origin archive/<name>`.

## P1 - Decide open items (owner decisions, one at a time)

1. `phil-cia-bucket-3-execution-takeover` (9 commits, RB-013 LM74502 fix on Regulator Rev-C): review, then PR it.
2. #61 `phil-cia-bucket-3-execution` (draft, conflicts, failing STM32 build): superseded by takeover? If yes, close.
3. #103 flash headroom (closed, unmerged) + `phil-cia-flash-size-optimization`: reopen/re-PR, or archive.
   If abandoned, update the #102/#103 mapping in `docs/FIRMWARE_DEVELOPMENT_PLAN.md`.
4. #71 CrowPanel setup screen, #57 star lighting, `display/main-scope-ia-refresh`, `revert-66-...`:
   per branch choose PR / archive / delete. Display work must match `docs/display-project/README.md`.
5. `phil-cia-firmware-scope-buckets-alignment`: check post-merge commits; PR or archive.

## P2 - Sync main checkout

- After 0.1 is resolved: `git fetch --prune`, `git checkout main`, `git merge --ff-only origin/main`.
- Done when `git status` is clean and `git rev-list --left-right --count main...origin/main` is `0 0`.

## P3 - Remove stale worktrees and local branches (~33 worktrees, ~70 branches)

- Use `gh pr list --state all` to classify, NOT `git branch --merged` (PRs are squash-merged).
- For each worktree on a merged/archived branch with a clean status: `git worktree remove <path>`,
  then `git branch -D <branch>`. Skip any worktree with uncommitted changes; report it.
- Finish with `git worktree prune` and `git fetch --prune`.

## P4 - Delete obsolete remote branches

- Only branches tagged in 0.2 or fully merged. Keep `main`. Delete with `git push origin --delete <name>`.
- Enable "Automatically delete head branches" in GitHub repo settings to stop recurrence.

## P5 - Docs

- Update "Branch cleanup" in `docs/SYSTEM_DEVELOPMENT_WORKFLOW.md` and delete this file when finished.
- Docs PR from a branch; no new root-level HANDOFF/SUMMARY files.

## Exit criteria

Open PRs only for live work; one checkout clean and equal to origin/main; no worktree on a merged branch;
every unmerged idea is either a PR or an `archive/*` tag.
