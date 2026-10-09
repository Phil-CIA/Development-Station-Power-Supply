#!/usr/bin/env python3
"""Copilot CLI hook: auto-file documents that agents fetch more than once.

Wired up by .github/hooks/doc-fetch-cache.json for `web_fetch` and shell
(`powershell` / `bash`) downloads.

  1st fetch of a URL  -> allowed; result is staged in a machine-wide cache
                         (~/.copilot/doc-fetch-cache, shared by all worktrees/sessions)
  2nd fetch           -> the document is filed into docs/datasheets/ (PDFs downloaded
                         and text-extracted, web pages saved as markdown) and the fetch
                         is denied with a pointer to the local copy
  any later fetch     -> denied with a pointer to the local copy

Append "#refetch" to a URL to bypass the hook for a deliberate fresh copy.

Environment overrides:
  DOC_FETCH_THRESHOLD  fetch count that triggers filing (default 2)
  DOC_FETCH_HOME       staging/log directory (default ~/.copilot/doc-fetch-cache)
  DOC_FETCH_DISABLE=1  turn the hook off

The hook must never break tool calls: every error is swallowed and the tool is allowed.
"""

from __future__ import annotations

import hashlib
import json
import os
import re
import sys
import time
import urllib.parse
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
lib = None  # datasheet module, imported lazily so non-download tool calls exit fast

THRESHOLD = max(1, int(os.environ.get("DOC_FETCH_THRESHOLD", "2")))
STATE_DIR = Path(os.environ.get("DOC_FETCH_HOME") or Path.home() / ".copilot" / "doc-fetch-cache")
LOG_FILE = STATE_DIR / "fetch-log.json"
STAGE_DIR = STATE_DIR / "staged"
REFETCH_MARKER = "#refetch"
DEFAULT_MAX_LENGTH = 5000

SKIP_URL = re.compile(
    r"^https?://(localhost|127\.|\[::1\]|0\.0\.0\.0"
    r"|api\.github\.com|uploads\.github\.com"
    r"|github\.com/[^/]+/[^/]+/(pull|pulls|issues|actions|compare|commit|commits|runs|checks|security)\b"
    r"|[^/]*\.?githubusercontent\.com/.*/(pull|issues)/)",
    re.IGNORECASE,
)
SHELL_DOWNLOAD = re.compile(
    r"\b(curl|wget|Invoke-WebRequest|iwr|Invoke-RestMethod|irm|Start-BitsTransfer|DownloadFile)\b",
    re.IGNORECASE,
)
URL_IN_TEXT = re.compile(r"https?://[^\s'\"<>|`)]+", re.IGNORECASE)
GENERIC_STEMS = {"download", "datasheet", "ds", "pdf", "file", "document", "view", "getfile", "index"}

LIB_NOTE = ("Library files are part of the repo: commit them on your branch. Improve the entry with "
            "`python scripts/datasheet.py add <PART> --title ... --mfr ... --tags ...` and record key "
            "specs in NOTES.md.")


# ---------------------------------------------------------------- state


class _Lock:
    def __init__(self, path: Path, timeout: float = 3.0):
        self.path, self.timeout, self.fd = path, timeout, None

    def __enter__(self):
        deadline = time.time() + self.timeout
        while True:
            try:
                self.fd = os.open(self.path, os.O_CREAT | os.O_EXCL | os.O_WRONLY)
                return self
            except FileExistsError:
                try:
                    if time.time() - self.path.stat().st_mtime > 30:
                        self.path.unlink()
                        continue
                except FileNotFoundError:
                    continue
                if time.time() > deadline:
                    return self
                time.sleep(0.05)

    def __exit__(self, *exc):
        if self.fd is not None:
            os.close(self.fd)
            try:
                self.path.unlink()
            except FileNotFoundError:
                pass


def _read_log() -> dict:
    try:
        return json.loads(LOG_FILE.read_text(encoding="utf-8"))
    except (FileNotFoundError, ValueError):
        return {}


