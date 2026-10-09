import pathlib
import struct
import sys
import tempfile
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))

from check_stm32_size_budget import BudgetCheckError, evaluate_budget


FLASH_BASE = 0x08000000


def _make_minimal_elf(path: pathlib.Path, *, paddr: int, filesz: int) -> None:
    ident = bytearray(16)
    ident[0:4] = b"\x7fELF"
    ident[4] = 1  # 32-bit
    ident[5] = 1  # little-endian
    ident[6] = 1  # version

    e_type = 2
    e_machine = 40  # ARM
    e_version = 1
    e_entry = paddr
    e_phoff = 52
    e_shoff = 0
    e_flags = 0
    e_ehsize = 52
    e_phentsize = 32
    e_phnum = 1
    e_shentsize = 0
    e_shnum = 0
    e_shstrndx = 0

    elf_header = struct.pack(
        "<16sHHIIIIIHHHHHH",
        bytes(ident),
        e_type,
        e_machine,
        e_version,
        e_entry,
        e_phoff,
        e_shoff,
        e_flags,
        e_ehsize,
        e_phentsize,
        e_phnum,
        e_shentsize,
        e_shnum,
        e_shstrndx,
    )

    p_type = 1
    p_offset = 0x100
    p_vaddr = paddr
    p_paddr = paddr
    p_filesz = filesz
    p_memsz = filesz
    p_flags = 5
    p_align = 4
    prog_header = struct.pack(
        "<IIIIIIII",
        p_type,
        p_offset,
        p_vaddr,
        p_paddr,
        p_filesz,
        p_memsz,
        p_flags,
        p_align,
    )

    raw = bytearray(max(p_offset + filesz, 0x120))
    raw[0 : len(elf_header)] = elf_header
    raw[e_phoff : e_phoff + len(prog_header)] = prog_header
    raw[p_offset : p_offset + filesz] = b"\xaa" * filesz
    path.write_bytes(bytes(raw))


def _make_inputs(root: pathlib.Path, *, image_size: int, log_text: str) -> pathlib.Path:
    build_dir = root / ".pio" / "build" / "bluepill_f103c8"
    build_dir.mkdir(parents=True, exist_ok=True)

    (build_dir / "firmware.bin").write_bytes(b"\x00" * image_size)
    _make_minimal_elf(build_dir / "firmware.elf", paddr=FLASH_BASE, filesz=image_size)
    (build_dir / "firmware.map").write_text(
        "\n".join(
            [
                " .isr_vector  0x08000000  0x10c",
                " .text        0x0800010c  0x1000",
                " .rodata      0x0800110c  0x200",
                " .init_array  0x0800130c  0x10",
                " .fini_array  0x0800131c  0x10",
                " .data        0x20000000  0x80",
            ]
        ),
        encoding="utf-8",
    )

    log_path = root / "build.log"
    log_path.write_text(log_text, encoding="utf-8")
    return log_path


