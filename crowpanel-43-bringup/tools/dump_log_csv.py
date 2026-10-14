#!/usr/bin/env python3
"""Capture LOG_DUMP_CSV output from the CrowPanel USB serial console."""

import argparse
from datetime import datetime
from pathlib import Path
import time


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
        output = args.output or Path(
            f"crowpanel-log-{datetime.now().strftime('%Y%m%d-%H%M%S')}.csv"
        )
        output.write_text("\n".join(lines) + "\n", encoding="utf-8", newline="")
    except (OSError, RuntimeError) as exc:
        parser.exit(1, f"error: {exc}\n")

    print(f"Saved {len(lines)} CSV lines to {output}")


if __name__ == "__main__":
    main()
