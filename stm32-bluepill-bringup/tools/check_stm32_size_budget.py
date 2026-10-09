#!/usr/bin/env python3
"""Check STM32 Blue Pill image and RAM budgets from PlatformIO build artifacts.

This checker enforces flash acceptance using the actual programmed image size
(`firmware.bin`) and reports PlatformIO's static RAM metric separately.
"""

from __future__ import annotations

import argparse
import json
import pathlib
import re
import struct
import sys
from dataclasses import dataclass


class BudgetCheckError(RuntimeError):
    """Raised when required inputs are missing, malformed, or over budget."""


@dataclass(frozen=True)
class PlatformIOMetric:
    kind: str
    percent: float
    used_bytes: int
    capacity_bytes: int


def _read_text(path: pathlib.Path) -> str:
    try:
        return path.read_text(encoding="utf-8", errors="replace")
    except OSError as exc:
        raise BudgetCheckError(f"unable to read file: {path}: {exc}") from exc


def _require_file(path: pathlib.Path, label: str) -> None:
    if not path.exists():
        raise BudgetCheckError(f"required {label} missing: {path}")
    if not path.is_file():
        raise BudgetCheckError(f"required {label} is not a file: {path}")
    if path.stat().st_size <= 0:
        raise BudgetCheckError(f"required {label} is empty: {path}")


def parse_platformio_metric(log_text: str, metric: str) -> PlatformIOMetric:
    # Matches lines like:
    # Flash: [====== ]  68.6% (used 44956 bytes from 65536 bytes)
    pattern = re.compile(
        rf"{metric}:\s*\[[^\]]*\]\s*([0-9]+(?:\.[0-9]+)?)%\s*"
        r"\(used\s*([0-9][0-9,]*)\s*bytes\s*from\s*([0-9][0-9,]*)\s*bytes\)",
        flags=re.IGNORECASE,
    )
    match = pattern.search(log_text)
    if not match:
        raise BudgetCheckError(f"missing or malformed PlatformIO {metric} metric in build log")

    percent = float(match.group(1))
    used = int(match.group(2).replace(",", ""))
    capacity = int(match.group(3).replace(",", ""))
    return PlatformIOMetric(metric.lower(), percent, used, capacity)


def parse_elf_flash_span(elf_path: pathlib.Path, flash_base: int = 0x08000000) -> int:
    data = elf_path.read_bytes()
    if len(data) < 52:
        raise BudgetCheckError(f"malformed ELF (too small): {elf_path}")
    if data[0:4] != b"\x7fELF":
        raise BudgetCheckError(f"malformed ELF (bad magic): {elf_path}")

    elf_class = data[4]
    elf_endian = data[5]
    if elf_class != 1:
        raise BudgetCheckError(f"unsupported ELF class (need 32-bit): {elf_class}")
    if elf_endian != 1:
        raise BudgetCheckError("unsupported ELF endianness (need little-endian)")

    e_phoff = struct.unpack_from("<I", data, 28)[0]
    e_phentsize = struct.unpack_from("<H", data, 42)[0]
    e_phnum = struct.unpack_from("<H", data, 44)[0]

    if e_phnum <= 0 or e_phentsize <= 0:
        raise BudgetCheckError("malformed ELF (missing program headers)")

    min_addr = None
    max_end = None

    for i in range(e_phnum):
        off = e_phoff + i * e_phentsize
        if off + 32 > len(data):
            raise BudgetCheckError("malformed ELF (truncated program header table)")

        p_type, p_offset, p_vaddr, p_paddr, p_filesz, _p_memsz, _p_flags, _p_align = struct.unpack_from(
            "<IIIIIIII", data, off
        )
        if p_type != 1 or p_filesz <= 0:
            continue

        # Keep only FLASH-resident load segments for programmed image span.
        if p_paddr < flash_base:
            continue

        if p_offset + p_filesz > len(data):
            raise BudgetCheckError("malformed ELF (segment exceeds file length)")

        seg_start = p_paddr
        seg_end = p_paddr + p_filesz
        min_addr = seg_start if min_addr is None else min(min_addr, seg_start)
        max_end = seg_end if max_end is None else max(max_end, seg_end)

    if min_addr is None or max_end is None:
        raise BudgetCheckError("unable to find FLASH load segments in ELF")
    if max_end <= min_addr:
        raise BudgetCheckError("malformed ELF FLASH span")

    return max_end - min_addr


def parse_map_sections(map_text: str) -> dict[str, int]:
    # Parse GNU ld map output section summary lines.
    # Example: " .text          0x0800010c    0xae90"
    sec_re = re.compile(r"^\s*\.(\S+)\s+0x[0-9a-fA-F]+\s+0x([0-9a-fA-F]+)\b")
    sections: dict[str, int] = {}
    for line in map_text.splitlines():
        match = sec_re.match(line)
        if not match:
            continue
        name = f".{match.group(1)}"
        size = int(match.group(2), 16)
        # Keep first top-level section record encountered.
        sections.setdefault(name, size)

    if not sections:
        raise BudgetCheckError("missing or malformed linker map section table")

    return sections


