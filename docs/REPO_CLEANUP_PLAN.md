# Repo Cleanup Plan (priority-ordered, agent-delegable)

Plan written against main `89e1021`. Rules for every step: verify with `git`/`gh` first, never commit to main, no destructive action without
the owner's approval, tag any branch with unique work as `archive/<name>` before deleting it.
Commit trailer: `Co-authored-by: Copilot App <223556219+Copilot@users.noreply.github.com>`.

## Status (2026-10-12, main `4fd82dd`)

| Phase | State |
|---|---|
| P0 | Done. Archive tags are on origin. Main-checkout backup stash `stash@{0}` ("pre-sync backup", `c57d4d2b006637eb7209106004662a544af0347d`) is owner-designated KEEP: never drop, pop, or apply it. |
| P1 | Done. #114 and #116 merged. #115 (`firmware/stm32-flash-headroom`) is an open draft; leave it alone. |
| P2 | Done. Main checkout is clean and equal to origin/main. |
| P3 | Done. Unique uncommitted work was saved to #117, #118, #119 (merged) and to tags `archive/crowpanel-setup-thresholds-wip`, `archive/revc-bringup-plan-2026-09-21`, `archive/docs-reorg-proposal-2026-09-20`. Remaining worktrees are intentionally kept: `phil-cia-didactic-invention` (untracked LVGL work), `phil-cia-jubilant-eureka` (Rev-D HAT, actively open in KiCad), `phil-cia-studious-doodle` (#115). |
| P4 | Ready for execution; inventory and procedure below. Not yet approved. |
| P5 | Not started. |

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

### P4 handoff (verified 2026-10-12)

Origin has 15 branches besides `main`. Because PRs are squash-merged, `git merge-base --is-ancestor`
fails for every one of them; do not use ancestry or `--merged` to classify. Preservation was verified
by matching each branch tip to a merged PR's head SHA or to an `archive/<branch>` tag at the same commit.

**Keep:**

| Branch | Reason |
|---|---|
| `main` | Default branch |
| `firmware/stm32-flash-headroom` | Head of open draft #115 |

**Group A - squash-merged; branch tip equals the merged PR's head:**

| Branch | Tip | PR |
|---|---|---|
| `phil-cia-unmerged-repository-work` | `fdf1f3a` | #113 |
| `phil-cia-bucket-3-salvage` | `16d8ed6` | #114 |
| `phil-cia-repo-cleanup-p1-branches` | `301d880` | #116 |
| `phil-cia-rb013-driver-rework-log` | `396ccbf` | #117 |
| `phil-cia-fan-control-spec` | `ee1865f` | #118 |
| `phil-cia-revc-erc-drc-snapshot` | `37c698e` | #119 |

**Group B - preserved by an `archive/<branch>` tag at the identical commit:**

| Branch | Tip | Context |
|---|---|---|
| `display/main-scope-ia-refresh` | `11f1c1a` | no PR |
| `phil-cia-bucket-3-execution` | `64389e2` | #61 closed |
| `phil-cia-bucket-3-execution-takeover` | `4099be4` | no PR |
| `phil-cia-crowpanel-setup-screen` | `6b040f9` | #71 closed |
| `phil-cia-firmware-scope-buckets-alignment` | `4e2308b` | #50 merged from an earlier commit; later commits are only in the tag |
| `phil-cia-star-lighting-feasibility` | `2841ec9` | #57 closed |
| `phil-cia-stm32-flash-headroom` | `09dcb91` | #103 closed |
| `revert-66-phil-cia-crowpanel-ui-audit` | `a9b6fa3` | no PR |

Deleting a closed PR's head branch prevents reopening that PR; its commits remain in the archive tag.

**Procedure:**

1. Get owner approval for Group A and Group B separately. Without approval, delete nothing.
2. `git fetch --prune origin "+refs/tags/archive/*:refs/tags/archive/*"`, then re-verify every row:
   - the remote tip still matches the short SHA in the table (`git ls-remote --heads origin <branch>`; use that full SHA below),
   - Group A: `gh pr view <n> --json state,headRefOid` shows `MERGED` and the same head SHA,
   - Group B: `git rev-parse "archive/<branch>^{commit}"` equals the tip.
   Skip and report any branch that changed, gained an open PR, or is checked out in a worktree.
3. Delete one branch at a time with a lease, so a moved branch is refused:
   `git push --force-with-lease=refs/heads/<branch>:<full-sha> origin :refs/heads/<branch>`
   (the lease only guards the delete; this is not a force-push of content).
4. After each group, confirm `git ls-remote --heads origin` lists only expected branches and the
   `archive/*` tags are untouched.
5. "Automatically delete head branches" is currently **off**. Turn it on only with separate owner
   approval: `gh api -X PATCH repos/Phil-CIA/Development-Station-Power-Supply -F delete_branch_on_merge=true`.
6. Finish with `git fetch --prune` in the main checkout and update the Status table above.

**Do not, in P4:** delete or move any tag, touch `main` or `firmware/stm32-flash-headroom`, force-push,
change the backup stash, remove worktrees, or start P5.

## P5 - Docs

- Update "Branch cleanup" in `docs/SYSTEM_DEVELOPMENT_WORKFLOW.md` and delete this file when finished.
- Docs PR from a branch; no new root-level HANDOFF/SUMMARY files.

## Exit criteria

Open PRs only for live work; one checkout clean and equal to origin/main; no worktree on a merged branch;
every unmerged idea is either a PR or an `archive/*` tag.
