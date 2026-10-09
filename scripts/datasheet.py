#!/usr/bin/env python3
"""Local datasheet / reference-doc library for this repo.

Agents and humans should check here BEFORE fetching a document from the web.
The doc-fetch hook (.github/hooks/doc-fetch-cache.json -> scripts/doc_fetch_hook.py)
files any document an agent fetches twice automatically.

Layout (docs/datasheets/):
    INDEX.md                    generated table of every entry (do not hand-edit)
    parts/<PART>/meta.json      part number, title, manufacturer, source URL(s), tags
    parts/<PART>/<file>.pdf     original document(s)
    parts/<PART>/<file>.txt     extracted text with page markers (grep this)
    parts/<PART>/NOTES.md       curated key specs / project-specific findings
    web/<slug>/meta.json        web reference pages (auto-filed by the hook)
    web/<slug>/content_<N>.md   page text as returned by web_fetch (N = start_index)

Usage:
    python scripts/datasheet.py find MCP4231
    python scripts/datasheet.py find "calibration register"
    python scripts/datasheet.py add INA219 --url https://www.ti.com/lit/ds/symlink/ina219.pdf --mfr TI
    python scripts/datasheet.py add MCP4231 --file C:\\path\\to\\local.pdf
    python scripts/datasheet.py list
    python scripts/datasheet.py reindex
"""

from __future__ import annotations

import argparse
import datetime as _dt
import hashlib
import json
import os
import re
import shutil
import sys
import urllib.parse
import urllib.request
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
LIB_ROOT = REPO_ROOT / "docs" / "datasheets"
PARTS_DIR = LIB_ROOT / "parts"
WEB_DIR = LIB_ROOT / "web"
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


class LibraryError(Exception):
    pass


def slugify(text: str, upper: bool = True) -> str:
    slug = re.sub(r"[^A-Za-z0-9._-]+", "-", text.strip()).strip("-.")
    if not slug:
        raise LibraryError(f"'{text}' produces an empty slug")
    slug = slug[:100]
    return slug.upper() if upper else slug.lower()


def normalize_url(url: str) -> str:
    parts = urllib.parse.urlsplit(url.strip())
    path = parts.path.rstrip("/") or "/"
    return urllib.parse.urlunsplit((parts.scheme.lower(), parts.netloc.lower(), path, parts.query, ""))


def rel(path: Path) -> str:
    try:
        return path.relative_to(REPO_ROOT).as_posix()
    except ValueError:
        return str(path)


def load_meta(entry_dir: Path) -> dict:
    meta_file = entry_dir / "meta.json"
    if meta_file.exists():
        return json.loads(meta_file.read_text(encoding="utf-8"))
    return {}


def save_meta(entry_dir: Path, meta: dict) -> None:
    tmp = entry_dir / "meta.json.tmp"
    tmp.write_text(json.dumps(meta, indent=2) + "\n", encoding="utf-8")
    os.replace(tmp, entry_dir / "meta.json")


def iter_entries(kinds=("parts", "web")):
    for kind in kinds:
        root = PARTS_DIR if kind == "parts" else WEB_DIR
        if not root.exists():
            continue
        for entry_dir in sorted(root.iterdir()):
            if entry_dir.is_dir():
                yield entry_dir, load_meta(entry_dir)


def find_by_url(url: str):
    """Return (entry_dir, meta, matching_docs) for a URL already in the library, else None."""
    target = normalize_url(url)
    for entry_dir, meta in iter_entries():
        docs = [d for d in meta.get("documents", []) if d.get("url") and normalize_url(d["url"]) == target]
        if docs:
            return entry_dir, meta, docs
    return None


def extract_text(pdf: Path, quiet: bool = False) -> Path | None:
    txt = pdf.with_suffix(".txt")
    try:
        from pypdf import PdfReader
    except ImportError:
        if not quiet:
            print("warning: pypdf not installed (pip install pypdf); skipping text extraction")
        return None
    try:
        reader = PdfReader(str(pdf))
        pages = [f"===== page {i} =====\n{page.extract_text() or ''}" for i, page in enumerate(reader.pages, start=1)]
    except Exception as exc:  # noqa: BLE001 - malformed vendor PDFs are common
        if not quiet:
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


def download(url: str, dest: Path, timeout: int = 60) -> None:
    req = urllib.request.Request(url, headers={
        "User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/124.0 Safari/537.36",
        "Accept": "application/pdf,*/*;q=0.8",
    })
    tmp = dest.with_suffix(dest.suffix + ".part")
    try:
        with urllib.request.urlopen(req, timeout=timeout) as resp, tmp.open("wb") as out:
            shutil.copyfileobj(resp, out)
    except Exception as exc:
        tmp.unlink(missing_ok=True)
        raise LibraryError(f"download failed for {url}: {exc}") from exc
    if tmp.read_bytes()[:5] != b"%PDF-":
        tmp.unlink()
        raise LibraryError(f"{url} did not return a PDF (vendor may have served an HTML page)")
    os.replace(tmp, dest)


