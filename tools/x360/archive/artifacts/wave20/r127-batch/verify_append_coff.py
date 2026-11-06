"""Verify both append functions, ignoring only the resolved strncat branch displacement."""
import hashlib
import json
from pathlib import Path
import struct
import sys

here = Path(__file__).resolve().parent
wave = here.parent
reader = wave / "r120-debug2d-match/compare.py"
coff = {"__file__": str(reader)}
exec(reader.read_text(encoding="utf-8-sig").split("\nrows = []", 1)[0], coff)
read_original = coff["x360rd"].rd
assert read_original(0x82f241fc, 1) == b"\x01" and coff["x360rd"].f32(0x82f240b8) == 550.0

def verify(path):
    result = {}
    data = path.read_bytes()
    _, _, _, symbol_table, count, _, _ = struct.unpack_from("<HHIIIHH", data)
    for method, artist_address, size in (("StrStream", 0x82815e80, 56), ("SimpleStrStream", 0x82815ec8, 48)):
        coff["symbol"] = f"?Append@{method}@CgsDev@@".encode()
        _, compiled, relocations = coff["read_function"](path)
        offset, symbol, kind = relocations[0]
        assert relocations == [(size - 4, symbol, 6)] and len(compiled) == size
        assert symbol < count
        name = data[symbol_table + symbol * 18:symbol_table + symbol * 18 + 8].rstrip(b"\0")
        assert name == b"strncat"
        original = read_original(artist_address, size)
        assert compiled[:-4] == original[:-4]
        before, after = struct.unpack(">I", original[-4:])[0], struct.unpack(">I", compiled[-4:])[0]
        assert (before & ~0x03fffffc) == (after & ~0x03fffffc) == 0x48000000
        branch_offset = before & 0x03fffffc
        target = artist_address + size - 4 + branch_offset - (0x04000000 if branch_offset & 0x02000000 else 0)
        assert target == 0x82c0cb20
        result[method] = {"artist_address": hex(artist_address), "bytes": size,
                          "raw_identical_prefix": size - 4, "one_branch24_relocation": True,
                          "artist_strncat_va": hex(target), "exact_after_address_fields": True}
    return {"object": str(path.resolve()), "sha256": hashlib.sha256(data).hexdigest(), "methods": result}

if __name__ == "__main__":
    proof = verify(Path(sys.argv[1]))
    with (here / "R127_APPEND_COFF.json").open("x", encoding="utf-8") as stream:
        json.dump(proof, stream, indent=2)
        stream.write("\n")
    print(json.dumps(proof))