def bump(url: str, tool: str) -> int:
    STATE_DIR.mkdir(parents=True, exist_ok=True)
    key = lib.normalize_url(url)
    with _Lock(STATE_DIR / "fetch-log.lock"):
        log = _read_log()
        now = time.strftime("%Y-%m-%dT%H:%M:%S")
        rec = log.setdefault(key, {"url": url, "count": 0, "first": now, "tools": []})
        rec["count"] += 1
        rec["last"] = now
        if tool not in rec["tools"]:
            rec["tools"].append(tool)
        tmp = LOG_FILE.with_suffix(".tmp")
        tmp.write_text(json.dumps(log, indent=1), encoding="utf-8")
        os.replace(tmp, LOG_FILE)
    return rec["count"]


def fetch_count(url: str) -> int:
    return _read_log().get(lib.normalize_url(url), {}).get("count", 0)


def stage_path(url: str, start: int, raw: bool) -> Path:
    h = hashlib.sha1(f"{lib.normalize_url(url)}|{start}|{int(raw)}".encode()).hexdigest()[:20]
    return STAGE_DIR / f"{h}.json"


# ---------------------------------------------------------------- helpers


def is_pdf_url(url: str) -> bool:
    return urllib.parse.urlsplit(url).path.lower().endswith(".pdf")


def part_from_pdf_url(url: str) -> str:
    stem = Path(urllib.parse.unquote(urllib.parse.urlsplit(url).path)).stem
    if len(stem) < 3 or stem.lower() in GENERIC_STEMS:
        return lib.web_slug(url)
    return stem


def deny(reason: str) -> None:
    print(json.dumps({"permissionDecision": "deny", "permissionDecisionReason": reason}))


def pointer_for(entry_dir: Path, files) -> str:
    paths = [lib.rel(entry_dir / f) for f in files]
    if entry_dir.parent == lib.PARTS_DIR:
        txts = [lib.rel(entry_dir / Path(f).with_suffix(".txt").name) for f in files
                if (entry_dir / Path(f).with_suffix(".txt").name).exists()]
        notes = lib.rel(entry_dir / "NOTES.md")
        return (f"read {notes} first, then the extracted text {', '.join(txts) or '(none)'}; "
                f"original PDF: {', '.join(paths)}")
    return f"read {', '.join(paths)}"


def file_pdf(url: str) -> Path | None:
    try:
        return lib.add_document(part_from_pdf_url(url), url=url, tags=["auto-filed"], quiet=True)
    except Exception:  # noqa: BLE001 - network/vendor failures: let the tool run instead
        return None


def handle_pdf_pre(url: str, tool: str) -> bool:
    """Returns True if the tool call was denied."""
    hit = lib.find_by_url(url)
    if hit:
        entry_dir, _meta, docs = hit
        deny(f"Doc-library hook: {url} is already filed locally — {pointer_for(entry_dir, [d['file'] for d in docs])}. "
             f"Do not re-download it. (Append '{REFETCH_MARKER}' to the URL only if a fresh copy is truly required.)")
        return True
    if bump(url, tool) >= THRESHOLD:
        dest = file_pdf(url)
        if dest:
            deny(f"Doc-library hook: this PDF has now been fetched {THRESHOLD}+ times, so it was filed locally instead — "
                 f"{pointer_for(dest.parent, [dest.name])}. {LIB_NOTE}")
            return True
    return False


# ---------------------------------------------------------------- events


def pre_web_fetch(args: dict) -> None:
    url = str(args.get("url") or "")
    if not url or REFETCH_MARKER in url or SKIP_URL.match(url):
        return
    if is_pdf_url(url):
        handle_pdf_pre(url, "web_fetch")
        return

    start = int(args.get("start_index") or 0)
    raw = bool(args.get("raw"))
    want = int(args.get("max_length") or DEFAULT_MAX_LENGTH)
    hit = lib.find_by_url(url)
    if hit:
        entry_dir, _meta, docs = hit
        if entry_dir.parent == lib.PARTS_DIR:
            deny(f"Doc-library hook: {url} is already filed locally — {pointer_for(entry_dir, [d['file'] for d in docs])}.")
            return
        for d in docs:
            if (d.get("start_index", 0) == start and bool(d.get("raw")) == raw
                    and d.get("max_length", DEFAULT_MAX_LENGTH) >= want and (entry_dir / d["file"]).exists()):
                deny(f"Doc-library hook: this page (start_index={start}) is already filed locally — "
                     f"{pointer_for(entry_dir, [d['file']])}. Use start_index to page beyond it. "
                     f"(Append '{REFETCH_MARKER}' to the URL only if a fresh copy is truly required.)")
                return
        return  # new chunk of a filed page: allow; post hook files it

    if bump(url, "web_fetch") >= THRESHOLD:
        staged = stage_path(url, start, raw)
        if staged.exists():
            data = json.loads(staged.read_text(encoding="utf-8"))
            if data.get("max_length", DEFAULT_MAX_LENGTH) >= want:
                dest = lib.add_web_page(url, data["text"], start, raw, data.get("max_length", DEFAULT_MAX_LENGTH),
                                        tags=["auto-filed"])
                deny(f"Doc-library hook: this page has now been fetched {THRESHOLD}+ times, so the earlier result was "
                     f"filed locally instead — read {lib.rel(dest)}. {LIB_NOTE}")


