"""Compare real PPC COFF function words against the original loaded ARTIST image."""
import difflib
import hashlib
import json
from pathlib import Path
import re
import struct
import sys


here = Path(__file__).resolve().parent
wave = here.parent
workflow = Path("D:/Reverse/BP-Decomp_Workflow")
sys.path.insert(0, str(workflow / "tools/re"))
import x360rd

symbol = b"?Is2DPointOnScreen@Debug2DImmediateRender@CgsDev@@"
export = workflow / ".ida-exports/BURNOUT_X360_ARTIST.XEX/0x82818838.json"
listing = json.loads(export.read_text(encoding="utf-8"))["assembly"]
addresses = [int(m[1], 16) for line in listing.splitlines()
             if (m := re.match(r"^0x([0-9A-Fa-f]+)\s+(?!\.)\w", line))]
start = addresses[0]
assert addresses == list(range(start, start + 4 * len(addresses), 4))
original = x360rd.rd(start, 4 * len(addresses))
assert original[:4].hex() == "1000038c" and original[-4:].hex() == "4e800020"


def words(data):
    assert len(data) % 4 == 0
    return [data[i:i + 4].hex() for i in range(0, len(data), 4)]


def read_function(path):
    data = path.read_bytes()
    machine, count, _, symbols, n_symbols, optional, _ = struct.unpack_from("<HHIIIHH", data)
    assert machine == 0x1f2 and optional == 0
    strings = symbols + 18 * n_symbols
    index = 0
    matches = []
    while index < n_symbols:
        pos = symbols + index * 18
        short = data[pos:pos + 8]
        if short[:4] == bytes(4):
            offset = struct.unpack_from("<I", short, 4)[0] + strings
            name = data[offset:data.index(bytes(1), offset)]
        else:
            name = short.rstrip(bytes(1))
        value, section, _, storage, aux = struct.unpack_from("<IhHBB", data, pos + 8)
        if name.startswith(symbol) and 1 <= section <= count and storage == 2:
            matches.append((name, section, value))
        index += 1 + aux
    name, section, value = matches[0]
    assert len(matches) == 1 and value == 0
    _, _, _, size, raw, reloc, _, n_reloc, _, _ = struct.unpack_from(
        "<8sIIIIIIHHI", data, 20 + 40 * (section - 1)
    )
    return name.decode(), data[raw:raw + size], [struct.unpack_from("<IIH", data, reloc + 10 * i)
                                               for i in range(n_reloc)]


rows = []
for path in (wave / "r115-intake/native-selected/CgsDebug2DImmediateRender-5cb1111c.obj",
             here / (sys.argv[1] if len(sys.argv) > 1 else "debug2d.obj")):
    name, actual, relocs = read_function(path)
    match = difflib.SequenceMatcher(None, words(original), words(actual), autojunk=False)
    row = {"object": str(path), "symbol": name, "words": len(actual) // 4,
           "original_words": len(original) // 4, "aligned_identical_words": sum(x.size for x in match.get_matching_blocks()),
           "raw_identical": original == actual, "relocation_count": len(relocs),
           "sha256": hashlib.sha256(actual).hexdigest(), "relocations": relocs}
    rows.append(row)
    print(json.dumps(row))
    if path.parent == here:
        print("ARTIST:", " ".join(words(original)))
        print("built :", " ".join(words(actual)))
print("ARTIST", hex(start), len(original), hashlib.sha256(original).hexdigest())
