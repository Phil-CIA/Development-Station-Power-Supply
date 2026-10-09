#!/usr/bin/env python3
"""Local datasheet library for this repo.

Agents and humans should check here BEFORE fetching a datasheet from the web,
and add anything newly fetched so it is never downloaded twice.

Layout (docs/datasheets/):
    INDEX.md                    generated table of every entry (do not hand-edit)
    parts/<slug>/meta.json      part number, title, manufacturer, source URL, tags
    parts/<slug>/<file>.pdf     original document(s)
    parts/<slug>/<file>.txt     extracted text (grep this instead of opening the PDF)
    parts/<slug>/NOTES.md       curated key specs / project-specific findings

Usage:
    python scripts/datasheet.py find MCP4231          # search metadata + extracted text
    python scripts/datasheet.py add MCP4231 --url https://.../MCP4231.pdf --mfr Microchip \
        --title "7/8-bit digital potentiometer" --tags digipot,spi
    python scripts/datasheet.py add MCP4231 --file C:\\path\\to\\local.pdf
    python scripts/datasheet.py list
    python scripts/datasheet.py reindex                # rebuild INDEX.md, re-extract missing text
"""

from __future__ import annotations

import argparse
import datetime as _dt
import hashlib
import json
import re
import shutil
import sys
import urllib.request
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
LIB_ROOT = REPO_ROOT / "docs" / "datasheets"
PARTS_DIR = LIB_ROOT / "parts"
INDEX_FILE = LIB_ROOT / "INDEX.md"

NOTES_TEMPLATE = """# {part} — Key Notes

Source: {source}

Curated facts used by this project. Cite page/table numbers from the PDF.
Prefer adding findings here over re-reading the full datasheet.

## Key specs

| Parameter | Value | Page |
|-----------|-------|------|

## Project usage / gotchas

"""


def slugify(part: str) -> str:
    slug = re.sub(r"[^A-Za-z0-9._-]+", "-", part.strip()).strip("-")
    if not slug:
        sys.exit("error: part number produces an empty slug")
    return slug.upper()


def load_meta(part_dir: Path) -> dict:
    meta_file = part_dir / "meta.json"
    if meta_file.exists():
        return json.loads(meta_file.read_text(encoding="utf-8"))
    return {}


def save_meta(part_dir: Path, meta: dict) -> None:
    (part_dir / "meta.json").write_text(json.dumps(meta, indent=2) + "\n", encoding="utf-8")


def iter_parts():
    if not PARTS_DIR.exists():
        return
    for part_dir in sorted(PARTS_DIR.iterdir()):
        if part_dir.is_dir():
            yield part_dir, load_meta(part_dir)


def extract_text(pdf: Path) -> Path | None:
    txt = pdf.with_suffix(".txt")
    try:
        from pypdf import PdfReader
    except ImportError:
        print("warning: pypdf not installed (pip install pypdf); skipping text extraction")
        return None
    try:
        reader = PdfReader(str(pdf))
        pages = []
        for i, page in enumerate(reader.pages, start=1):
            pages.append(f"===== page {i} =====\n{page.extract_text() or ''}")
    except Exception as exc:  # noqa: BLE001 - malformed vendor PDFs are common
        print(f"warning: text extraction failed for {pdf.name}: {exc}")
        return None
    txt.write_text("\n".join(pages), encoding="utf-8")
    return txt


def sha256(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1 << 16), b""):
            h.update(chunk)
    return h.hexdigest()


def download(url: str, dest: Path) -> None:
    req = urllib.request.Request(url, headers={"User-Agent": "Mozilla/5.0 (datasheet-library)"})
    with urllib.request.urlopen(req, timeout=60) as resp, dest.open("wb") as out:
        shutil.copyfileobj(resp, out)
    if dest.read_bytes()[:5] != b"%PDF-":
        print(f"warning: {dest.name} does not look like a PDF (vendor may have served an HTML page)")


def cmd_add(args) -> None:
    slug = slugify(args.part)
    part_dir = PARTS_DIR / slug
    part_dir.mkdir(parents=True, exist_ok=True)
    meta = load_meta(part_dir)

    filename = args.name or slug
    if not filename.lower().endswith(".pdf"):
        filename += ".pdf"
    dest = part_dir / filename

    if args.file and Path(args.file).resolve() == dest.resolve():
        pass
    elif dest.exists() and not args.force:
        print(f"already in library: {dest.relative_to(REPO_ROOT)} (use --force to replace)")
    elif args.file:
        src = Path(args.file)
        if not src.exists():
            sys.exit(f"error: {src} not found")
        if src.resolve() != dest.resolve():
            shutil.copy2(src, dest)
    elif args.url:
        print(f"downloading {args.url}")
        download(args.url, dest)
    elif not dest.exists():
        sys.exit("error: provide --url or --file")

    if dest.exists():
        if extract_text(dest):
            print(f"extracted text -> {dest.with_suffix('.txt').relative_to(REPO_ROOT)}")

    docs = {d["file"]: d for d in meta.get("documents", [])}
    doc = docs.get(filename, {"file": filename})
    if args.url:
        doc["url"] = args.url
    if args.doc_type:
        doc["type"] = args.doc_type
    doc.setdefault("type", "datasheet")
    if dest.exists():
        doc["sha256"] = sha256(dest)
    doc["added"] = doc.get("added") or _dt.date.today().isoformat()
    docs[filename] = doc

    meta["part"] = meta.get("part") or args.part
    if args.title:
        meta["title"] = args.title
    if args.mfr:
        meta["manufacturer"] = args.mfr
    if args.tags:
        tags = set(meta.get("tags", [])) | {t.strip().lower() for t in args.tags.split(",") if t.strip()}
        meta["tags"] = sorted(tags)
    if args.used_in:
        used = set(meta.get("used_in", [])) | {u.strip() for u in args.used_in.split(",") if u.strip()}
        meta["used_in"] = sorted(used)
    meta["documents"] = sorted(docs.values(), key=lambda d: d["file"])
    save_meta(part_dir, meta)

    notes = part_dir / "NOTES.md"
    if not notes.exists():
        notes.write_text(NOTES_TEMPLATE.format(part=meta["part"], source=args.url or filename), encoding="utf-8")

    write_index()
    print(f"library entry: {part_dir.relative_to(REPO_ROOT)}")


