import pathlib
import struct
import sys
import tempfile
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))

from check_stm32_size_budget import BudgetCheckError, evaluate_budget


FLASH_BASE = 0x08000000
FLASH_CAPACITY = 65536
RAM_CAPACITY = 20480


def _make_elf(path: pathlib.Path, *, segments: list[dict], e_phentsize: int = 32) -> None:
    ident = bytearray(16)
    ident[0:4] = b"\x7fELF"
    ident[4] = 1  # 32-bit
    ident[5] = 1  # little-endian
    ident[6] = 1  # version

    e_type = 2
    e_machine = 40  # ARM
    e_version = 1
    e_entry = segments[0]["p_paddr"] if segments else FLASH_BASE
    e_phoff = 52
    e_shoff = 0
    e_flags = 0
    e_ehsize = 52
    e_phnum = len(segments)
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

    headers = bytearray()
    max_size = 0x120
    for seg in segments:
        headers.extend(
            struct.pack(
                "<IIIIIIII",
                seg.get("p_type", 1),
                seg["p_offset"],
                seg.get("p_vaddr", seg["p_paddr"]),
                seg["p_paddr"],
                seg["p_filesz"],
                seg.get("p_memsz", seg["p_filesz"]),
                seg.get("p_flags", 5),
                seg.get("p_align", 4),
            )
        )
        max_size = max(max_size, seg["p_offset"] + seg["p_filesz"])

    raw = bytearray(max_size)
    raw[0 : len(elf_header)] = elf_header
    raw[e_phoff : e_phoff + len(headers)] = headers
    for seg in segments:
        if seg["p_filesz"] > 0:
            start = seg["p_offset"]
            end = start + seg["p_filesz"]
            raw[start:end] = b"\xaa" * seg["p_filesz"]
    path.write_bytes(bytes(raw))


def _default_map_text() -> str:
    return "\n".join(
        [
            "Discarded input sections",
            " .text          0x00000000        0x9999 discarded.o",
            "Memory Configuration",
            "FLASH 0x08000000 0x00010000 xr",
            "RAM   0x20000000 0x00005000 xrw",
            "Linker script and memory map",
            ".isr_vector  0x08000000  0x10c",
            ".text        0x0800010c  0x1000",
            " .text.helper 0x08000120  0x10 nested.o",
            ".rodata      0x0800110c  0x200",
            ".data        0x20000000  0x80 load address 0x0800130c",
            ".bss         0x20000080  0x100",
        ]
    )


