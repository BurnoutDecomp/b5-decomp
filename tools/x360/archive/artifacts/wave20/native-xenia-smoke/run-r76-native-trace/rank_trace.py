import json
import pathlib
import re
import struct

run = pathlib.Path(__file__).resolve().parent
stage = json.loads((run / "staged.json").read_text())
result = json.loads(pathlib.Path(stage["strict_link_result"]).read_text())
map_path, = (pathlib.Path(path) for path in result["outputs"] if path.endswith(".map"))
names = {}
for line in map_path.read_text(encoding="latin1").splitlines():
    match = re.match(r"^\s*[0-9A-Fa-f]+:[0-9A-Fa-f]+\s+(\S+)\s+([0-9A-Fa-f]{8})\s+f\b\s+(.*)", line)
    if match:
        symbol, address, owner = match.groups()
        names.setdefault(int(address, 16), []).append((symbol, owner))
records = []
for path in sorted((run / "funcdata").iterdir()):
    data = path.read_bytes()
    index = 0
    while index + 64 <= len(data):
        size, start, end = struct.unpack_from("<III", data, index)
        if (0x82000000 <= start < end < 0x84000000 and (end-start)%4 == 0
                and size == 56 + ((end-start)//4)*8 and index+size <= len(data)):
            counts = tuple(n[0] for n in struct.iter_unpack("<Q", data[index+56:index+size]))
            if any(counts):
                positive = [i for i, n in enumerate(counts) if n]
                records.append({"start": hex(start), "end": hex(end), "symbols": names.get(start, []),
                                "total": sum(counts), "max": max(counts), "hits": len(positive),
                                "last_nonzero": hex(start+4*positive[-1]),
                                "hot_address": hex(start+4*counts.index(max(counts)))})
            index += size
        elif not any(data[index:index+256]):
            index += 256
        else:
            index += 4
with (run / "startup-trace-functions.json").open("x", encoding="utf-8") as stream:
    json.dump(records, stream, indent=2)
    stream.write("\n")
print("functions with nonzero counts:", len(records))
for r in sorted(records, key=lambda x: x["max"], reverse=True)[:16]:
    print("HOT", r["start"], r["max"], r["hot_address"], r["last_nonzero"], r["symbols"][:3])
for r in records[-18:]:
    print("ORDER", r["start"], r["last_nonzero"], r["max"], r["symbols"][:3])
