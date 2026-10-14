import pathlib
import sys
import tempfile
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))

import dump_log_csv


class _FakeSerialPort:
    def __init__(self, *, lines=None, open_error=None):
        self._lines = list(lines or [])
        self._index = 0
        self._open_error = open_error
        self.writes = []

    def __enter__(self):
        if self._open_error is not None:
            raise self._open_error
        return self

    def __exit__(self, exc_type, exc, tb):
        return False

    def reset_input_buffer(self):
        return None

    def write(self, data):
        self.writes.append(data)

    def readline(self):
        if self._index < len(self._lines):
            line = self._lines[self._index]
            self._index += 1
            return line
        return b""


class _FakeSerialModule:
    def __init__(self, *, lines=None, open_error=None):
        self._lines = lines
        self._open_error = open_error

    def Serial(self, *args, **kwargs):
        return _FakeSerialPort(lines=self._lines, open_error=self._open_error)


def _base_lines(*rows, sample_period_ms=200, exported_samples=None):
    if exported_samples is None:
        exported_samples = len(rows)
    lines = [
        f"# sample_period_ms={sample_period_ms}",
        f"# exported_samples={exported_samples}",
        dump_log_csv.EXPECTED_HEADER,
    ]
    lines.extend(rows)
    return lines


class DumpLogCsvValidationTests(unittest.TestCase):
    def test_accepts_valid_rows(self):
        lines = _base_lines(
            "0,1000,1,5000,5.000,-20,-0.020,1,0,1,3300,3.300,-10,-0.010,1,0",
            "1,1200,2,5001,5.001,-21,-0.021,2,0,2,,,,,0,1",
        )
        dump_log_csv.validate_csv_lines(lines)

    def test_rejects_unexpected_non_metadata_line_before_header(self):
        lines = [
            "# sample_period_ms=200",
            "console noise before header",
            "# exported_samples=1",
            dump_log_csv.EXPECTED_HEADER,
            "0,1000,1,5000,5.000,0,0.000,1,0,1,3300,3.300,0,0.000,1,0",
        ]
        with self.assertRaisesRegex(RuntimeError, "Unexpected non-metadata content before CSV header"):
            dump_log_csv.validate_csv_lines(lines)

    def test_requires_exactly_one_exported_samples_entry(self):
        missing = [
            "# sample_period_ms=200",
            dump_log_csv.EXPECTED_HEADER,
            "0,1000,1,5000,5.000,0,0.000,1,0,1,3300,3.300,0,0.000,1,0",
        ]
        duplicate = [
            "# sample_period_ms=200",
            "# exported_samples=1",
            "# exported_samples=1",
            dump_log_csv.EXPECTED_HEADER,
            "0,1000,1,5000,5.000,0,0.000,1,0,1,3300,3.300,0,0.000,1,0",
        ]
        for case in (missing, duplicate):
            with self.assertRaisesRegex(RuntimeError, "exactly one exported_samples"):
                dump_log_csv.validate_csv_lines(case)

    def test_rejects_negative_or_mismatched_exported_samples(self):
        negative = _base_lines(
            "0,1000,1,5000,5.000,0,0.000,1,0,1,3300,3.300,0,0.000,1,0",
            exported_samples=-1,
        )
        mismatch = _base_lines(
            "0,1000,1,5000,5.000,0,0.000,1,0,1,3300,3.300,0,0.000,1,0",
            exported_samples=2,
        )
        with self.assertRaisesRegex(RuntimeError, "must be non-negative"):
            dump_log_csv.validate_csv_lines(negative)
        with self.assertRaisesRegex(RuntimeError, "does not match parsed row count"):
            dump_log_csv.validate_csv_lines(mismatch)

    def test_requires_idx_zero_start_and_contiguous(self):
        wrong_start = _base_lines(
            "1,1000,1,5000,5.000,0,0.000,1,0,1,3300,3.300,0,0.000,1,0"
        )
        gap = _base_lines(
            "0,1000,1,5000,5.000,0,0.000,1,0,1,3300,3.300,0,0.000,1,0",
            "2,1200,2,5001,5.001,0,0.000,2,0,2,3300,3.300,0,0.000,1,0",
        )
        for case in (wrong_start, gap):
            with self.assertRaisesRegex(RuntimeError, "expected idx"):
                dump_log_csv.validate_csv_lines(case)

    def test_requires_binary_has_ch2_and_demo(self):
        bad_has_ch2 = _base_lines(
            "0,1000,1,5000,5.000,0,0.000,1,0,1,3300,3.300,0,0.000,2,0"
        )
        bad_demo = _base_lines(
            "0,1000,1,5000,5.000,0,0.000,1,0,1,3300,3.300,0,0.000,1,2"
        )
        with self.assertRaisesRegex(RuntimeError, "has_ch2 must be 0 or 1"):
            dump_log_csv.validate_csv_lines(bad_has_ch2)
        with self.assertRaisesRegex(RuntimeError, "demo must be 0 or 1"):
            dump_log_csv.validate_csv_lines(bad_demo)

    def test_requires_empty_ch2_cells_when_has_ch2_is_zero(self):
        bad = _base_lines(
            "0,1000,1,5000,5.000,0,0.000,1,0,1,3300,3.300,0,0.000,0,0"
        )
        with self.assertRaisesRegex(RuntimeError, "has_ch2=0 requires empty CH2 fields"):
            dump_log_csv.validate_csv_lines(bad)

    def test_rejects_t_ms_outside_uint32(self):
        too_large = _base_lines(
            f"0,{(1 << 32)},1,5000,5.000,0,0.000,1,0,1,3300,3.300,0,0.000,1,0"
        )
        with self.assertRaisesRegex(RuntimeError, "t_ms out of uint32 range"):
            dump_log_csv.validate_csv_lines(too_large)

    def test_accepts_uint32_rollover_when_step_is_plausible(self):
        lines = _base_lines(
            f"0,{dump_log_csv.UINT32_MAX - 150},1,5000,5.000,0,0.000,1,0,1,3300,3.300,0,0.000,1,0",
            "1,49,2,5001,5.001,0,0.000,2,0,2,3300,3.300,0,0.000,1,0",
            sample_period_ms=200,
        )
        dump_log_csv.validate_csv_lines(lines)

    def test_rejects_arbitrary_timestamp_backwards_jump(self):
        lines = _base_lines(
            "0,50000,1,5000,5.000,0,0.000,1,0,1,3300,3.300,0,0.000,1,0",
            "1,40000,2,5001,5.001,0,0.000,2,0,2,3300,3.300,0,0.000,1,0",
            sample_period_ms=200,
        )
        with self.assertRaisesRegex(RuntimeError, "timestamp step"):
            dump_log_csv.validate_csv_lines(lines)


