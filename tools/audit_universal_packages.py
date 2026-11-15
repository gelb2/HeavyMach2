#!/usr/bin/env python3
"""Read-only census of all six original HM2 packages for universal planning.

No archive is extracted. The ignored local inventory records every member's
identity; the repository summary contains metadata, counts, hashes and changed
record identifiers, never original resource bytes or full decoded tables.
"""
from __future__ import annotations

import argparse
from collections import Counter
from datetime import datetime
import hashlib
import json
from pathlib import Path, PurePosixPath
import plistlib
import re
import stat
import struct
import zipfile

from audit_ipa_versions import decode

ROOT = Path(__file__).resolve().parents[1]
VERSION_IDS = ("phone100", "phone110", "phone130", "phone201", "hd100", "hd103")
TECHNICAL_PLIST_KEYS = (
    "CFBundleIdentifier", "CFBundleVersion", "CFBundleShortVersionString",
    "CFBundleExecutable", "UIDeviceFamily", "MinimumOSVersion",
    "UIRequiredDeviceCapabilities", "UISupportedInterfaceOrientations",
    "UISupportedInterfaceOrientations~ipad", "UIRequiresFullScreen",
    "UIInterfaceOrientation", "NSMainNibFile", "NSMainNibFile~ipad", "UIStatusBarHidden",
)