def pre_shell(args: dict) -> None:
    command = str(args.get("command") or "")
    if not SHELL_DOWNLOAD.search(command):
        return
    for url in URL_IN_TEXT.findall(command):
        url = url.rstrip(".,;")
        if REFETCH_MARKER in url or SKIP_URL.match(url) or not is_pdf_url(url):
            continue
        if handle_pdf_pre(url, "shell"):
            return


def post_web_fetch(args: dict, payload: dict) -> None:
    url = str(args.get("url") or "")
    if not url or REFETCH_MARKER in url or SKIP_URL.match(url) or is_pdf_url(url):
        return
    result = payload.get("toolResult") or payload.get("tool_result") or {}
    text = result.get("textResultForLlm") or result.get("text_result_for_llm") or ""
    if not text.strip():
        return
    start = int(args.get("start_index") or 0)
    raw = bool(args.get("raw"))
    max_length = int(args.get("max_length") or DEFAULT_MAX_LENGTH)

    STAGE_DIR.mkdir(parents=True, exist_ok=True)
    stage_path(url, start, raw).write_text(
        json.dumps({"url": url, "start_index": start, "raw": raw, "max_length": max_length, "text": text}),
        encoding="utf-8")

    hit = lib.find_by_url(url)
    if (hit and hit[0].parent == lib.WEB_DIR) or (not hit and fetch_count(url) >= THRESHOLD):
        dest = lib.add_web_page(url, text, start, raw, max_length, tags=["auto-filed"])
        print(json.dumps({"additionalContext": f"Doc-library hook: this page is now filed at {lib.rel(dest)}; "
                                                f"read it there next time. {LIB_NOTE}"}))


def main() -> None:
    if os.environ.get("DOC_FETCH_DISABLE") == "1" or len(sys.argv) < 2:
        return
    payload = json.loads(sys.stdin.buffer.read().decode("utf-8", errors="replace") or "{}")
    tool = payload.get("toolName") or payload.get("tool_name") or ""
    args = payload.get("toolArgs") if "toolArgs" in payload else payload.get("tool_input")
    if isinstance(args, str):
        try:
            args = json.loads(args)
        except ValueError:
            args = {"command": args}
    if not isinstance(args, dict):
        return

    global lib
    if tool == "web_fetch" or (tool in ("powershell", "bash") and SHELL_DOWNLOAD.search(str(args.get("command") or ""))):
        import datasheet as lib
    else:
        return

    if sys.argv[1] == "pre":
        if tool == "web_fetch":
            pre_web_fetch(args)
        elif tool in ("powershell", "bash"):
            pre_shell(args)
    elif sys.argv[1] == "post" and tool == "web_fetch":
        post_web_fetch(args, payload)


if __name__ == "__main__":
    try:
        main()
    except Exception as exc:  # noqa: BLE001 - never block a tool because of this hook
        try:
            STATE_DIR.mkdir(parents=True, exist_ok=True)
            with (STATE_DIR / "hook-errors.log").open("a", encoding="utf-8") as f:
                f.write(f"{time.strftime('%Y-%m-%dT%H:%M:%S')} {sys.argv[1:]} {exc!r}\n")
        except Exception:  # noqa: BLE001
            pass
    sys.exit(0)
