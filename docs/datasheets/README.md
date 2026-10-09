# Datasheet Library

Single local home for datasheets, reference manuals, app notes, and errata used
by this project. **Agents and humans must check here before fetching any
document from the web**, and must add anything newly fetched so it is never
downloaded twice.

Browse everything in [INDEX.md](INDEX.md) (generated).

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
