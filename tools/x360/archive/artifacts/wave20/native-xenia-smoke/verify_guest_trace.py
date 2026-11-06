import argparse
import hashlib
import json
import pathlib
import re
import struct

SYMBOLS = {
    "crt_entry": "mainCRTStartup",
    "game_main": "main",
    "game_construct": "?Construct@BrnGameModule@",
    "game_prepare": "?Prepare@BrnGameModule@",
    "game_update": "?Update@BrnGameModule@",
}

def sha(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()

def linked_symbols(map_path):
    selected = {}
    for line in map_path.open(encoding="latin1"):
        match = re.match(r"^\s*[0-9A-Fa-f]+:[0-9A-Fa-f]+\s+(\S+)\s+([0-9A-Fa-f]{8})\s+f\b", line)
        if not match:
            continue
        symbol, address = match.groups()
        for label, query in SYMBOLS.items():
            if (symbol.startswith(query) if query.startswith("?") else symbol == query):
                value = (symbol, int(address, 16))
                assert label not in selected or selected[label] == value, (label, value)
                selected[label] = value
    assert selected.keys() == SYMBOLS.keys(), selected
    return selected

def execution_count(chunk, address):
    needle = struct.pack("<I", address)
    offset = chunk.find(needle, 4)
    executed = 0
    while offset >= 0:
        if offset % 4 == 0 and offset + 8 <= len(chunk):
            size, start, end = struct.unpack_from("<III", chunk, offset - 4)
            body = offset - 4 + 56
            limit = offset - 4 + size
            if start < end and (end - start) % 4 == 0 and size == 56 + ((end - start) // 4) * 8:
                if 64 <= size <= 5_000_000 and limit <= len(chunk):
                    executed += sum(value[0] != 0 for value in struct.iter_unpack("<Q", chunk[body:limit]))
        offset = chunk.find(needle, offset + 4)
    return executed

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--run", type=pathlib.Path, required=True)
    args = parser.parse_args()
    run = args.run.resolve()
    staged = json.loads((run / "staged.json").read_text())
    process = json.loads((run / "process-result.json").read_text())
    result_path = pathlib.Path(staged["strict_link_result"])
    assert sha(result_path) == staged["strict_link_result_sha256"]
    result = json.loads(result_path.read_text())
    maps = [pathlib.Path(p) for p in result["outputs"] if p.endswith(".map")]
    assert len(maps) == 1 and sha(maps[0]) == result["outputs"][str(maps[0])]["sha256"]
    assert sha(pathlib.Path(staged["image"])) == staged["image_sha256"]
    symbols = linked_symbols(maps[0])
    hits = {label: 0 for label in symbols}
    for path in sorted((run / "funcdata").iterdir()):
        if path.is_file() and re.search(r"\.[0-9]+$", path.name):
            chunk = path.read_bytes()
            for label, (_, address) in symbols.items():
                hits[label] += execution_count(chunk, address)
    report = {
        "staged_image_sha256": staged["image_sha256"],
        "map_sha256": sha(maps[0]),
        "process": process,
        "guest_functions": {
            label: {"symbol": symbol, "address": hex(address),
                    "nonzero_instruction_counters": hits[label]}
            for label, (symbol, address) in symbols.items()
        },
    }
    with (run / "guest-execution.json").open("x", encoding="utf-8") as stream:
        json.dump(report, stream, indent=2)
        stream.write("\n")
    print(json.dumps(report, indent=2))

if __name__ == "__main__":
    main()
