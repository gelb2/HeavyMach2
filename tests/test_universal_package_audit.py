"""Synthetic format/boundary checks; contains no original game resources."""
from datetime import datetime
import importlib.util
from pathlib import Path
import struct
import sys
import unittest

TOOLS = Path(__file__).resolve().parents[1] / "tools"
sys.path.insert(0, str(TOOLS))
spec = importlib.util.spec_from_file_location("universal_audit", TOOLS / "audit_universal_packages.py")
audit = importlib.util.module_from_spec(spec)
spec.loader.exec_module(audit)


def png_chunk(kind, data):
    # CRC is irrelevant to this metadata parser; ZIP reader checks archive CRC.
    return struct.pack(">I", len(data)) + kind + data + bytes(4)


class UniversalPackageAuditTests(unittest.TestCase):
    def test_cgbi_before_ihdr_and_bounds(self):
        data = b"\x89PNG\r\n\x1a\n" + png_chunk(b"CgBI", b"test")
        data += png_chunk(b"IHDR", struct.pack(">IIBBBBB", 30, 70, 8, 6, 0, 0, 0))
        data += png_chunk(b"IEND", b"")
        self.assertEqual(audit.png_metadata(data)["pixels"], [30, 70])
        self.assertEqual(audit.png_metadata(data)["encoding"], "CgBI PNG")
        with self.assertRaisesRegex(ValueError, "exceeds|Incomplete"):
            audit.png_metadata(data[:-2])

    def test_wav_format_and_odd_chunk_padding(self):
        fmt = struct.pack("<HHIIHH", 1, 2, 22050, 88200, 4, 16)
        chunks = b"JUNK" + struct.pack("<I", 1) + b"x\0"
        chunks += b"fmt " + struct.pack("<I", len(fmt)) + fmt
        chunks += b"data" + struct.pack("<I", 4) + bytes(4)
        raw = b"RIFF" + struct.pack("<I", len(chunks) + 4) + b"WAVE" + chunks
        meta = audit.wav_metadata(raw)
        self.assertEqual((meta["channels"], meta["bits_per_sample"], meta["sample_rate"]), (2, 16, 22050))
        self.assertEqual(meta["audio_data_bytes"], 4)
        with self.assertRaisesRegex(ValueError, "exceeds"):
            audit.wav_metadata(raw[:-1])

    def test_table_duplicate_occurrences_and_comment_changes(self):
        left = audit.table_tokens(b"Code\tValue\nA\t1\nA\t2\n//B\t3\n")
        right = audit.table_tokens(b"Code\tValue\nA\t1\nA\t9\nB\t3\n")
        change = audit.table_difference(left, right)
        self.assertEqual(change["changed_records"], [{"key": "A", "occurrence": 1, "fields": ["Value"]}])
        self.assertEqual(change["only_right_record_ids"], [{"key": "B", "occurrence": 0}])
        self.assertFalse(change["comments_equal"])
        self.assertEqual(change["left"]["duplicate_first_column_values"], {"A": 2})

    def test_table_missing_column_is_not_silently_padded(self):
        parsed = audit.table_tokens(b"Code\tA\tB\nX\t1\n")
        self.assertEqual(parsed["rows"], [["X", "1"]])
        self.assertEqual(parsed["column_anomalies"], [{"row": 0, "key": "X", "expected": 3, "actual": 2}])

    def test_base_type_and_id_pair_survives_row_removal(self):
        left = audit.table_tokens(b"Type\tImageFile\tX\tID\nButton\tA.png\t10\t1\nButton\tB.png\t20\t2\n")
        right = audit.table_tokens(b"Type\tImageFile\tX\tID\nButton\tB.png\t25\t2\n")
        change = audit.table_difference(left, right)
        self.assertEqual(change["only_left_record_ids"], [{"key": "Type=Button|ID=1", "occurrence": 0}])
        self.assertEqual(change["changed_records"], [{"key": "Type=Button|ID=2", "occurrence": 0, "fields": ["X"]}])

    def test_metadata_hash_preserves_type(self):
        self.assertNotEqual(audit.json_hash(b"x"), audit.json_hash("x"))
        self.assertEqual(audit.json_hash(datetime(2020, 1, 1)), audit.json_hash(datetime(2020, 1, 1)))

    def test_logical_package_paths_do_not_merge_resources(self):
        self.assertEqual(audit.logical_path("Payload/Game.app/A.png", "Payload/Game.app/"), "app/A.png")
        self.assertEqual(audit.logical_path("A.png", "Payload/Game.app/"), "package/A.png")


if __name__ == "__main__":
    unittest.main()
