# Datasheet Library

Single local home for datasheets, reference manuals, app notes, errata, and
web reference pages used by this project. Documents are fetched from the
internet once and then read locally.

Browse everything in [INDEX.md](INDEX.md) (generated).

## Automatic filing of repeat fetches

A Copilot CLI hook ([`.github/hooks/doc-fetch-cache.json`](../../.github/hooks/doc-fetch-cache.json)
→ [`scripts/doc_fetch_hook.py`](../../scripts/doc_fetch_hook.py)) watches every
agent `web_fetch` and every shell download (`curl`, `wget`, `Invoke-WebRequest`, …):

| Fetch of the same URL | What happens |
|---|---|
| 1st | Allowed. Logged and the result staged in `~/.copilot/doc-fetch-cache/` (shared by all worktrees/sessions on this machine, not committed). |
| 2nd | Filed into this library instead: PDFs are downloaded to `parts/<PART>/` with text extracted; web pages are saved to `web/<slug>/content_<start>.md`. The fetch is blocked and the agent is told where the local copy is. |
| 3rd+ | Blocked, with a pointer to the local copy. |

- Auto-filed entries are tagged `auto-filed`. Agents should commit them on
  their branch and tidy the metadata (`add <PART> --title --mfr --tags`).
- Append `#refetch` to a URL to bypass the hook deliberately (e.g. new revision).
- Vendors that block scripted downloads (e.g. Microchip returns 403) cannot be
  auto-filed; the fetch is allowed and the agent should save the file manually
  with `add <PART> --file <downloaded.pdf>`.
- Settings: `DOC_FETCH_THRESHOLD` (default `2`), `DOC_FETCH_HOME` (state dir),
  `DOC_FETCH_DISABLE=1` (off). Hook errors never block a tool; they are logged
  to `~/.copilot/doc-fetch-cache/hook-errors.log`.
- GitHub PR/issue/API URLs and localhost are never tracked.
- Copilot cloud agent sessions run the same hook, but their state directory is
  ephemeral, so counting only happens within one job there.

## Look it up first

```powershell
python scripts/datasheet.py find INA219              # by part number, tag, title, or board
python scripts/datasheet.py find "calibration register"   # full-text search of extracted text + notes
python scripts/datasheet.py list
```

`find` exits non-zero when nothing matches. Read in this order:

1. `parts/<PART>/NOTES.md` — curated key specs and project gotchas (cheapest).
2. `parts/<PART>/<file>.txt` — extracted text with `===== page N =====` markers; grep it.
3. `parts/<PART>/<file>.pdf` — only when you need figures, tables, or layout.
4. `web/<slug>/content_<N>.md` — saved web reference pages.

## Add a missing document

```powershell
# from a URL
python scripts/datasheet.py add INA219 --url https://www.ti.com/lit/ds/symlink/ina219.pdf `
    --mfr "Texas Instruments" --title "Current/power monitor, I2C" `
    --tags current-sense,i2c --used-in dsp-regulator

# from a local file, or a second document for the same part
python scripts/datasheet.py add ESP32-S3 --file C:\Downloads\esp32-s3_technical_reference_manual_en.pdf `
    --name ESP32-S3_TRM.pdf --doc-type reference-manual
```

`add` stores the PDF, extracts text (needs `pip install pypdf`), writes
`meta.json` (source URL, sha256, date), creates a `NOTES.md` stub, and
regenerates `INDEX.md`. Re-running `add` merges tags/metadata; it will not
overwrite an existing PDF without `--force`.

Commit the whole `parts/<PART>/` folder plus `INDEX.md` with your branch.

## Conventions

- One folder per part number (upper-case slug). Variants share a folder unless
  their datasheets differ.
- `--used-in` uses board/subsystem names from `hardware/kicad/` or firmware
  folders (e.g. `dsp-regulator`, `front-display-board`, `crowpanel-43-bringup`).
- When you extract a spec from a datasheet during work, record it in that
  part's `NOTES.md` with a page reference so the next session does not have to
  re-read the PDF.
- Run `python scripts/datasheet.py reindex` after manual edits or merge
  conflicts in `INDEX.md`; never hand-edit the index.
- Existing extracted-spec docs elsewhere (e.g.
  [`../IPS3608_REFERENCE_MANUAL_KEY_SPECS.md`](../IPS3608_REFERENCE_MANUAL_KEY_SPECS.md))
  stay where they are.
