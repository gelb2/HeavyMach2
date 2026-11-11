#!/usr/bin/env python3
"""Index local HD method evidence and candidate references in the restoration."""

import argparse
from collections import Counter, defaultdict
import csv
import json
from pathlib import Path
import re


REPO = Path(__file__).resolve().parents[1]
HD103_SHA = "5feb7ae5f796cdfee5d650037923d338ce7b28de7f600c8548a7a5bbf7ef5090"
ADDRESS = re.compile(r"0x[0-9a-fA-F]{4,8}(?![0-9a-fA-F])")
SOURCE_SUFFIXES = {".cpp", ".hpp", ".h", ".c", ".m", ".mm", ".swift"}
CALL_GROUPS = {
    "button": {"initWithTile:TileNo:ID:", "initWithTile:TileNo:", "setButtonId:", "handleButton:"},
    "asset": {"getTile:", "getUniTile:", "getAlphaTile:", "getImage:", "loadChunkWithName:"},
    "sound": {"getSound:", "play:", "play:X:Y:", "playBGM:", "initWithContentsOfFile:"},
    "item": {"getItemInfo:", "spawnItem", "useItem:", "initWithItem:", "listItem", "addItem:Count:"},
    "save": {"saveDataFile", "openDataFile", "initSlotWithName:Country:", "selectSlot:", "slotData"},
}
SELECTOR = re.compile(r'"([A-Za-z_][A-Za-z_0-9:]*)"')
BUTTON_ID_SELECTORS = {"initWithTile:TileNo:ID:", "setButtonId:"}
NUMBER = re.compile(r"(?:0[xX][0-9a-fA-F]+|[0-9]+)")


def read_json(path):
    return json.loads(path.read_text(encoding="utf-8"))


def address_references(folders, addresses):
    candidates = defaultdict(list)
    for folder in folders:
        for source in sorted((REPO / folder).rglob("*")):
            if source.suffix not in SOURCE_SUFFIXES or not source.is_file():
                continue
            for line_number, line in enumerate(source.read_text(encoding="utf-8").splitlines(), 1):
                _, delimiter, comment = line.partition("//")
                text = comment if delimiter else line if "ScreenId::" in line and '"' in line else ""
                for address in dict.fromkeys(match.lower() for match in ADDRESS.findall(text)):
                    if address in addresses:
                        candidates[address].append(f"{source.relative_to(REPO)}:{line_number}")
    return candidates


def export_status(analysis, name):
    path = analysis / name / "export-manifest.json"
    if not path.exists():
        return {}
    manifest = read_json(path)
    if manifest["source_sha256"] != HD103_SHA:
        raise ValueError(f"{name} must come from HD 1.0.3")
    return {method["address"]: method["exports"] for method in manifest["methods"]}


def button_argument(lines, index, selector):
    # Ghidra wraps long Objective-C calls across several lines.
    statement = " ".join(lines[index:index + 5]).split(";", 1)[0].rstrip()
    before, separator, last_arg = statement.rpartition(",")
    if not separator or f'"{selector}"' not in before or not last_arg.endswith(")"):
        return "unresolved", ""
    argument = last_arg[:-1].strip()
    if NUMBER.fullmatch(argument):
        return "fixed", int(argument, 16 if argument.lower().startswith("0x") else 10)
    return "expression", argument


def index_calls(analysis, units):
    counts = Counter()
    buttons = Counter()
    with (
        (analysis / "restoration-call-sites.tsv").open("w", encoding="utf-8", newline="") as calls,
        (analysis / "restoration-button-ids.tsv").open("w", encoding="utf-8", newline="") as ids,
    ):
        writer = csv.writer(calls, delimiter="\t", lineterminator="\n")
        button_writer = csv.writer(ids, delimiter="\t", lineterminator="\n")
        writer.writerow(["group", "class", "method", "address", "selector", "pseudocode_line"])
        button_writer.writerow(["class", "method", "address", "pseudocode_line", "kind", "id_or_expression"])
        for location, cls, method, address in units:
            path = analysis / location / "functions" / f"{address}.pseudo.c"
            if not path.is_file():
                continue
            lines = path.read_text(encoding="utf-8").splitlines()
            for index, line in enumerate(lines):
                line_number = index + 1
                for selector in SELECTOR.findall(line):
                    if selector in BUTTON_ID_SELECTORS:
                        kind, value = button_argument(lines, index, selector)
                        button_writer.writerow([cls, method, address, line_number, kind, value])
                        buttons[kind] += 1
                    for group, accepted in CALL_GROUPS.items():
                        if selector in accepted:
                            writer.writerow([group, cls, method, address, selector, line_number])
                            counts[group] += 1
    return dict(counts), dict(buttons)