def _make_inputs(
    root: pathlib.Path,
    *,
    image_size: int,
    log_text: str,
    segments: list[dict] | None = None,
    map_text: str | None = None,
) -> pathlib.Path:
    build_dir = root / ".pio" / "build" / "bluepill_f103c8"
    build_dir.mkdir(parents=True, exist_ok=True)

    (build_dir / "firmware.bin").write_bytes(b"\x00" * image_size)
    elf_segments = segments or [
        {
            "p_offset": 0x100,
            "p_vaddr": FLASH_BASE,
            "p_paddr": FLASH_BASE,
            "p_filesz": image_size,
            "p_memsz": image_size,
            "p_flags": 5,
        }
    ]
    _make_elf(build_dir / "firmware.elf", segments=elf_segments)
    (build_dir / "firmware.map").write_text(map_text or _default_map_text(), encoding="utf-8")

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
                flash_capacity_bytes=FLASH_CAPACITY,
                flash_limit_bytes=100,
            )
            self.assertEqual(report["flash"]["used_bytes"], 100)
            self.assertEqual(report["flash"]["free_bytes"], FLASH_CAPACITY - 100)

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
                    flash_capacity_bytes=FLASH_CAPACITY,
                    flash_limit_bytes=100,
                )

    def test_static_ram_exact_capacity_passes(self) -> None:
        with tempfile.TemporaryDirectory() as td:
            root = pathlib.Path(td)
            log_path = _make_inputs(
                root,
                image_size=32,
                log_text=(
                    "Flash: [===] 1.0% (used 32 bytes from 65536 bytes)\n"
                    "RAM:   [===] 100.0% (used 20480 bytes from 20480 bytes)\n"
                ),
            )
            report = evaluate_budget(
                env="bluepill_f103c8",
                build_dir=root / ".pio" / "build" / "bluepill_f103c8",
                build_log=log_path,
                flash_capacity_bytes=FLASH_CAPACITY,
                flash_limit_bytes=40,
            )
            self.assertTrue(report["platformio_metrics"]["ram"]["within_capacity"])

    def test_static_ram_one_byte_over_fails(self) -> None:
        with tempfile.TemporaryDirectory() as td:
            root = pathlib.Path(td)
            log_path = _make_inputs(
                root,
                image_size=32,
                log_text=(
                    "Flash: [===] 1.0% (used 32 bytes from 65536 bytes)\n"
                    "RAM:   [===] 100.0% (used 20481 bytes from 20480 bytes)\n"
                ),
            )
            with self.assertRaisesRegex(BudgetCheckError, "static RAM metric exceeds capacity"):
                evaluate_budget(
                    env="bluepill_f103c8",
                    build_dir=root / ".pio" / "build" / "bluepill_f103c8",
                    build_log=log_path,
                    flash_capacity_bytes=FLASH_CAPACITY,
                    flash_limit_bytes=40,
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
                    flash_capacity_bytes=FLASH_CAPACITY,
                    flash_limit_bytes=100,
                )

            (build_dir / "firmware.bin").write_bytes(b"")
            with self.assertRaises(BudgetCheckError):
                evaluate_budget(
                    env="bluepill_f103c8",
                    build_dir=build_dir,
                    build_log=log_path,
                    flash_capacity_bytes=FLASH_CAPACITY,
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
                    flash_capacity_bytes=FLASH_CAPACITY,
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
                flash_capacity_bytes=FLASH_CAPACITY,
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
                flash_capacity_bytes=FLASH_CAPACITY,
                flash_limit_bytes=150,
            )

            self.assertEqual(report_a["flash"]["used_bytes"], report_b["flash"]["used_bytes"])
            self.assertEqual(report_a["flash"]["free_bytes"], report_b["flash"]["free_bytes"])
            self.assertNotEqual(
                report_a["platformio_metrics"]["ram"]["used_bytes"],
                report_b["platformio_metrics"]["ram"]["used_bytes"],
            )

    def test_multiple_segments_and_data_lma_in_flash_are_counted(self) -> None:
        with tempfile.TemporaryDirectory() as td:
            root = pathlib.Path(td)
            # Programmed flash span: [0x08000000, 0x08000050) = 80 bytes
            segments = [
                {
                    "p_offset": 0x100,
                    "p_vaddr": FLASH_BASE,
                    "p_paddr": FLASH_BASE,
                    "p_filesz": 0x20,
                    "p_memsz": 0x20,
                    "p_flags": 5,
                },
                # Gap then .data initializers loaded from flash
                {
                    "p_offset": 0x200,
                    "p_vaddr": 0x20000000,
                    "p_paddr": FLASH_BASE + 0x40,
                    "p_filesz": 0x10,
                    "p_memsz": 0x90,
                    "p_flags": 6,
                },
                # RAM-only and zero-file segments must not affect flash span
                {
                    "p_offset": 0x300,
                    "p_vaddr": 0x20000100,
                    "p_paddr": 0x20000100,
                    "p_filesz": 0x08,
                    "p_memsz": 0x08,
                    "p_flags": 6,
                },
                {
                    "p_offset": 0x308,
                    "p_vaddr": 0x20000200,
                    "p_paddr": 0x20000200,
                    "p_filesz": 0x00,
                    "p_memsz": 0x20,
                    "p_flags": 6,
                },
            ]
            log_path = _make_inputs(
                root,
                image_size=0x50,
                segments=segments,
                log_text=(
                    "Flash: [===] 1.0% (used 80 bytes from 65536 bytes)\n"
                    "RAM:   [===] 5.0% (used 1000 bytes from 20480 bytes)\n"
                ),
            )
            report = evaluate_budget(
                env="bluepill_f103c8",
                build_dir=root / ".pio" / "build" / "bluepill_f103c8",
                build_log=log_path,
                flash_capacity_bytes=FLASH_CAPACITY,
                flash_limit_bytes=0x50,
            )
            self.assertEqual(report["flash"]["used_bytes"], 0x50)
            self.assertEqual(report["flash"]["elf_flash_span_bytes"], 0x50)

    def test_shifted_image_start_fails(self) -> None:
        with tempfile.TemporaryDirectory() as td:
            root = pathlib.Path(td)
            segments = [
                {
                    "p_offset": 0x100,
                    "p_vaddr": FLASH_BASE + 0x10,
                    "p_paddr": FLASH_BASE + 0x10,
                    "p_filesz": 0x20,
                    "p_memsz": 0x20,
                }
            ]
            log_path = _make_inputs(
                root,
                image_size=0x20,
                segments=segments,
                log_text=(
                    "Flash: [===] 1.0% (used 32 bytes from 65536 bytes)\n"
                    "RAM:   [===] 5.0% (used 1000 bytes from 20480 bytes)\n"
                ),
            )
            with self.assertRaisesRegex(BudgetCheckError, "does not start at expected base"):
                evaluate_budget(
                    env="bluepill_f103c8",
                    build_dir=root / ".pio" / "build" / "bluepill_f103c8",
                    build_log=log_path,
                    flash_capacity_bytes=FLASH_CAPACITY,
                    flash_limit_bytes=0x30,
                )

    def test_out_of_region_flash_segment_start_fails(self) -> None:
        with tempfile.TemporaryDirectory() as td:
            root = pathlib.Path(td)
            segments = [
                {
                    "p_offset": 0x100,
                    "p_vaddr": FLASH_BASE + FLASH_CAPACITY,
                    "p_paddr": FLASH_BASE + FLASH_CAPACITY,
                    "p_filesz": 0x10,
                    "p_memsz": 0x10,
                }
            ]
            log_path = _make_inputs(
                root,
                image_size=0x10,
                segments=segments,
                log_text=(
                    "Flash: [===] 1.0% (used 16 bytes from 65536 bytes)\n"
                    "RAM:   [===] 5.0% (used 1000 bytes from 20480 bytes)\n"
                ),
            )
            with self.assertRaisesRegex(BudgetCheckError, "starts above flash upper bound"):
                evaluate_budget(
                    env="bluepill_f103c8",
                    build_dir=root / ".pio" / "build" / "bluepill_f103c8",
                    build_log=log_path,
                    flash_capacity_bytes=FLASH_CAPACITY,
                    flash_limit_bytes=0x20,
                )

    def test_upper_bound_crossing_fails(self) -> None:
        with tempfile.TemporaryDirectory() as td:
            root = pathlib.Path(td)
            segments = [
                {
                    "p_offset": 0x100,
                    "p_vaddr": FLASH_BASE,
                    "p_paddr": FLASH_BASE,
                    "p_filesz": FLASH_CAPACITY + 1,
                    "p_memsz": FLASH_CAPACITY + 1,
                }
            ]
            log_path = _make_inputs(
                root,
                image_size=FLASH_CAPACITY + 1,
                segments=segments,
                log_text=(
                    "Flash: [===] 1.0% (used 65537 bytes from 65536 bytes)\n"
                    "RAM:   [===] 5.0% (used 1000 bytes from 20480 bytes)\n"
                ),
            )
            with self.assertRaisesRegex(BudgetCheckError, "exceeds flash upper bound"):
                evaluate_budget(
                    env="bluepill_f103c8",
                    build_dir=root / ".pio" / "build" / "bluepill_f103c8",
                    build_log=log_path,
                    flash_capacity_bytes=FLASH_CAPACITY,
                    flash_limit_bytes=FLASH_CAPACITY,
                )

    def test_malformed_or_truncated_elf_fails(self) -> None:
        with tempfile.TemporaryDirectory() as td:
            root = pathlib.Path(td)
            log_path = _make_inputs(
                root,
                image_size=16,
                log_text=(
                    "Flash: [===] 1.0% (used 16 bytes from 65536 bytes)\n"
                    "RAM:   [===] 5.0% (used 1000 bytes from 20480 bytes)\n"
                ),
            )
            build_dir = root / ".pio" / "build" / "bluepill_f103c8"

            (build_dir / "firmware.elf").write_bytes(b"\x7fELF")
            with self.assertRaisesRegex(BudgetCheckError, "malformed ELF"):
                evaluate_budget(
                    env="bluepill_f103c8",
                    build_dir=build_dir,
                    build_log=log_path,
                    flash_capacity_bytes=FLASH_CAPACITY,
                    flash_limit_bytes=32,
                )

            _make_elf(
                build_dir / "firmware.elf",
                segments=[
                    {
                        "p_offset": 0x100,
                        "p_vaddr": FLASH_BASE,
                        "p_paddr": FLASH_BASE,
                        "p_filesz": 0x20,
                    }
                ],
            )
            elf_data = bytearray((build_dir / "firmware.elf").read_bytes())
            # Corrupt e_phoff to point outside the file.
            struct.pack_into("<I", elf_data, 28, 0xFFFFFF00)
            (build_dir / "firmware.elf").write_bytes(bytes(elf_data))
            with self.assertRaisesRegex(BudgetCheckError, "truncated program header table"):
                evaluate_budget(
                    env="bluepill_f103c8",
                    build_dir=build_dir,
                    build_log=log_path,
                    flash_capacity_bytes=FLASH_CAPACITY,
                    flash_limit_bytes=32,
                )

    def test_discarded_or_nested_input_sections_cannot_mask_output_sections(self) -> None:
        with tempfile.TemporaryDirectory() as td:
            root = pathlib.Path(td)
            map_text = "\n".join(
                [
                    "Discarded input sections",
                    " .text 0x00000000 0x7777 discarded.o",
                    "Memory Configuration",
                    "FLASH 0x08000000 0x00010000 xr",
                    "RAM   0x20000000 0x00005000 xrw",
                    "Linker script and memory map",
                    ".text 0x08000000 0x10",
                    " .text.some_input 0x08000010 0x9999 object.o",
                    ".rodata 0x08000010 0x20",
                    ".data 0x20000000 0x08 load address 0x08000030",
                    ".bss 0x20000008 0x04",
                ]
            )
            log_path = _make_inputs(
                root,
                image_size=0x38,
                map_text=map_text,
                segments=[
                    {
                        "p_offset": 0x100,
                        "p_vaddr": FLASH_BASE,
                        "p_paddr": FLASH_BASE,
                        "p_filesz": 0x30,
                    },
                    {
                        "p_offset": 0x200,
                        "p_vaddr": 0x20000000,
                        "p_paddr": FLASH_BASE + 0x30,
                        "p_filesz": 0x08,
                        "p_memsz": 0x20,
                    },
                ],
                log_text=(
                    "Flash: [===] 1.0% (used 56 bytes from 65536 bytes)\n"
                    "RAM:   [===] 5.0% (used 1000 bytes from 20480 bytes)\n"
                ),
            )
            report = evaluate_budget(
                env="bluepill_f103c8",
                build_dir=root / ".pio" / "build" / "bluepill_f103c8",
                build_log=log_path,
                flash_capacity_bytes=FLASH_CAPACITY,
                flash_limit_bytes=64,
            )
            self.assertEqual(report["map_sections"][".text"], 0x10)

    def test_wrapped_top_level_map_section_entries_are_supported(self) -> None:
        with tempfile.TemporaryDirectory() as td:
            root = pathlib.Path(td)
            map_text = "\n".join(
                [
                    "Memory Configuration",
                    "FLASH 0x08000000 0x00010000 xr",
                    "RAM   0x20000000 0x00005000 xrw",
                    "Linker script and memory map",
                    ".text",
                    " 0x08000000 0x20",
                    ".rodata 0x08000020 0x10",
                    ".data",
                    " 0x20000000 0x08 load address 0x08000030",
                    ".bss",
                    " 0x20000008 0x10",
                ]
            )
            log_path = _make_inputs(
                root,
                image_size=0x38,
                map_text=map_text,
                segments=[
                    {
                        "p_offset": 0x100,
                        "p_vaddr": FLASH_BASE,
                        "p_paddr": FLASH_BASE,
                        "p_filesz": 0x30,
                    },
                    {
                        "p_offset": 0x200,
                        "p_vaddr": 0x20000000,
                        "p_paddr": FLASH_BASE + 0x30,
                        "p_filesz": 0x08,
                        "p_memsz": 0x20,
                    },
                ],
                log_text=(
                    "Flash: [===] 1.0% (used 56 bytes from 65536 bytes)\n"
                    "RAM:   [===] 5.0% (used 1000 bytes from 20480 bytes)\n"
                ),
            )
            report = evaluate_budget(
                env="bluepill_f103c8",
                build_dir=root / ".pio" / "build" / "bluepill_f103c8",
                build_log=log_path,
                flash_capacity_bytes=FLASH_CAPACITY,
                flash_limit_bytes=64,
            )
            self.assertEqual(report["map_sections"][".text"], 0x20)
            self.assertEqual(report["map_sections"][".data"], 0x08)
            self.assertEqual(report["map_sections"][".bss"], 0x10)

    def test_incorrect_capacity_metadata_fails(self) -> None:
        with tempfile.TemporaryDirectory() as td:
            root = pathlib.Path(td)
            log_path = _make_inputs(
                root,
                image_size=32,
                log_text=(
                    "Flash: [===] 1.0% (used 32 bytes from 70000 bytes)\n"
                    "RAM:   [===] 5.0% (used 1000 bytes from 24576 bytes)\n"
                ),
            )
            with self.assertRaisesRegex(BudgetCheckError, "configured flash capacity must be 65536"):
                evaluate_budget(
                    env="bluepill_f103c8",
                    build_dir=root / ".pio" / "build" / "bluepill_f103c8",
                    build_log=log_path,
                    flash_capacity_bytes=70000,
                    flash_limit_bytes=40,
                )
            with self.assertRaisesRegex(BudgetCheckError, "configured static RAM capacity must be 20480"):
                evaluate_budget(
                    env="bluepill_f103c8",
                    build_dir=root / ".pio" / "build" / "bluepill_f103c8",
                    build_log=log_path,
                    flash_capacity_bytes=FLASH_CAPACITY,
                    flash_limit_bytes=40,
                    ram_capacity_bytes=24576,
                )
            with self.assertRaisesRegex(BudgetCheckError, "PlatformIO flash capacity mismatch"):
                evaluate_budget(
                    env="bluepill_f103c8",
                    build_dir=root / ".pio" / "build" / "bluepill_f103c8",
                    build_log=log_path,
                    flash_capacity_bytes=FLASH_CAPACITY,
                    flash_limit_bytes=40,
                )

            log_path_ram = _make_inputs(
                root,
                image_size=32,
                log_text=(
                    "Flash: [===] 1.0% (used 32 bytes from 65536 bytes)\n"
                    "RAM:   [===] 5.0% (used 1000 bytes from 24576 bytes)\n"
                ),
            )
            with self.assertRaisesRegex(BudgetCheckError, "PlatformIO static RAM capacity mismatch"):
                evaluate_budget(
                    env="bluepill_f103c8",
                    build_dir=root / ".pio" / "build" / "bluepill_f103c8",
                    build_log=log_path_ram,
                    flash_capacity_bytes=FLASH_CAPACITY,
                    flash_limit_bytes=40,
                )


if __name__ == "__main__":
    unittest.main()