def _merge_list(meta: dict, key: str, values) -> None:
    if values:
        meta[key] = sorted(set(meta.get(key, [])) | {v.strip() for v in values if v and v.strip()})


def _silent(*_):
    pass


def add_document(part: str, url: str | None = None, file: str | None = None, name: str | None = None,
                 doc_type: str | None = None, title: str | None = None, mfr: str | None = None,
                 tags=None, used_in=None, force: bool = False, quiet: bool = False) -> Path:
    """Add (or update) a PDF for a part. Returns the stored PDF path."""
    log = _silent if quiet else print
    slug = slugify(part)
    part_dir = PARTS_DIR / slug
    created = not part_dir.exists()
    part_dir.mkdir(parents=True, exist_ok=True)
    try:
        return _add_document(part_dir, part, url, file, name, doc_type, title, mfr, tags, used_in, force, log)
    except Exception:
        if created:
            shutil.rmtree(part_dir, ignore_errors=True)
        raise


def _add_document(part_dir, part, url, file, name, doc_type, title, mfr, tags, used_in, force, log) -> Path:
    slug = part_dir.name
    meta = load_meta(part_dir)

    filename = name or slug
    if not filename.lower().endswith(".pdf"):
        filename += ".pdf"
    dest = part_dir / filename

    if file and Path(file).resolve() == dest.resolve():
        pass
    elif dest.exists() and not force:
        log(f"already in library: {rel(dest)} (use --force to replace)")
    elif file:
        src = Path(file)
        if not src.exists():
            raise LibraryError(f"{src} not found")
        shutil.copy2(src, dest)
    elif url:
        log(f"downloading {url}")
        download(url, dest)
    elif not dest.exists():
        raise LibraryError("provide --url or --file")

    if dest.exists() and (force or not dest.with_suffix(".txt").exists()):
        if extract_text(dest, quiet=log is _silent):
            log(f"extracted text -> {rel(dest.with_suffix('.txt'))}")

    docs = {d["file"]: d for d in meta.get("documents", [])}
    doc = docs.get(filename, {"file": filename})
    if url:
        doc["url"] = url
    if doc_type:
        doc["type"] = doc_type
    doc.setdefault("type", "datasheet")
    if dest.exists():
        doc["sha256"] = sha256(dest)
    doc["added"] = doc.get("added") or _dt.date.today().isoformat()
    docs[filename] = doc

    meta["part"] = meta.get("part") or part
    if title:
        meta["title"] = title
    if mfr:
        meta["manufacturer"] = mfr
    _merge_list(meta, "tags", [t.lower() for t in (tags or [])])
    _merge_list(meta, "used_in", used_in or [])
    meta["documents"] = sorted(docs.values(), key=lambda d: d["file"])
    save_meta(part_dir, meta)

    notes = part_dir / "NOTES.md"
    if not notes.exists():
        notes.write_text(NOTES_TEMPLATE.format(part=meta["part"], source=url or filename), encoding="utf-8")

    write_index()
    log(f"library entry: {rel(part_dir)}")
    return dest


def web_slug(url: str) -> str:
    parts = urllib.parse.urlsplit(url)
    return slugify(f"{parts.netloc}{parts.path}", upper=False)


def add_web_page(url: str, text: str, start_index: int = 0, raw: bool = False, max_length: int = 5000,
                 tags=None) -> Path:
    """Store a web page chunk as returned by web_fetch. Returns the stored file path."""
    entry = find_by_url(url)
    entry_dir = entry[0] if entry and entry[0].parent == WEB_DIR else WEB_DIR / web_slug(url)
    entry_dir.mkdir(parents=True, exist_ok=True)
    meta = load_meta(entry_dir)

    filename = f"content_{int(start_index)}{'.html' if raw else '.md'}"
    dest = entry_dir / filename
    dest.write_text(text, encoding="utf-8")

    if not meta.get("title"):
        m = re.search(r"^#\s+(.+)$", text, re.MULTILINE)
        meta["title"] = m.group(1).strip()[:120] if m else ""
    meta["part"] = meta.get("part") or urllib.parse.urlsplit(url).netloc
    meta["kind"] = "web"
    docs = {d["file"]: d for d in meta.get("documents", [])}
    docs[filename] = {
        "file": filename,
        "url": url,
        "type": "web-page",
        "start_index": int(start_index),
        "raw": bool(raw),
        "max_length": int(max_length),
        "added": docs.get(filename, {}).get("added") or _dt.date.today().isoformat(),
    }
    meta["documents"] = sorted(docs.values(), key=lambda d: (d.get("start_index", 0), d["file"]))
    _merge_list(meta, "tags", list(tags or []) + ["web"])
    save_meta(entry_dir, meta)
    write_index()
    return dest


