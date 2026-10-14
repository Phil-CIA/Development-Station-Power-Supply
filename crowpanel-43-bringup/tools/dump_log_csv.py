#!/usr/bin/env python3
"""Capture LOG_DUMP_CSV output from the CrowPanel USB serial console."""

import argparse
from datetime import datetime
from pathlib import Path
import time

EXPECTED_HEADER = (
    "idx,t_ms,seq,v12_mV,v12_V,i12_mA,i12_A,rx_count,err_count,uart_frames,"
    "v3v3_mV,v3v3_V,i3v3_mA,i3v3_A,has_ch2,demo"
)
UINT32_MAX = (1 << 32) - 1
DEFAULT_SAMPLE_PERIOD_MS = 200
MAX_SAMPLE_PERIOD_GAP_MULTIPLIER = 500
MIN_ALLOWED_SAMPLE_GAP_MS = 60000


def _parse_metadata(lines):
    metadata = {}
    for line in lines:
        if not line.startswith("# "):
            continue
        content = line[2:]
        if "=" not in content:
            continue
        key, value = content.split("=", 1)
        metadata.setdefault(key.strip(), []).append(value.strip())
    return metadata


def validate_csv_lines(lines):
    if not lines:
        raise RuntimeError("CSV block was empty; no CSV file was written.")

    if EXPECTED_HEADER not in lines:
        raise RuntimeError("CSV header missing or malformed; no CSV file was written.")

    header_index = lines.index(EXPECTED_HEADER)
    data_lines = lines[header_index + 1 :]
    for line in lines[:header_index]:
        if not line.startswith("# "):
            raise RuntimeError(
                "Unexpected non-metadata content before CSV header; "
                "no CSV file was written."
            )

    metadata = _parse_metadata(lines[:header_index])
    exported_entries = metadata.get("exported_samples", [])
    if len(exported_entries) != 1:
        raise RuntimeError(
            "Expected exactly one exported_samples metadata entry before CSV header; "
            "no CSV file was written."
        )
    try:
        expected_count = int(exported_entries[0])
    except ValueError as exc:
        raise RuntimeError(
            f"Invalid exported_samples metadata value: {exported_entries[0]!r}; "
            "no CSV file was written."
        ) from exc
    if expected_count < 0:
        raise RuntimeError("exported_samples must be non-negative; no CSV file was written.")
    if expected_count != len(data_lines):
        raise RuntimeError(
            "exported_samples metadata does not match parsed row count; "
            "no CSV file was written."
        )

    sample_period_ms = DEFAULT_SAMPLE_PERIOD_MS
    sample_period_entries = metadata.get("sample_period_ms", [])
    if sample_period_entries:
        try:
            sample_period_ms = int(sample_period_entries[-1])
        except ValueError as exc:
            raise RuntimeError(
                f"Invalid sample_period_ms metadata value: {sample_period_entries[-1]!r}; "
                "no CSV file was written."
            ) from exc
        if sample_period_ms <= 0:
            raise RuntimeError("sample_period_ms must be greater than zero; no CSV file was written.")
    max_allowed_step = max(
        sample_period_ms * MAX_SAMPLE_PERIOD_GAP_MULTIPLIER, MIN_ALLOWED_SAMPLE_GAP_MS
    )

    prev_t_ms = None
    for row_no, line in enumerate(data_lines, start=1):
        cells = line.split(",")
        if len(cells) != 16:
            raise RuntimeError(
                f"Malformed row {row_no}: expected 16 fields, got {len(cells)}; "
                "no CSV file was written."
            )

        try:
            idx = int(cells[0])
            t_ms = int(cells[1])
            int(cells[2])  # seq
            int(cells[3])  # v12_mV
            float(cells[4])  # v12_V
            int(cells[5])  # i12_mA
            float(cells[6])  # i12_A
            int(cells[7])  # rx_count
            int(cells[8])  # err_count
            int(cells[9])  # uart_frames
            has_ch2 = int(cells[14])
            demo = int(cells[15])
        except ValueError as exc:
            raise RuntimeError(
                f"Malformed row {row_no}: expected numeric values; "
                "no CSV file was written."
            ) from exc

        expected_idx = row_no - 1
        if idx != expected_idx:
            raise RuntimeError(
                f"Malformed row {row_no}: expected idx {expected_idx}, got {idx}; "
                "no CSV file was written."
            )
        if not 0 <= t_ms <= UINT32_MAX:
            raise RuntimeError(
                f"Malformed row {row_no}: t_ms out of uint32 range ({t_ms}); "
                "no CSV file was written."
            )
        if prev_t_ms is not None:
            forward_delta = (t_ms - prev_t_ms) & UINT32_MAX
            if forward_delta == 0:
                raise RuntimeError(
                    f"Malformed row {row_no}: timestamp did not advance ({t_ms}); "
                    "no CSV file was written."
                )
            if forward_delta > max_allowed_step:
                raise RuntimeError(
                    f"Malformed row {row_no}: timestamp step {forward_delta} ms exceeds "
                    f"allowed window {max_allowed_step} ms (sample_period_ms={sample_period_ms}); "
                    "no CSV file was written."
                )
            if t_ms < prev_t_ms and prev_t_ms < UINT32_MAX - max_allowed_step:
                raise RuntimeError(
                    f"Malformed row {row_no}: timestamp rollover not plausible at {t_ms}; "
                    "no CSV file was written."
                )
        prev_t_ms = t_ms

        if has_ch2 not in (0, 1):
            raise RuntimeError(
                f"Malformed row {row_no}: has_ch2 must be 0 or 1; "
                "no CSV file was written."
            )
        if demo not in (0, 1):
            raise RuntimeError(
                f"Malformed row {row_no}: demo must be 0 or 1; "
                "no CSV file was written."
            )

        ch2_cells = cells[10:14]
        if has_ch2 == 0 and any(cell != "" for cell in ch2_cells):
            raise RuntimeError(
                f"Malformed row {row_no}: has_ch2=0 requires empty CH2 fields; "
                "no CSV file was written."
            )
        if has_ch2 == 1:
            try:
                int(cells[10])
                float(cells[11])
                int(cells[12])
                float(cells[13])
            except ValueError as exc:
                raise RuntimeError(
                    f"Malformed row {row_no}: has_ch2=1 requires numeric CH2 fields; "
                    "no CSV file was written."
                ) from exc