def audit(analysis):
    metadata = read_json(analysis / "objc-layouts.json")
    evidence = read_json(analysis / "rea-procedure-inventory.json")
    if metadata["sha256"] != HD103_SHA or evidence["subject"]["digest"]["sha256"] != HD103_SHA:
        raise ValueError("Objective-C metadata and REA evidence must both come from HD 1.0.3")
    procedures = evidence["normalized_result"]
    entries = {row["address"]: row for row in procedures if not row["procedure"]["external"]}
    methods = [
        (cls["name"], kind, method)
        for cls in metadata["classes"]
        for field, kind in (("methods", "instance"), ("class_methods", "class"))
        for method in cls[field]
    ]
    addresses = {method["address"] for _, _, method in methods}
    source_refs = address_references(("Sources",), addresses)
    test_refs = address_references(("tests", "Apple/Tests", "Apple/UITests"), addresses)
    exports = export_status(analysis, "method-evidence")
    native = {address: row for address, row in entries.items()
              if address not in addresses and not row["procedure"]["thunk"]}
    native_exports = export_status(analysis, "native-evidence")
    call_sites, button_ids = index_calls(analysis, [
        *(("method-evidence", cls, method["selector"], method["address"]) for cls, _, method in methods),
        *(("native-evidence", "Native", row["value"], address) for address, row in native.items()),
    ])

    with (analysis / "restoration-method-index.tsv").open("w", encoding="utf-8", newline="") as file:
        writer = csv.writer(file, delimiter="\t", lineterminator="\n")
        writer.writerow([
            "class", "kind", "selector", "address", "rea_procedure", "assembly",
            "pseudocode", "source_address_references", "test_address_references",
        ])
        for cls, kind, method in methods:
            address = method["address"]
            exported = exports.get(address, {})
            writer.writerow([
                cls, kind, method["selector"], address, entries.get(address, {}).get("value", ""),
                exported.get("assembly", {}).get("status", "pending"),
                exported.get("pseudo", {}).get("status", "pending"),
                " ".join(source_refs.get(address, ())), " ".join(test_refs.get(address, ())),
            ])

    summary = {
        "binary_sha256": HD103_SHA,
        "rea_inventory_evidence": evidence["evidence_id"],
        "classes": len(metadata["classes"]),
        "method_declarations": len(methods),
        "method_addresses": len(addresses),
        "method_addresses_in_rea": len(addresses & entries.keys()),
        "method_addresses_missing_in_rea": sorted(addresses - entries.keys()),
        "rea_procedures": dict(Counter(
            "external" if row["procedure"]["external"] else
            "thunk" if row["procedure"]["thunk"] else "body"
            for row in procedures
        )),
        "method_exports": {
            kind: dict(Counter(exports.get(address, {}).get(kind, {}).get("status", "pending")
                               for address in addresses))
            for kind in ("assembly", "pseudo")
        },
        "native_bodies_outside_methods": len(native),
        "native_exports": {
            kind: dict(Counter(native_exports.get(address, {}).get(kind, {}).get("status", "pending")
                               for address in native))
            for kind in ("assembly", "pseudo")
        },
        "selector_mentions_to_review": call_sites,
        "button_id_arguments_to_review": button_ids,
        "addresses_cited_in_source": len(source_refs),
        "addresses_cited_in_tests": len(test_refs),
        "note": "Address references and selector mentions are review leads. They do not measure implementation or test coverage.",
    }
    result = json.dumps(summary, ensure_ascii=False, indent=2) + "\n"
    (analysis / "restoration-method-summary.json").write_text(result, encoding="utf-8")
    print(result, end="")
    return not summary["method_addresses_missing_in_rea"]


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("analysis", type=Path)
    directory = parser.parse_args().analysis.resolve()
    if directory.is_relative_to(REPO):
        parser.error("Raw analysis output must stay outside the repository")
    raise SystemExit(0 if audit(directory) else 1)