def write_index() -> None:
    lines = [
        "# Datasheet Library Index",
        "",
        "Generated by `python scripts/datasheet.py` — do not hand-edit.",
        "See [README.md](README.md) for how to search and add documents.",
        "",
        "## Parts",
        "",
        "| Part | Manufacturer | Title | Tags | Used in | Documents | Notes |",
        "|------|--------------|-------|------|---------|-----------|-------|",
    ]
    for entry_dir, meta in iter_entries(("parts",)):
        r = f"parts/{entry_dir.name}"
        docs = []
        for d in meta.get("documents", []):
            txt = Path(d["file"]).with_suffix(".txt").name
            link = f"[{d['file']}]({r}/{d['file']})"
            if (entry_dir / txt).exists():
                link += f" ([txt]({r}/{txt}))"
            docs.append(link)
        notes = f"[NOTES]({r}/NOTES.md)" if (entry_dir / "NOTES.md").exists() else ""
        lines.append(
            f"| {meta.get('part', entry_dir.name)} | {meta.get('manufacturer', '')} | {meta.get('title', '')} | "
            f"{', '.join(meta.get('tags', []))} | {', '.join(meta.get('used_in', []))} | {'<br>'.join(docs)} | {notes} |"
        )
    lines += ["", "## Web references", "", "| Title | Source URL | Files | Tags |", "|-------|------------|-------|------|"]
    for entry_dir, meta in iter_entries(("web",)):
        r = f"web/{entry_dir.name}"
        docs = meta.get("documents", [])
        url = docs[0].get("url", "") if docs else ""
        files = "<br>".join(f"[{d['file']}]({r}/{d['file']})" for d in docs)
        lines.append(f"| {meta.get('title', '') or entry_dir.name} | {url} | {files} | {', '.join(meta.get('tags', []))} |")
    INDEX_FILE.parent.mkdir(parents=True, exist_ok=True)
    tmp = INDEX_FILE.with_suffix(".md.tmp")
    tmp.write_text("\n".join(lines) + "\n", encoding="utf-8")
    os.replace(tmp, INDEX_FILE)


# ---------------------------------------------------------------- CLI


def _split(value: str | None):
    return [v for v in (value or "").split(",") if v.strip()]


def cmd_add(args) -> None:
    add_document(args.part, url=args.url, file=args.file, name=args.name, doc_type=args.doc_type,
                 title=args.title, mfr=args.mfr, tags=_split(args.tags), used_in=_split(args.used_in),
                 force=args.force)


def cmd_find(args) -> None:
    query = " ".join(args.query)
    pattern = re.compile(re.escape(query), re.IGNORECASE)
    hits = 0
    for entry_dir, meta in iter_entries():
        haystack = " ".join(
            [entry_dir.name, meta.get("part", ""), meta.get("title", ""), meta.get("manufacturer", "")]
            + meta.get("tags", []) + meta.get("used_in", [])
            + [d.get("url", "") for d in meta.get("documents", [])]
        )
        meta_hit = bool(pattern.search(haystack))
        text_hits = []
        if args.text or not meta_hit:
            files = sorted(entry_dir.glob("*.txt")) + sorted(entry_dir.glob("*.md"))
            for f in files:
                for line in f.read_text(encoding="utf-8", errors="replace").splitlines():
                    if pattern.search(line):
                        text_hits.append(f"    {f.name}: {line.strip()[:160]}")
                        if len(text_hits) >= args.max_lines:
                            break
                if len(text_hits) >= args.max_lines:
                    break
        if meta_hit or text_hits:
            hits += 1
            print(f"{meta.get('part', entry_dir.name)} — {meta.get('title', '')}  [{rel(entry_dir)}]")
            for line in text_hits:
                print(line)
    if not hits:
        print(f"no match for '{query}' in library — fetch it, then: python scripts/datasheet.py add <PART> --url <URL>")
        sys.exit(1)


def cmd_list(_args) -> None:
    for entry_dir, meta in iter_entries():
        files = ", ".join(d["file"] for d in meta.get("documents", []))
        print(f"{rel(entry_dir):<48} {meta.get('title', '')[:40]:<40} {files}")


def cmd_reindex(_args) -> None:
    for entry_dir, _meta in iter_entries(("parts",)):
        for pdf in entry_dir.glob("*.pdf"):
            if not pdf.with_suffix(".txt").exists() and extract_text(pdf):
                print(f"extracted {rel(pdf)}")
    write_index()
    print(f"wrote {rel(INDEX_FILE)}")


def main() -> None:
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            stream.reconfigure(encoding="utf-8", errors="replace")
    parser = argparse.ArgumentParser(description="Local datasheet library")
    sub = parser.add_subparsers(dest="cmd", required=True)

    p = sub.add_parser("find", help="search metadata, URLs, notes, and extracted text")
    p.add_argument("query", nargs="+")
    p.add_argument("--text", action="store_true", help="also show text matches for metadata hits")
    p.add_argument("--max-lines", type=int, default=10)
    p.set_defaults(func=cmd_find)

    p = sub.add_parser("add", help="add a PDF from a URL or local file")
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
    try:
        args.func(args)
    except LibraryError as exc:
        sys.exit(f"error: {exc}")


if __name__ == "__main__":
    main()