def sha(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def json_hash(value) -> str:
    def typed(other):
        if isinstance(other, bytes):
            return {"$type": "bytes", "bytes": len(other), "sha256": sha(other)}
        if isinstance(other, datetime):
            return {"$type": "datetime", "iso8601": other.isoformat()}
        if isinstance(other, plistlib.UID):
            return {"$type": "plist-uid", "value": other.data}
        raise TypeError("Unsupported metadata type: " + type(other).__name__)
    return sha(json.dumps(value, ensure_ascii=False, sort_keys=True,
                          separators=(",", ":"), default=typed).encode())


def table_tokens(raw: bytes) -> dict:
    """Keep each cell/duplicate/order for comparison; do not return it publicly."""
    plain = decode(raw)
    text = plain.decode("utf-8-sig")
    nonempty = [line.strip() for line in text.splitlines() if line.strip()]
    comments = [line for line in nonempty if line.startswith("//")]
    lines = [line for line in nonempty if not line.startswith("//")]
    if not lines:
        return {"fields": [], "rows": [], "comments": comments,
                "decoded_sha256": sha(plain), "column_anomalies": []}
    fields = [cell.strip() for cell in lines[0].split("\t")]
    rows, anomalies = [], []
    for ordinal, line in enumerate(lines[1:]):
        cells = [cell.strip() for cell in line.split("\t")]
        # Match the existing census' optional trailing empty columns, without
        # throwing away an unexpected nonempty field or silently padding rows.
        while len(cells) > len(fields) and cells[-1] == "":
            cells.pop()
        if len(cells) != len(fields):
            anomalies.append({"row": ordinal, "key": cells[0] if cells else "",
                              "expected": len(fields), "actual": len(cells)})
        rows.append(cells)
    return {"fields": fields, "rows": rows, "comments": comments,
            "decoded_sha256": sha(plain), "column_anomalies": anomalies}


def public_table(t: dict) -> dict:
    keys = [r[0] if r else "" for r in t["rows"]]
    duplicates = {k: v for k, v in Counter(keys).items() if v > 1}
    return {"fields": t["fields"], "rows": len(t["rows"]),
            "unique_first_column_values": len(set(keys)),
            "duplicate_first_column_values": duplicates,
            "comment_lines": len(t["comments"]),
            "comments_sha256": json_hash(t["comments"]),
            "decoded_sha256": t["decoded_sha256"],
            "ordered_active_cells_sha256": json_hash([t["fields"], t["rows"]]),
            "column_anomalies": t["column_anomalies"]}


def png_metadata(data: bytes) -> dict:
    if not data.startswith(b"\x89PNG\r\n\x1a\n"):
        raise ValueError("PNG extension/signature mismatch")
    offset, chunks, ihdr, end = 8, [], None, False
    while offset + 12 <= len(data):
        size = struct.unpack_from(">I", data, offset)[0]
        if offset + size + 12 > len(data):
            raise ValueError("PNG chunk exceeds file")
        kind = data[offset + 4:offset + 8].decode("ascii")
        chunks.append(kind)
        if kind == "IHDR":
            if size != 13:
                raise ValueError("Invalid PNG IHDR length")
            w, h, depth, color, compression, filtering, interlace = struct.unpack_from(
                ">IIBBBBB", data, offset + 8)
            ihdr = {"pixels": [w, h], "bit_depth": depth, "color_type": color,
                    "compression": compression, "filter": filtering,
                    "interlace": interlace}
        offset += size + 12
        if kind == "IEND":
            end = True
            break
    if not ihdr or not end:
        raise ValueError("Incomplete PNG")
    return dict(ihdr, encoding="CgBI PNG" if "CgBI" in chunks else "PNG",
                chunks=chunks, trailing_bytes=len(data) - offset)


def jpeg_metadata(data: bytes) -> dict:
    if not data.startswith(b"\xff\xd8"):
        raise ValueError("JPEG extension/signature mismatch")
    p = 2
    while p < len(data):
        if data[p] != 0xff:
            raise ValueError("Invalid JPEG marker")
        while p < len(data) and data[p] == 0xff:
            p += 1
        if p == len(data):
            raise ValueError("Truncated JPEG marker")
        marker = data[p]
        p += 1
        if marker in (0xd8, 0xd9) or 0xd0 <= marker <= 0xd7:
            continue
        if p + 2 > len(data):
            break
        length = struct.unpack_from(">H", data, p)[0]
        if length < 2 or p + length > len(data):
            raise ValueError("JPEG segment exceeds file")
        if marker in (0xc0, 0xc1, 0xc2, 0xc3, 0xc5, 0xc6, 0xc7,
                      0xc9, 0xca, 0xcb, 0xcd, 0xce, 0xcf):
            depth, h, w, components = struct.unpack_from(">BHHB", data, p + 2)
            return {"encoding": "JPEG", "pixels": [w, h], "bit_depth": depth,
                    "components": components, "sof_marker": hex(marker)}
        if marker == 0xda:
            break
        p += length
    raise ValueError("JPEG dimensions missing")


def wav_metadata(data: bytes) -> dict:
    if data[:4] != b"RIFF" or data[8:12] != b"WAVE":
        raise ValueError("WAV extension/signature mismatch")
    p, fmt, chunks, audio_bytes = 12, None, [], 0
    while p + 8 <= len(data):
        name, size = struct.unpack_from("<4sI", data, p)
        start = p + 8
        if start + size > len(data):
            raise ValueError("WAV chunk exceeds file")
        chunks.append(name.decode("ascii", errors="backslashreplace"))
        if name == b"fmt ":
            if size < 16:
                raise ValueError("Invalid WAV format")
            code, channels, rate, byte_rate, align, bits = struct.unpack_from(
                "<HHIIHH", data, start)
            fmt = {"format_code": code, "channels": channels,
                   "sample_rate": rate, "byte_rate": byte_rate,
                   "block_align": align, "bits_per_sample": bits,
                   "format_bytes": size}
        elif name == b"data":
            audio_bytes += size
        p = start + size + size % 2
    if not fmt:
        raise ValueError("WAV format chunk missing")
    return dict(fmt, audio_data_bytes=audio_bytes, chunks=chunks,
                riff_declared_bytes=struct.unpack_from("<I", data, 4)[0] + 8,
                actual_bytes=len(data), trailing_bytes=max(0, len(data) - p))


def mp3_metadata(data: bytes) -> dict:
    result = {"encoding": "MPEG audio", "id3v2": data.startswith(b"ID3"),
              "id3v1": len(data) >= 128 and data[-128:-125] == b"TAG"}
    start = 0
    if result["id3v2"] and len(data) >= 10:
        size = 0
        for value in data[6:10]:
            size = (size << 7) | (value & 0x7f)
        start = 10 + size
        result.update(id3v2_version=list(data[3:5]), id3v2_bytes=start)
    for p in range(start, min(len(data) - 3, start + 8192)):
        header = int.from_bytes(data[p:p + 4], "big")
        if header & 0xffe00000 != 0xffe00000:
            continue
        version, layer, bitrate, rate = ((header >> 19) & 3, (header >> 17) & 3,
                                         (header >> 12) & 15, (header >> 10) & 3)
        if version == 1 or layer == 0 or bitrate in (0, 15) or rate == 3:
            continue
        result.update(first_frame_offset=p, version={3: "MPEG1", 2: "MPEG2", 0: "MPEG2.5"}[version],
                      layer=4 - layer, bitrate_index=bitrate,
                      sample_rate=[44100, 48000, 32000][rate] // {3: 1, 2: 2, 0: 4}[version],
                      channels=1 if ((header >> 6) & 3) == 3 else 2)
        return result
    result["first_frame_observed"] = False
    return result


def logical_path(path: str, app_prefix: str) -> str:
    if path.startswith(app_prefix):
        return "app/" + path[len(app_prefix):]
    return "package/" + path


def describe_kind(name: str, executable: str) -> str:
    ext = PurePosixPath(name).suffix.lower()
    if name == executable:
        return "executable"
    return {".png": "png", ".jpg": "jpeg", ".jpeg": "jpeg",
            ".wav": "wav", ".mp3": "mp3", ".tble": "table",
            ".tbl": "table", ".mcb": "mcb", ".plist": "plist",
            ".vsh": "shader", ".fsh": "shader", ".nib": "nib",
            ".strings": "strings"}.get(ext, "other")


def inspect_package(source: dict, source_root: Path, version: str) -> dict:
    path = source_root / source["relative_path"]
    before = hashlib.file_digest(path.open("rb"), "sha256").hexdigest()
    if before != source["sha256"]:
        raise ValueError("Original SHA-256 mismatch: " + version)
    members, tables, errors, plists = {}, {}, [], {}
    with zipfile.ZipFile(path) as archive:
        total_entries = len(archive.infolist())
        infos = [x for x in archive.infolist() if not x.is_dir()]
        info_names = [x.filename for x in infos]
        if len(info_names) != len(set(info_names)):
            raise ValueError("Duplicate ZIP member names require occurrence-aware pairing")
        info_paths = [x for x in info_names if re.fullmatch(r"Payload/[^/]+\.app/Info\.plist", x)]
        if len(info_paths) != 1:
            raise ValueError("Expected one app Info.plist")
        app_info = plistlib.loads(archive.read(info_paths[0]))
        prefix = str(PurePosixPath(info_paths[0]).parent) + "/"
        executable = "app/" + app_info["CFBundleExecutable"]
        for member in infos:
            data = archive.read(member)
            name = logical_path(member.filename, prefix)
            kind = describe_kind(name, executable)
            if name == executable:
                name = "app/$EXECUTABLE"
            mode = member.external_attr >> 16
            record = {"archive_path": member.filename, "logical_path": name,
                      "bytes": len(data), "sha256": sha(data),
                      "crc32_verified_by_zip_reader": True,
                      "kind": kind,
                      "zip_member_type": "symlink" if stat.S_ISLNK(mode) else "regular"}
            if name in members:
                raise ValueError("Logical path collision: " + name)
            try:
                kind = record["kind"]
                if kind == "png":
                    record["metadata"] = png_metadata(data)
                elif kind == "jpeg":
                    record["metadata"] = jpeg_metadata(data)
                elif kind == "wav":
                    record["metadata"] = wav_metadata(data)
                elif kind == "mp3":
                    record["metadata"] = mp3_metadata(data)
                elif kind == "table":
                    tables[name] = table_tokens(data)
                    record["metadata"] = public_table(tables[name])
                elif kind == "plist":
                    parsed = plistlib.loads(data)
                    if isinstance(parsed, dict):
                        safe = {k: parsed[k] for k in TECHNICAL_PLIST_KEYS if k in parsed}
                        record["metadata"] = {"keys": sorted(parsed), "technical_values": safe}
                        plists[name] = {k: json_hash(v) for k, v in parsed.items()}
                elif kind == "mcb":
                    record["metadata"] = {"header_first_16_sha256": sha(data[:16]),
                                          "bytes": len(data)}
            except (ValueError, UnicodeError, struct.error, plistlib.InvalidFileException) as error:
                errors.append({"path": name, "error": str(error), "kind": record["kind"]})
            members[name] = record
    after = hashlib.file_digest(path.open("rb"), "sha256").hexdigest()
    if after != before:
        raise ValueError("Original changed during read: " + version)
    return {"id": version, "source_index": VERSION_IDS.index(version),
            "source_relative_path": source["relative_path"], "ipa_sha256": before,
            "ipa_bytes": source["bytes"], "input_unchanged": True,
            "zip_total_entries": total_entries, "zip_directory_entries": total_entries - len(infos),
            "zip_member_count": len(infos), "members": members,
            "app_info": {k: app_info[k] for k in TECHNICAL_PLIST_KEYS if k in app_info},
            "counts_by_kind": dict(sorted(Counter(m["kind"] for m in members.values()).items())),
            "zip_member_type_counts": dict(Counter(m["zip_member_type"] for m in members.values())),
            "parse_errors": errors, "_tables": tables, "_plists": plists}


def identity_columns(table: dict) -> list[int]:
    # Base tables contain repeated Image/Button types with a separate ID. Use
    # that declared pair instead of interpreting the nth Button as a unique ID.
    fields = table["fields"]
    if fields and fields[0] == "Type" and "ID" in fields:
        return [0, fields.index("ID")]
    return [0]


def row_occurrences(table: dict) -> dict:
    counters, out = Counter(), {}
    columns = identity_columns(table)
    for row in table["rows"]:
        key = row[0] if row else ""
        if len(columns) > 1:
            key = "|".join(table["fields"][i] + "=" + (row[i] if i < len(row) else "<missing>") for i in columns)
        ordinal = counters[key]
        counters[key] += 1
        out[(key, ordinal)] = row
    return out


def table_difference(left: dict, right: dict) -> dict:
    lrows, rrows = row_occurrences(left), row_occurrences(right)
    common = sorted(lrows.keys() & rrows.keys())
    changed, field_counts = [], Counter()
    for identity in common:
        lrow, rrow = lrows[identity], rrows[identity]
        if lrow == rrow:
            continue
        fields = []
        for i in range(max(len(lrow), len(rrow))):
            lv, rv = lrow[i] if i < len(lrow) else None, rrow[i] if i < len(rrow) else None
            if lv != rv:
                lf = left["fields"][i] if i < len(left["fields"]) else f"extra_column_{i}"
                rf = right["fields"][i] if i < len(right["fields"]) else f"extra_column_{i}"
                field = lf if lf == rf else f"{lf} -> {rf}"
                fields.append(field)
                field_counts[field] += 1
        changed.append({"key": identity[0], "occurrence": identity[1], "fields": fields})
    return {"header_equal": left["fields"] == right["fields"],
            "active_cells_and_order_equal": left["fields"] == right["fields"] and left["rows"] == right["rows"],
            "comments_equal": left["comments"] == right["comments"],
            "decoded_bytes_equal": left["decoded_sha256"] == right["decoded_sha256"],
            "left": public_table(left), "right": public_table(right),
            "only_left_record_ids": [{"key": k, "occurrence": n} for k, n in sorted(lrows.keys() - rrows.keys())],
            "only_right_record_ids": [{"key": k, "occurrence": n} for k, n in sorted(rrows.keys() - lrows.keys())],
            "changed_common_record_count": len(changed), "changed_records": changed,
            "changed_cells_by_field": dict(sorted(field_counts.items())),
            "row_identity_columns": {"left": [left["fields"][i] for i in identity_columns(left) if i < len(left["fields"])],
                                     "right": [right["fields"][i] for i in identity_columns(right) if i < len(right["fields"])]},
            "row_identity_note": "Declared Type+ID for Base-style schemas, otherwise first column; ordinal occurrences preserve duplicate rows. Static pairing does not assert runtime dispatch identity."}


def pair_packages(left: dict, right: dict, details: bool = True) -> dict:
    l, r = left["members"], right["members"]
    common = sorted(l.keys() & r.keys())
    equal = [p for p in common if l[p]["sha256"] == r[p]["sha256"]]
    different = [p for p in common if l[p]["sha256"] != r[p]["sha256"]]
    kinds = sorted(set(x["kind"] for x in l.values()) | set(x["kind"] for x in r.values()))
    counts = {}
    for kind in kinds:
        lc, rc = {p for p in l if l[p]["kind"] == kind}, {p for p in r if r[p]["kind"] == kind}
        counts[kind] = {"left": len(lc), "right": len(rc), "common": len(lc & rc),
                        "byte_equal": sum(p in lc & rc for p in equal),
                        "byte_different": sum(p in lc & rc for p in different),
                        "only_left": len(lc - rc), "only_right": len(rc - lc)}
    result = {"left": left["id"], "right": right["id"], "counts_by_kind": counts,
              "all_members": {"common": len(common), "byte_equal": len(equal),
                              "byte_different": len(different),
                              "only_left": len(l.keys() - r.keys()),
                              "only_right": len(r.keys() - l.keys())}}
    if not details:
        return result
    result.update(only_left=sorted(l.keys() - r.keys()), only_right=sorted(r.keys() - l.keys()),
                  byte_equal_paths=equal, byte_different_paths=different)
    result["tables"] = {p: table_difference(left["_tables"][p], right["_tables"][p])
                        for p in sorted(left["_tables"].keys() & right["_tables"].keys())}
    result["images"] = {p: {"byte_equal": p in equal, "left": l[p].get("metadata"),
                            "right": r[p].get("metadata"),
                            "dimensions_equal": l[p].get("metadata", {}).get("pixels") == r[p].get("metadata", {}).get("pixels")}
                        for p in common if l[p]["kind"] in ("png", "jpeg")}
    result["audio"] = {p: {"byte_equal": p in equal, "left": l[p].get("metadata"),
                           "right": r[p].get("metadata")}
                       for p in common if l[p]["kind"] in ("wav", "mp3")}
    result["plist_changed_keys"] = {}
    for p in sorted(left["_plists"].keys() & right["_plists"].keys()):
        lp, rp = left["_plists"][p], right["_plists"][p]
        result["plist_changed_keys"][p] = {"only_left": sorted(lp.keys() - rp.keys()),
                                          "only_right": sorted(rp.keys() - lp.keys()),
                                          "changed_common": [k for k in sorted(lp.keys() & rp.keys()) if lp[k] != rp[k]]}
    result["table_summary"] = {
        "common_parsed_tables": len(result["tables"]),
        "active_cells_and_order_equal": sum(x["active_cells_and_order_equal"] for x in result["tables"].values()),
        "comments_different": sum(not x["comments_equal"] for x in result["tables"].values()),
        "header_different": sum(not x["header_equal"] for x in result["tables"].values()),
        "changed_tables": [p for p, x in result["tables"].items() if not x["decoded_bytes_equal"]],
    }
    return result


def summarize_version(version: dict) -> dict:
    members = version["members"]
    return {k: version[k] for k in ("id", "source_index", "source_relative_path", "ipa_sha256",
                                   "ipa_bytes", "input_unchanged", "zip_total_entries", "zip_directory_entries", "zip_member_count", "app_info",
                                   "counts_by_kind", "zip_member_type_counts", "parse_errors")} | {
        "image_encoding_counts": dict(Counter(m.get("metadata", {}).get("encoding", "unparsed")
                                               for m in members.values() if m["kind"] in ("png", "jpeg"))),
        "wav_format_counts": dict(Counter(f"format={m['metadata']['format_code']};channels={m['metadata']['channels']};bits={m['metadata']['bits_per_sample']};rate={m['metadata']['sample_rate']}"
                                           for m in members.values() if m["kind"] == "wav" and "metadata" in m)),
        "tables": {p: public_table(t) for p, t in version["_tables"].items()},
    }


def mission_comparison(left: dict, right: dict) -> dict:
    """Separate changed display-text rows from every remaining mission row."""
    output = {}
    for language in ("kr", "us"):
        names = sorted(p for p in left["_tables"].keys() & right["_tables"].keys()
                       if re.fullmatch(r"app/M\d{6}_" + language + r"\.tble", p))
        changed_categories = Counter()
        byte_equal, text_only, nontext_differences, header_differences = [], [], [], []
        nontext_categories = Counter()
        for name in names:
            lt, rt = left["_tables"][name], right["_tables"][name]
            diff = table_difference(lt, rt)
            for row in diff["changed_records"]:
                changed_categories[row["key"]] += 1
            if not diff["header_equal"]:
                header_differences.append(name)
            lr = [row for row in lt["rows"] if row and row[0] not in ("Name", "AcptComment")]
            rr = [row for row in rt["rows"] if row and row[0] not in ("Name", "AcptComment")]
            nontext_categories.update(row[0] for row in lr)
            if lt["fields"] != rt["fields"] or lr != rr:
                nontext_differences.append(name)
            if left["members"][name]["sha256"] == right["members"][name]["sha256"]:
                byte_equal.append(name)
            elif lt["fields"] == rt["fields"] and lr == rr:
                text_only.append(name)
        output[language] = {"common_mission_files": len(names), "byte_equal_files": len(byte_equal),
                            "text_only_different_files": text_only,
                            "changed_common_rows_by_category": dict(changed_categories),
                            "nontext_categories_compared": dict(nontext_categories),
                            "nontext_rows_and_order_equal_files": len(names) - len(nontext_differences),
                            "nontext_different_files": nontext_differences,
                            "header_different_files": header_differences}
    output["meaning_boundary"] = "Name and AcptComment feed name/comment in the current HD-derived reader (Sources/Core/Progression/Progression.cpp:84). Equality of Require/AcceptAct/CompleteAct and other table tokens does not prove native runtime equivalence."
    return output


def mission_language_comparison(version: dict) -> dict:
    differences = []
    for name, korean in sorted(version["_tables"].items()):
        if not re.fullmatch(r"app/M\d{6}_kr\.tble", name):
            continue
        english_name = name.replace("_kr.tble", "_us.tble")
        english = version["_tables"].get(english_name)
        if english is None:
            differences.append({"mission": PurePosixPath(name).name[:7], "missing_english_file": True})
            continue
        left = {**korean, "rows": [r for r in korean["rows"] if r and r[0] not in ("Name", "AcptComment")]}
        right = {**english, "rows": [r for r in english["rows"] if r and r[0] not in ("Name", "AcptComment")]}
        change = table_difference(left, right)
        if not change["active_cells_and_order_equal"]:
            differences.append({"mission": PurePosixPath(name).name[:7],
                                "changed_rows": change["changed_records"],
                                "only_korean": change["only_left_record_ids"],
                                "only_english": change["only_right_record_ids"]})
    return {"version": version["id"], "nontext_different_missions": differences,
            "meaning_boundary": "This inventories original language-file differences; no rules were corrected or merged."}


def compact_pair(pair: dict, primary=False) -> dict:
    """Avoid repeating full inventories in the repository's review summary."""
    out = {k: pair[k] for k in ("left", "right", "counts_by_kind", "all_members", "only_left", "only_right", "byte_different_paths", "table_summary", "plist_changed_keys")}
    out["tables"] = {}
    for path, table in pair["tables"].items():
        if not primary and table["decoded_bytes_equal"]:
            continue
        row = {k: table[k] for k in ("header_equal", "active_cells_and_order_equal", "comments_equal", "decoded_bytes_equal", "changed_common_record_count", "changed_cells_by_field", "only_left_record_ids", "only_right_record_ids", "row_identity_columns")}
        for side in ("left", "right"):
            row[side] = {k: table[side][k] for k in ("rows", "unique_first_column_values", "comment_lines", "comments_sha256", "decoded_sha256", "ordered_active_cells_sha256", "column_anomalies")}
        if primary and not table["active_cells_and_order_equal"]:
            row["changed_records"] = table["changed_records"]
            row["fields"] = table["left"]["fields"]
        out["tables"][path] = row
    images = pair["images"]
    out["image_summary"] = {
        "common": len(images), "byte_equal": sum(v["byte_equal"] for v in images.values()),
        "dimensions_different": sum(not v["dimensions_equal"] for v in images.values()),
        "encoding_different": sum(v["left"].get("encoding") != v["right"].get("encoding") for v in images.values()),
    }
    if primary:
        out["changed_images"] = {path: {"left_pixels": row["left"].get("pixels"), "right_pixels": row["right"].get("pixels"),
                                         "left_encoding": row["left"].get("encoding"), "right_encoding": row["right"].get("encoding")}
                                 for path, row in images.items() if not row["byte_equal"]}
        out["audio"] = pair["audio"]
        out["difference_controls"] = {}
        for path, control in pair["difference_controls"].items():
            groups = {}
            for version, digest in control["member_hashes"].items():
                groups.setdefault(digest, []).append(version)
            out["difference_controls"][path] = {
                "byte_identity_version_groups": list(groups.values()),
                "hd100_hd103_equal": control["hd100_hd103_equal"],
                "phone_history_has_byte_change": control["phone_history_has_byte_change"],
                "cause": "unresolved",
            }
    if not primary:
        out["unchanged_tables_omitted_from_details"] = len(pair["tables"]) - len(out["tables"])
    return out


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--local-output", type=Path, default=ROOT / ".local/evidence/universal-package-audit-20261113")
    parser.add_argument("--summary", type=Path, default=ROOT / "docs/evidence/universal-package-differences-20261113.json")
    args = parser.parse_args()
    output = args.local_output.resolve()
    if not output.is_relative_to(ROOT / ".local"):
        parser.error("Member inventories must stay in this repository's ignored .local directory")
    summary = args.summary.resolve()
    if not (summary.is_relative_to(ROOT / "docs/evidence") or summary.is_relative_to(ROOT / ".local")):
        parser.error("Summary output must stay in this repository's docs/evidence or .local directory")
    manifest = json.loads((ROOT / "docs/source-provenance.json").read_text())
    if len(manifest["files"]) != len(VERSION_IDS):
        raise ValueError("Expected all six approved inputs")
    versions = [inspect_package(source, Path(manifest["source_root"]), version)
                for source, version in zip(manifest["files"], VERSION_IDS)]
    output.mkdir(parents=True, exist_ok=True)
    for version in versions:
        inventory = {k: v for k, v in version.items() if not k.startswith("_")}
        (output / (version["id"] + "-inventory.json")).write_text(json.dumps(inventory, ensure_ascii=False, indent=2) + "\n")
    pairs = [(3, 5), (0, 1), (1, 2), (2, 3), (4, 5), (3, 4)]
    detailed = {f"{VERSION_IDS[a]}--{VERSION_IDS[b]}": pair_packages(versions[a], versions[b]) for a, b in pairs}
    matrix = [pair_packages(versions[a], versions[b], details=False)
              for a in range(6) for b in range(a + 1, 6)]
    primary = detailed["phone201--hd103"]
    primary["difference_controls"] = {}
    for name in primary["byte_different_paths"]:
        history = {v["id"]: v["members"].get(name, {}).get("sha256") for v in versions}
        primary["difference_controls"][name] = {
            "member_hashes": history,
            "hd100_hd103_equal": history["hd100"] is not None and history["hd100"] == history["hd103"],
            "phone_history_has_byte_change": len({history[v] for v in VERSION_IDS[:4] if history[v] is not None}) > 1,
            "cause": "unresolved: cross-family and version effects are not causally separable from package identity alone",
        }
    previous = json.loads((ROOT / "docs/evidence/standard-hd-package-metadata.json").read_text())
    checks = {"same_png_count_matches_previous": primary["counts_by_kind"]["png"]["byte_equal"] == previous["stats"][".png"]["identical"],
              "different_png_count_matches_previous": primary["counts_by_kind"]["png"]["byte_different"] == previous["stats"][".png"]["changed"],
              "resized_png_count_matches_previous": sum(not x["dimensions_equal"] for p, x in primary["images"].items() if p.endswith(".png")) == previous["resized_count"]}
    old_versions = json.loads((ROOT / "docs/evidence/ipa-version-audit-summary.json").read_text())["versions"]
    for version, old in zip(versions, old_versions):
        counts_match = sum(p.endswith(".tble") for p in version["_tables"]) == old["decoded_tble_count"]
        hashes_match = all(version["members"]["app/" + name]["sha256"] == hashes["encoded_sha256"] and
                           version["_tables"]["app/" + name]["decoded_sha256"] == hashes["decoded_sha256"]
                           for name, hashes in old["core_table_hashes"].items())
        checks[version["id"] + "_prior_table_count_and_core_hashes_match"] = (
            counts_match and hashes_match and version["source_relative_path"] == old["file"] and
            version["ipa_sha256"] == old["ipa_sha256"])
    (output / "comparisons-full.json").write_text(json.dumps(detailed, ensure_ascii=False, indent=2) + "\n")
    version_summaries = [summarize_version(v) for v in versions]
    for version in version_summaries:
        tables = version.pop("tables")
        version["table_count"] = len(tables)
        version["table_column_anomalies"] = {p: t["column_anomalies"] for p, t in tables.items() if t["column_anomalies"]}
    result = {
        "schema": "hm2-universal-package-differences-v1", "recorded_on": "2026-11-13",
        "scope": "All nondirectory members of six approved local IPAs; static metadata/token comparison only",
        "original_bytes_exported": False, "originals_unchanged": all(v["input_unchanged"] for v in versions),
        "versions": version_summaries, "comparisons": {key: compact_pair(pair, key == "phone201--hd103") for key, pair in detailed.items()},
        "primary_mission_classification": mission_comparison(versions[3], versions[5]),
        "within_version_mission_language_comparison": [mission_language_comparison(v) for v in versions],
        "local_complete_inventory": ".local/evidence/universal-package-audit-20261113/ (six member inventories and comparisons-full.json)",
        "all_pair_count_matrix": matrix, "prior_evidence_cross_checks": checks,
        "limits": ["Byte equality proves the compared bytes match, not executable behavior.",
                   "Table token equality excludes comments and whitespace only where explicitly labeled.",
                   "Table pairing uses Type+ID for Base-style rows, otherwise first-column values plus occurrence; runtime dispatch still needs code analysis.",
                   "Image dimensions/encoding do not prove equal decoded pixels, alpha margins, anchors or displayed scale.",
                   "Audio file/format equality does not prove equal triggers, timing, mixing or lifecycle behavior.",
                   "Different versions are not controlled experiments for attributing changes to iPhone/iPad.",
                   "No apps, simulators, game builds, devices or native sessions were run by this tool."],
    }
    summary.parent.mkdir(parents=True, exist_ok=True)
    summary.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n")
    print(json.dumps({"versions": len(versions), "members": sum(v["zip_member_count"] for v in versions),
                      "primary_counts": primary["counts_by_kind"], "primary_table_summary": primary["table_summary"],
                      "parse_errors": {v["id"]: v["parse_errors"] for v in versions if v["parse_errors"]},
                      "cross_checks": checks}, ensure_ascii=False, indent=2))
    if any(v["parse_errors"] for v in versions) or not all(checks.values()):
        raise SystemExit(1)


if __name__ == "__main__":
    main()