def cmd_find(args) -> None:
    query = " ".join(args.query)
    pattern = re.compile(re.escape(query), re.IGNORECASE)
    hits = 0
    for part_dir, meta in iter_parts():
        haystack = " ".join(
            [part_dir.name, meta.get("part", ""), meta.get("title", ""), meta.get("manufacturer", "")]
            + meta.get("tags", [])
            + meta.get("used_in", [])
        )
        meta_hit = bool(pattern.search(haystack))
        text_hits = []
        if args.text or not meta_hit:
            for f in sorted(part_dir.glob("*.txt")) + [part_dir / "NOTES.md"]:
                if not f.exists():
                    continue
                for line in f.read_text(encoding="utf-8", errors="replace").splitlines():
                    if pattern.search(line):
                        text_hits.append(f"    {f.name}: {line.strip()[:160]}")
                        if len(text_hits) >= args.max_lines:
                            break
                if len(text_hits) >= args.max_lines:
                    break
        if meta_hit or text_hits:
            hits += 1
            print(f"{meta.get('part', part_dir.name)} — {meta.get('title', '')}  [{part_dir.relative_to(REPO_ROOT)}]")
            for line in text_hits:
                print(line)
    if not hits:
        print(f"no match for '{query}' in library — fetch it, then: python scripts/datasheet.py add <PART> --url <URL>")
        sys.exit(1)


def cmd_list(_args) -> None:
    for part_dir, meta in iter_parts():
        files = ", ".join(d["file"] for d in meta.get("documents", []))
        print(f"{meta.get('part', part_dir.name):<24} {meta.get('manufacturer', ''):<16} {files}")


def write_index() -> None:
    lines = [
        "# Datasheet Library Index",
        "",
        "Generated by `python scripts/datasheet.py reindex` — do not hand-edit.",
        "See [README.md](README.md) for how to search and add documents.",
        "",
        "| Part | Manufacturer | Title | Tags | Used in | Documents | Notes |",
        "|------|--------------|-------|------|---------|-----------|-------|",
    ]
    for part_dir, meta in iter_parts():
        rel = f"parts/{part_dir.name}"
        docs = "<br>".join(
            f"[{d['file']}]({rel}/{d['file']})" + (f" ([txt]({rel}/{Path(d['file']).with_suffix('.txt').name}))"
                                                    if (part_dir / Path(d['file']).with_suffix('.txt').name).exists() else "")
            for d in meta.get("documents", [])
        )
        notes = f"[NOTES]({rel}/NOTES.md)" if (part_dir / "NOTES.md").exists() else ""
        lines.append(
            f"| {meta.get('part', part_dir.name)} | {meta.get('manufacturer', '')} | {meta.get('title', '')} | "
            f"{', '.join(meta.get('tags', []))} | {', '.join(meta.get('used_in', []))} | {docs} | {notes} |"
        )
    INDEX_FILE.write_text("\n".join(lines) + "\n", encoding="utf-8")


def cmd_reindex(_args) -> None:
    for part_dir, _meta in iter_parts():
        for pdf in part_dir.glob("*.pdf"):
            if not pdf.with_suffix(".txt").exists():
                if extract_text(pdf):
                    print(f"extracted {pdf.relative_to(REPO_ROOT)}")
    write_index()
    print(f"wrote {INDEX_FILE.relative_to(REPO_ROOT)}")


def main() -> None:
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            stream.reconfigure(encoding="utf-8", errors="replace")
    parser = argparse.ArgumentParser(description="Local datasheet library")
    sub = parser.add_subparsers(dest="cmd", required=True)

    p = sub.add_parser("find", help="search metadata, notes, and extracted text")
    p.add_argument("query", nargs="+")
    p.add_argument("--text", action="store_true", help="also show text matches for metadata hits")
    p.add_argument("--max-lines", type=int, default=10)
    p.set_defaults(func=cmd_find)

    p = sub.add_parser("add", help="add a document from a URL or local file")
    p.add_argument("part", help="part number, e.g. MCP4231 or ESP32-S3-WROOM-1")
    src = p.add_mutually_exclusive_group()
    src.add_argument("--url")
    src.add_argument("--file")
    p.add_argument("--name", help="stored filename (default: <PART>.pdf); use for app notes/errata")
    p.add_argument("--doc-type", help="datasheet | app-note | errata | reference-manual | user-guide")
    p.add_argument("--title")
    p.add_argument("--mfr")
    p.add_argument("--tags", help="comma-separated")
    p.add_argument("--used-in", help="comma-separated boards/subsystems, e.g. dsp-regulator,front-display-board")
    p.add_argument("--force", action="store_true", help="replace an existing file")
    p.set_defaults(func=cmd_add)

    sub.add_parser("list", help="list library entries").set_defaults(func=cmd_list)
    sub.add_parser("reindex", help="re-extract missing text and rebuild INDEX.md").set_defaults(func=cmd_reindex)

    args = parser.parse_args()
    args.func(args)


if __name__ == "__main__":
    main()
