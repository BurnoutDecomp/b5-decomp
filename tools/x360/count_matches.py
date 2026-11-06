"""Compare named linked Xbox 360 functions with the ARTIST reference."""
import argparse
import collections
import ctypes
import gzip
import hashlib
import json
from pathlib import Path
import re
import struct
import runpy

import pefile

MAP_ROW = re.compile(r"^\s*0003:[0-9a-fA-F]{8}\s+(\S+)\s+([0-9a-fA-F]{8,16})\s+f\s+(?:i\s+)?(\S+)")
ASSEMBLY_ROW = re.compile(r"^0x([0-9a-fA-F]+)\s+\S+")


def artist_inputs(workflow):
    workflow = workflow.resolve()
    reader = runpy.run_path(str(workflow / "tools/re/x360rd.py"))["rd"]
    exports = workflow / ".ida-exports/BURNOUT_X360_ARTIST.XEX"
    names = workflow / "progress/asmaudit_artist.json.gz"
    return reader, exports, names


def read_json(path):
    return json.loads(path.read_bytes())


def sha(data):
    return hashlib.sha256(data).hexdigest()


def packed_extents(data, start, end):
    # Standard two-word PPC .pdata records. Exclude other unwind encodings.
    entries = list(struct.iter_unpack(">II", data))
    sizes = {}
    for index, (address, flags) in enumerate(entries):
        length = ((flags >> 8) & 0x3FFFFF) * 4
        next_address = entries[index + 1][0] if index + 1 < len(entries) else end
        if flags >> 30 == 1 and address % 4 == 0 and length:
            if start <= address < address + length <= min(next_address, end):
                assert address not in sizes
                sizes[address] = length
    return sizes


def map_functions(map_data):
    names = collections.defaultdict(set)
    symbols = collections.defaultdict(set)
    undecorate = ctypes.WinDLL("dbghelp").UnDecorateSymbolName
    undecorate.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_uint, ctypes.c_uint]
    buffer = ctypes.create_string_buffer(8192)
    for line in map_data.decode(errors="replace").splitlines():
        row = MAP_ROW.match(line)
        if not row:
            continue
        mangled, address_text, owner = row.groups()
        address = int(address_text, 16)
        symbols[address].add(mangled)
        name = mangled
        if mangled.startswith("?"):
            if not undecorate(mangled.encode(), buffer, len(buffer), 0x1000):
                continue
            name = buffer.value.decode(errors="replace")
        names[name].add((address, owner, mangled))
    return names, symbols


def branch_target(word, address, mask):
    displacement = word & mask
    sign_bit = (mask + 4) >> 1
    if displacement & sign_bit:
        displacement -= sign_bit * 2
    return (displacement + (0 if word & 2 else address)) & 0xFFFFFFFF


def match_branches(original_body, compiled_body, original_start, compiled_start, targets):
    # Only direct PPC branch displacement fields may vary. Compare all targets.
    for offset, (a, b) in enumerate(zip(struct.iter_unpack(">I", original_body),
                                       struct.iter_unpack(">I", compiled_body))):
        old_word, new_word = a[0], b[0]
        mask = {16: 0x0000FFFC, 18: 0x03FFFFFC}.get(old_word >> 26)
        if mask is None:
            if old_word != new_word:
                return False
            continue
        if old_word & ~mask != new_word & ~mask:
            return False
        old_target = branch_target(old_word, original_start + 4 * offset, mask)
        new_target = branch_target(new_word, compiled_start + 4 * offset, mask)
        local = old_target - original_start
        if 0 <= local < len(original_body):
            if new_target != compiled_start + local:
                return False
        elif targets.get(old_target) != new_target:
            return False
    return True


