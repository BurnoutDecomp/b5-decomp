"""Compare the AddCommand COFF code and resolve its local jump table."""
import json
from pathlib import Path
import struct
import sys

here = Path(__file__).resolve().parent
wave = here.parent
workflow = Path("D:/Reverse/BP-Decomp_Workflow")
sys.path.insert(0, str(workflow / "tools/re"))
import x360rd

start = 0x823AAD48
original = x360rd.rd(start, 0xC4)
assert original[:12].hex() == "3964ffff2b0b00044d990020"
orig_words = struct.unpack(">49I", original)
original_targets = [addr - start for addr in orig_words[9:14]]
assert original_targets == [0x44, 0x38, 0x60, 0xA0, 0x7C]
assert [x360rd.u32(addr) for addr in (0x82001CC0, 0x82009B84)] == [0, 0x3F99999A]

masks = {0x0002: 0xffffffff, 0x0006: 0x03fffffc, 0x0010: 0xffff, 0x0011: 0xffff, 0x0012: 0}


def compare(path):
    data = path.read_bytes()
    machine, n_sections, _, symbols, n_symbols, optional, _ = struct.unpack_from("<HHIIIHH", data)
    assert machine == 0x1f2 and optional == 0
    strings = symbols + 18 * n_symbols
    index = 0
    symbol_by_index = {}
    functions = []
    while index < n_symbols:
        pos = symbols + index * 18
        short = data[pos:pos + 8]
        if short[:4] == bytes(4):
            offset = struct.unpack_from("<I", short, 4)[0] + strings
            name = data[offset:data.index(bytes(1), offset)].decode()
        else:
            name = short.rstrip(bytes(1)).decode()
        value, section, _, storage, aux = struct.unpack_from("<IhHBB", data, pos + 8)
        symbol_by_index[index] = (name, value, section)
        if name.startswith("?AddCommand@LoadingScreenRenderer@BrnGame@@") and 1 <= section <= n_sections and storage == 2:
            functions.append((section, value))
        index += 1 + aux
    (section, value), = functions
    assert value == 0
    _, _, _, size, raw, reloc, _, n_reloc, _, _ = struct.unpack_from(
        "<8sIIIIIIHHI", data, 20 + 40 * (section - 1)
    )
    body = data[raw:raw + size]
    words = struct.unpack(">" + "I" * (size // 4), body)
    word_masks = {}
    targets = {}
    switch_base = {}
    literals = {}
    for i in range(n_reloc):
        offset, target, kind = struct.unpack_from("<IIH", data, reloc + 10 * i)
        assert kind in masks, (offset, kind)
        word_masks[offset // 4] = word_masks.get(offset // 4, 0) | masks[kind]
        if kind != 0x0012:
            name, addend, target_section = symbol_by_index[target]
            if offset in (0x0c, 0x10):
                assert kind in (0x0010, 0x0011) and target_section == section
                switch_base[offset] = addend
            if 0x24 <= offset <= 0x34:
                assert kind == 0x0002 and target_section == section
                targets[offset] = addend + words[offset // 4]
            if name.startswith("__real@"):
                literals[offset] = name.removeprefix("__real@")
    diffs = [(i*4, f"{old:08X}", f"{new:08X}", f"{word_masks.get(i,0):08X}")
             for i, (old, new) in enumerate(zip(orig_words, words))
             if ((old ^ new) & ~word_masks.get(i, 0))]
    return {"object": str(path), "artist_bytes": len(original), "object_bytes": len(body),
            "raw_exact": body == original, "fixed_bit_differences": diffs,
            "resolved_switch_offsets": [targets[k] for k in range(0x24, 0x35, 4)],
            "literal_relocations": literals, "switch_base_relocations": switch_base}


if __name__ == "__main__":
    baseline = wave / "r25-intake-repaired/native-selected/BrnLoadingScreenRenderer-e5e683c8.obj"
    candidate = Path(sys.argv[1]) if len(sys.argv)>1 else here / "loading.obj"
    print(json.dumps({"artist_start": hex(start), "artist_switch_offsets": original_targets,
                      "before": compare(baseline), "after": compare(candidate)}, indent=2))