def capture_csv(port, command, timeout_seconds, startup_delay):
    try:
        import serial
    except ImportError as exc:
        raise RuntimeError(
            "pyserial is required; install it with: python -m pip install pyserial"
        ) from exc

    lines = []
    started = False
    with serial.Serial(port, 115200, timeout=1) as device:
        if startup_delay:
            time.sleep(startup_delay)
        device.reset_input_buffer()
        device.write((command + "\n").encode("ascii"))
        deadline = time.monotonic() + timeout_seconds

        while time.monotonic() < deadline:
            raw_line = device.readline()
            if not raw_line:
                continue
            line = raw_line.decode("utf-8", errors="replace").strip()
            if not started:
                if line == "csv: no samples":
                    raise RuntimeError("The trend buffer is empty; no CSV file was written.")
                if line == "csv_begin":
                    started = True
                continue
            if line == "csv_end":
                return lines
            lines.append(line)

    if not started:
        raise RuntimeError("Timed out waiting for csv_begin; no CSV file was written.")
    raise RuntimeError("Timed out waiting for csv_end; no CSV file was written.")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("port", help="CrowPanel USB serial port (for example COM12)")
    parser.add_argument(
        "--limit",
        type=int,
        default=0,
        help="Export only the newest N samples; 0 exports the full ring buffer.",
    )
    parser.add_argument(
        "--output",
        type=Path,
        help="Output CSV path (default: crowpanel-log-<timestamp>.csv).",
    )
    parser.add_argument(
        "--timeout",
        type=float,
        default=120,
        help="Seconds to wait for the complete export (default: 120).",
    )
    parser.add_argument(
        "--startup-delay",
        type=float,
        default=2,
        help="Seconds to allow the board to boot after opening the port (default: 2).",
    )
    args = parser.parse_args()

    if args.limit < 0:
        parser.error("--limit must be zero or greater")
    if args.timeout <= 0:
        parser.error("--timeout must be greater than zero")
    if args.startup_delay < 0:
        parser.error("--startup-delay must be zero or greater")

    command = "LOG_DUMP_CSV" + (f" {args.limit}" if args.limit else "")
    try:
        lines = capture_csv(args.port, command, args.timeout, args.startup_delay)
        validate_csv_lines(lines)
        output = args.output or Path(
            f"crowpanel-log-{datetime.now().strftime('%Y%m%d-%H%M%S')}.csv"
        )
        output.write_text("\n".join(lines) + "\n", encoding="utf-8", newline="")
    except (OSError, RuntimeError) as exc:
        parser.exit(1, f"error: {exc}\n")

    print(f"Saved {len(lines)} CSV lines to {output}")


if __name__ == "__main__":
    main()