class CheckBudgetTests(unittest.TestCase):
    def test_exact_limit_passes(self) -> None:
        with tempfile.TemporaryDirectory() as td:
            root = pathlib.Path(td)
            log_path = _make_inputs(
                root,
                image_size=100,
                log_text=(
                    "Flash: [===] 1.0% (used 100 bytes from 65536 bytes)\n"
                    "RAM:   [===] 5.0% (used 1000 bytes from 20480 bytes)\n"
                ),
            )
            report = evaluate_budget(
                env="bluepill_f103c8",
                build_dir=root / ".pio" / "build" / "bluepill_f103c8",
                build_log=log_path,
                flash_capacity_bytes=200,
                flash_limit_bytes=100,
            )
            self.assertEqual(report["flash"]["used_bytes"], 100)
            self.assertEqual(report["flash"]["free_bytes"], 100)

    def test_one_byte_over_limit_fails(self) -> None:
        with tempfile.TemporaryDirectory() as td:
            root = pathlib.Path(td)
            log_path = _make_inputs(
                root,
                image_size=101,
                log_text=(
                    "Flash: [===] 1.0% (used 101 bytes from 65536 bytes)\n"
                    "RAM:   [===] 5.0% (used 1000 bytes from 20480 bytes)\n"
                ),
            )
            with self.assertRaises(BudgetCheckError):
                evaluate_budget(
                    env="bluepill_f103c8",
                    build_dir=root / ".pio" / "build" / "bluepill_f103c8",
                    build_log=log_path,
                    flash_capacity_bytes=200,
                    flash_limit_bytes=100,
                )

    def test_missing_or_empty_image_fails(self) -> None:
        with tempfile.TemporaryDirectory() as td:
            root = pathlib.Path(td)
            log_path = _make_inputs(
                root,
                image_size=10,
                log_text=(
                    "Flash: [===] 1.0% (used 10 bytes from 65536 bytes)\n"
                    "RAM:   [===] 5.0% (used 1000 bytes from 20480 bytes)\n"
                ),
            )
            build_dir = root / ".pio" / "build" / "bluepill_f103c8"

            (build_dir / "firmware.bin").unlink()
            with self.assertRaises(BudgetCheckError):
                evaluate_budget(
                    env="bluepill_f103c8",
                    build_dir=build_dir,
                    build_log=log_path,
                    flash_capacity_bytes=200,
                    flash_limit_bytes=100,
                )

            (build_dir / "firmware.bin").write_bytes(b"")
            with self.assertRaises(BudgetCheckError):
                evaluate_budget(
                    env="bluepill_f103c8",
                    build_dir=build_dir,
                    build_log=log_path,
                    flash_capacity_bytes=200,
                    flash_limit_bytes=100,
                )

    def test_missing_or_malformed_ram_metric_fails(self) -> None:
        with tempfile.TemporaryDirectory() as td:
            root = pathlib.Path(td)
            log_path = _make_inputs(
                root,
                image_size=100,
                log_text="Flash: [===] 1.0% (used 100 bytes from 65536 bytes)\n",
            )
            with self.assertRaises(BudgetCheckError):
                evaluate_budget(
                    env="bluepill_f103c8",
                    build_dir=root / ".pio" / "build" / "bluepill_f103c8",
                    build_log=log_path,
                    flash_capacity_bytes=200,
                    flash_limit_bytes=100,
                )

    def test_flash_accounting_is_independent_of_ram_metric(self) -> None:
        with tempfile.TemporaryDirectory() as td:
            root = pathlib.Path(td)
            build_dir = root / ".pio" / "build" / "bluepill_f103c8"

            log_a = _make_inputs(
                root,
                image_size=120,
                log_text=(
                    "Flash: [===] 1.0% (used 120 bytes from 65536 bytes)\n"
                    "RAM:   [===] 5.0% (used 1000 bytes from 20480 bytes)\n"
                ),
            )
            report_a = evaluate_budget(
                env="bluepill_f103c8",
                build_dir=build_dir,
                build_log=log_a,
                flash_capacity_bytes=300,
                flash_limit_bytes=150,
            )

            log_b = root / "build-b.log"
            log_b.write_text(
                "Flash: [===] 1.0% (used 120 bytes from 65536 bytes)\n"
                "RAM:   [===] 99.0% (used 20200 bytes from 20480 bytes)\n",
                encoding="utf-8",
            )
            report_b = evaluate_budget(
                env="bluepill_f103c8",
                build_dir=build_dir,
                build_log=log_b,
                flash_capacity_bytes=300,
                flash_limit_bytes=150,
            )

            self.assertEqual(report_a["flash"]["used_bytes"], report_b["flash"]["used_bytes"])
            self.assertEqual(report_a["flash"]["free_bytes"], report_b["flash"]["free_bytes"])
            self.assertNotEqual(
                report_a["platformio_metrics"]["ram"]["used_bytes"],
                report_b["platformio_metrics"]["ram"]["used_bytes"],
            )


if __name__ == "__main__":
    unittest.main()