class DumpLogCsvCaptureTests(unittest.TestCase):
    def _with_serial_module(self, fake_module, func):
        old = sys.modules.get("serial")
        sys.modules["serial"] = fake_module
        try:
            return func()
        finally:
            if old is None:
                sys.modules.pop("serial", None)
            else:
                sys.modules["serial"] = old

    def test_capture_complete_export(self):
        fake = _FakeSerialModule(
            lines=[
                b"noise\n",
                b"csv_begin\n",
                b"# exported_samples=1\n",
                b"a,b,c\n",
                b"csv_end\n",
            ]
        )
        lines = self._with_serial_module(
            fake,
            lambda: dump_log_csv.capture_csv("COMX", "LOG_DUMP_CSV", timeout_seconds=0.2, startup_delay=0),
        )
        self.assertEqual(lines, ["# exported_samples=1", "a,b,c"])

    def test_capture_no_samples(self):
        fake = _FakeSerialModule(lines=[b"csv: no samples\n"])
        with self.assertRaisesRegex(RuntimeError, "trend buffer is empty"):
            self._with_serial_module(
                fake,
                lambda: dump_log_csv.capture_csv("COMX", "LOG_DUMP_CSV", timeout_seconds=0.1, startup_delay=0),
            )

    def test_capture_missing_csv_begin(self):
        fake = _FakeSerialModule(lines=[b"boot\n", b"still booting\n"])
        with self.assertRaisesRegex(RuntimeError, "Timed out waiting for csv_begin"):
            self._with_serial_module(
                fake,
                lambda: dump_log_csv.capture_csv("COMX", "LOG_DUMP_CSV", timeout_seconds=0.05, startup_delay=0),
            )

    def test_capture_missing_csv_end(self):
        fake = _FakeSerialModule(lines=[b"csv_begin\n", b"row\n"])
        with self.assertRaisesRegex(RuntimeError, "Timed out waiting for csv_end"):
            self._with_serial_module(
                fake,
                lambda: dump_log_csv.capture_csv("COMX", "LOG_DUMP_CSV", timeout_seconds=0.05, startup_delay=0),
            )

    def test_capture_open_failure(self):
        fake = _FakeSerialModule(open_error=OSError("access denied"))
        with self.assertRaisesRegex(OSError, "access denied"):
            self._with_serial_module(
                fake,
                lambda: dump_log_csv.capture_csv("COMX", "LOG_DUMP_CSV", timeout_seconds=0.1, startup_delay=0),
            )

    def test_main_does_not_write_file_on_validation_failure(self):
        bad_lines = [
            b"csv_begin\n",
            b"# sample_period_ms=200\n",
            b"# exported_samples=1\n",
            (dump_log_csv.EXPECTED_HEADER + "\n").encode("utf-8"),
            b"noise row\n",
            b"csv_end\n",
        ]
        fake = _FakeSerialModule(lines=bad_lines)
        with tempfile.TemporaryDirectory() as td:
            out_path = pathlib.Path(td) / "capture.csv"
            old_argv = sys.argv[:]
            try:
                sys.argv = [
                    "dump_log_csv.py",
                    "COMX",
                    "--startup-delay",
                    "0",
                    "--timeout",
                    "0.1",
                    "--output",
                    str(out_path),
                ]
                with self.assertRaises(SystemExit) as cm:
                    self._with_serial_module(fake, dump_log_csv.main)
                self.assertEqual(cm.exception.code, 1)
            finally:
                sys.argv = old_argv
            self.assertFalse(out_path.exists())


if __name__ == "__main__":
    unittest.main()