def evaluate_budget(
    *,
    env: str,
    build_dir: pathlib.Path,
    build_log: pathlib.Path,
    flash_capacity_bytes: int,
    flash_limit_bytes: int,
) -> dict:
    if flash_limit_bytes > flash_capacity_bytes:
        raise BudgetCheckError(
            f"invalid limit: {flash_limit_bytes} exceeds flash capacity {flash_capacity_bytes}"
        )

    bin_path = build_dir / "firmware.bin"
    elf_path = build_dir / "firmware.elf"
    map_path = build_dir / "firmware.map"

    _require_file(bin_path, "firmware binary")
    _require_file(elf_path, "firmware ELF")
    _require_file(map_path, "firmware map")
    _require_file(build_log, "build log")

    bin_size = bin_path.stat().st_size

    log_text = _read_text(build_log)
    ram_metric = parse_platformio_metric(log_text, "RAM")
    flash_metric = parse_platformio_metric(log_text, "Flash")

    elf_span = parse_elf_flash_span(elf_path)
    if elf_span != bin_size:
        raise BudgetCheckError(
            "firmware.bin size does not match ELF FLASH load span "
            f"(bin={bin_size}, elf_span={elf_span})"
        )

    map_sections = parse_map_sections(_read_text(map_path))

    flash_used = bin_size
    flash_free = flash_capacity_bytes - flash_used
    within_capacity = flash_used <= flash_capacity_bytes
    within_limit = flash_used <= flash_limit_bytes

    if not within_capacity:
        raise BudgetCheckError(
            f"{env}: complete image exceeds capacity: used={flash_used} capacity={flash_capacity_bytes}"
        )

    if not within_limit:
        raise BudgetCheckError(
            f"{env}: complete image exceeds limit: used={flash_used} limit={flash_limit_bytes}"
        )

    return {
        "env": env,
        "artifacts": {
            "firmware_bin": str(bin_path),
            "firmware_elf": str(elf_path),
            "firmware_map": str(map_path),
            "build_log": str(build_log),
        },
        "flash": {
            "measurement_source": "firmware.bin byte length",
            "used_bytes": flash_used,
            "free_bytes": flash_free,
            "capacity_bytes": flash_capacity_bytes,
            "limit_bytes": flash_limit_bytes,
            "elf_flash_span_bytes": elf_span,
            "within_capacity": within_capacity,
            "within_limit": within_limit,
        },
        "platformio_metrics": {
            "flash": {
                "used_bytes": flash_metric.used_bytes,
                "capacity_bytes": flash_metric.capacity_bytes,
                "percent": flash_metric.percent,
            },
            "ram": {
                "used_bytes": ram_metric.used_bytes,
                "capacity_bytes": ram_metric.capacity_bytes,
                "percent": ram_metric.percent,
            },
        },
        "map_sections": map_sections,
    }


def _render_report(report: dict) -> str:
    flash = report["flash"]
    ram = report["platformio_metrics"]["ram"]
    pio_flash = report["platformio_metrics"]["flash"]
    lines = [
        f"STM32 size budget report: {report['env']}",
        (
            "Complete image flash: "
            f"used={flash['used_bytes']} free={flash['free_bytes']} "
            f"capacity={flash['capacity_bytes']} limit={flash['limit_bytes']} bytes"
        ),
        (
            "PlatformIO flash metric (.text/.data style summary): "
            f"used={pio_flash['used_bytes']} from={pio_flash['capacity_bytes']} "
            f"({pio_flash['percent']:.1f}%)"
        ),
        (
            "PlatformIO static RAM metric: "
            f"used={ram['used_bytes']} from={ram['capacity_bytes']} ({ram['percent']:.1f}%)"
        ),
        (
            "ELF cross-check: "
            f"firmware.bin={flash['used_bytes']} bytes, elf_flash_span={flash['elf_flash_span_bytes']} bytes"
        ),
    ]
    return "\n".join(lines) + "\n"


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--env", required=True)
    parser.add_argument("--build-dir", required=True)
    parser.add_argument("--build-log", required=True)
    parser.add_argument("--flash-capacity-bytes", type=int, default=65536)
    parser.add_argument("--flash-limit-bytes", type=int, required=True)
    parser.add_argument("--report-text", required=True)
    parser.add_argument("--report-json", required=True)
    args = parser.parse_args(argv)

    build_dir = pathlib.Path(args.build_dir)
    build_log = pathlib.Path(args.build_log)
    report_text = pathlib.Path(args.report_text)
    report_json = pathlib.Path(args.report_json)

    try:
        report = evaluate_budget(
            env=args.env,
            build_dir=build_dir,
            build_log=build_log,
            flash_capacity_bytes=args.flash_capacity_bytes,
            flash_limit_bytes=args.flash_limit_bytes,
        )
        text = _render_report(report)

        report_text.parent.mkdir(parents=True, exist_ok=True)
        report_json.parent.mkdir(parents=True, exist_ok=True)
        report_text.write_text(text, encoding="utf-8")
        report_json.write_text(json.dumps(report, indent=2, sort_keys=True), encoding="utf-8")

        print(text, end="")
        return 0
    except BudgetCheckError as exc:
        print(f"STM32 budget check failed: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