def compare(link, output, workflow):
    read_artist, exports, names_path = artist_inputs(workflow)
    pe_path, map_path = link / "native-game.pe", link / "native-game.map"
    pe_data, map_data = pe_path.read_bytes(), map_path.read_bytes()
    result = read_json(link / "result.json")
    seal_path = link / "seal.json"
    seal = read_json(seal_path)
    assert result["exit_code"] == 0 and result["all_post_checks_pass"]
    expected = {Path(name).name: row for name, row in result["outputs"].items()}
    for path, data in ((pe_path, pe_data), (map_path, map_data)):
        assert expected[path.name]["sha256"] == sha(data)
    pe = pefile.PE(data=pe_data, fast_load=True)
    base = pe.OPTIONAL_HEADER.ImageBase
    sections = {section.Name.rstrip(b"\0"): section for section in pe.sections}
    text = sections[b".text"]
    pdata = sections[b".pdata"]
    image = pe.get_memory_mapped_image()
    compiled_sizes = packed_extents(pdata.get_data()[:pdata.Misc_VirtualSize],
                                    base + text.VirtualAddress,
                                    base + text.VirtualAddress + text.Misc_VirtualSize)

    original_pdata = read_artist(0x821BB000, 0x821ED960 - 0x821BB000)
    original_sizes = packed_extents(original_pdata, 0x821F0000, 0x82C775D0)
    assert [original_sizes[address] for address in (0x823EE4D8, 0x82682898, 0x826829C8)] == [168, 300, 88]
    original_names = collections.defaultdict(list)
    for address, row in json.loads(gzip.decompress(names_path.read_bytes()))["functions"].items():
        original_names[row["name"]].append(int(address, 16))
    linked_names, linked_symbols = map_functions(map_data)
    pairs = {}
    skipped = collections.Counter()
    for name, addresses in original_names.items():
        if len(addresses) != 1 or len(linked_names.get(name, ())) != 1:
            skipped["no unique name pair"] += 1
            continue
        address, owner, _ = next(iter(linked_names[name]))
        if len(linked_symbols[address]) != 1:
            skipped["linked aliases"] += 1
            continue
        pairs[name] = (addresses[0], address, owner)

    targets = {old: new for old, new, _ in pairs.values()}
    source_by_object = {Path(row["object"]).name: row["source"] for row in seal["selected"]}
    totals = collections.Counter()
    exact = []
    branch_only = []
    for name, (old, new, owner) in pairs.items():
        old_size, new_size = original_sizes.get(old), compiled_sizes.get(new)
        if old_size is None or new_size is None:
            skipped["missing packed unwind extent"] += 1
            continue
        source = source_by_object.get(owner, "")
        if not source.startswith("src/"):
            skipped["outside selected src owners"] += 1
            continue
        export = read_json(exports / f"0x{old:08X}.json")
        addresses = [int(row[1], 16) for line in export["assembly"].splitlines()
                     if (row := ASSEMBLY_ROW.match(line))]
        if export["name"] != name or addresses != list(range(old, old + old_size, 4)):
            skipped["original export extent or name disagrees"] += 1
            continue
        totals[source.split("/")[1]] += 1
        if old_size != new_size:
            continue
        original_body = read_artist(old, old_size)
        compiled_body = image[new - base:new - base + new_size]
        row = {"name": name, "original": f"{old:08X}", "linked": f"{new:08X}",
               "bytes": old_size, "source": source,
               "original_sha256": sha(original_body), "linked_sha256": sha(compiled_body)}
        if original_body == compiled_body:
            exact.append(row)
        elif match_branches(original_body, compiled_body, old, new, targets):
            branch_only.append(row)

    report = {"head": seal["head"], "pe_sha256": sha(pe_data), "map_sha256": sha(map_data),
              "seal_sha256": sha(seal_path.read_bytes()), "original_pdata_sha256": sha(original_pdata),
              "original_name_pack_sha256": sha(names_path.read_bytes()), "eligible": sum(totals.values()),
              "eligible_by_tree": totals, "excluded_name_counts": skipped,
              "raw_exact": len(exact), "raw_exact_bytes": sum(row["bytes"] for row in exact),
              "verified_branch_displacement_only": len(branch_only),
              "raw_exact_functions": exact, "branch_displacement_only_functions": branch_only}
    with output.open("x", encoding="utf-8") as file:
        json.dump(report, file, indent=2)
        file.write("\n")
    print(json.dumps({key: value for key, value in report.items() if not key.endswith("_functions")}, indent=2))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("link", type=Path, help="Completed strict link directory")
    parser.add_argument("output", type=Path, help="New census JSON")
    parser.add_argument("--workflow", required=True, type=Path,
                        help="BP-Decomp_Workflow checkout with the ARTIST exports and reader")
    args = parser.parse_args()
    compare(args.link.resolve(), args.output, args.workflow)


if __name__ == "__main__":
    main()