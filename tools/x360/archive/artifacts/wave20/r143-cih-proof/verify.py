"""Verify the CIH constructor-state field in original ARTIST and XDK PPC COFF."""

import argparse
import hashlib
import json
import struct
from pathlib import Path


ROOT = Path(__file__).resolve().parent
WAVE = ROOT.parent
ARTIST = WAVE / "native-xenia-smoke/run-r74-artist-no-trace/game/native-game.xex"
ARTIST_SHA = "6822f1c1f5d49e0b5df6a34d9fcc619bd331f4a4a5fe3081a0b10b2a03399655"
IN_CTOR = 0x08000000
CIH_OFFSET = 0x0C
GET = "?IsInCtor@AptCIH@@"
SET = "?SetInCtor@AptCIH@@"
ASSOCIATE = "?AssociateInstToClass@AptCIH@@"
CALL_FUNCTION = "?callFunction@AptActionInterpreter@@"


def sha(data):
    return hashlib.sha256(data).hexdigest()


def coff(path, prefix):
    data = path.read_bytes()
    machine, _, _, table, count, optional, _ = struct.unpack_from("<HHIIIHH", data)
    assert machine == 0x1F2 and optional == 0, path
    strings = table + 18 * count
    symbols = {}
    index = 0
    while index < count:
        name, value, section, _, _, aux = struct.unpack_from("<8sIhHBB", data, table + 18 * index)
        if name[:4] == bytes(4):
            start = strings + struct.unpack_from("<I", name, 4)[0]
            name = data[start:data.index(b"\0", start)]
        symbols[index] = name.rstrip(b"\0").decode("ascii", errors="replace"), value, section
        index += 1 + aux
    [(_, value, section)] = [row for row in symbols.values() if row[0].startswith(prefix) and row[2] > 0]
    header = 20 + 40 * (section - 1)
    assert value == 0 and data[header:header + 5] == b".text"
    size, offset, reloc = struct.unpack_from("<III", data, header + 16)
    count = struct.unpack_from("<H", data, header + 32)[0]
    refs = {}
    for index in range(count):
        off, symbol, kind = struct.unpack_from("<IIH", data, reloc + index * 10)
        if kind != 0x12:
            refs[off // 4] = symbols[symbol][0]
    return struct.unpack_from(f">{size // 4}I", data, offset), refs


def constant(value):
    return tuple((value >> bit) & 1 for bit in range(32))


def input_bits(name):
    return tuple(f"{name}[{bit}]" for bit in range(32))


def mask(mb, me):
    # PPC numbers bits from the most significant end, including wraparound.
    return {31 - bit for bit in range(32) if (bit - mb) % 32 <= (me - mb) % 32}


def combine(op, a, b):
    if op == "and":
        return 0 if a == 0 or b == 0 else (b if a == 1 or a == b else (a if b == 1 else (op, a, b)))
    if op == "or":
        return 1 if a == 1 or b == 1 else (b if a == 0 or a == b else (a if b == 0 else (op, a, b)))
    raise AssertionError(op)


FIELD = input_bits("flags")
ARG = input_bits("r4")
REGISTER_INPUTS = tuple(input_bits(f"r{r}") for r in range(32))


def evaluate(code, base):
    """Symbolically track individual bits through a small straight-line PPC slice."""
    regs = list(REGISTER_INPUTS)
    stores = []
    loads = []
    for index, word in enumerate(code):
        opcode, rt, ra, rb = word >> 26, word >> 21 & 31, word >> 16 & 31, word >> 11 & 31
        imm = word & 0xFFFF
        if word == 0x4E800020:  # blr
            assert not any(code[index + 1:]), "unexpected instructions after return"
            break
        if opcode in (32, 36):  # lwz/stw
            assert regs[ra] == REGISTER_INPUTS[base] and imm == CIH_OFFSET, f"unexpected field access {word:08x}"
            if opcode == 32:
                regs[rt] = FIELD
                loads.append(index)
            else:
                stores.append((index, regs[rt]))
        elif opcode in (20, 21):  # rlwimi/rlwinm
            selected = mask(word >> 6 & 31, word >> 1 & 31)
            rotated = tuple(regs[rt][(bit - rb) % 32] for bit in range(32))
            previous = regs[ra] if opcode == 20 else constant(0)
            regs[ra] = tuple(rotated[bit] if bit in selected else previous[bit] for bit in range(32))
        elif opcode in (14, 15) and ra == 0:  # li/lis
            value = imm if imm < 0x8000 else imm - 0x10000
            regs[rt] = constant(value << (16 if opcode == 15 else 0))
        elif opcode == 14:  # unrelated address setup cannot prove a field bit
            regs[rt] = input_bits(f"addi@{index}")
        elif opcode in (24, 25, 28, 29):  # ori/oris/andi./andis.
            value = constant(imm << (16 if opcode in (25, 29) else 0))
            op = "or" if opcode in (24, 25) else "and"
            regs[ra] = tuple(combine(op, a, b) for a, b in zip(regs[rt], value))
        elif opcode == 31 and (word >> 1 & 0x3FF) in (28, 444):  # and/or, including mr
            op = "or" if (word >> 1 & 0x3FF) == 444 else "and"
            regs[ra] = tuple(combine(op, a, b) for a, b in zip(regs[rt], regs[rb]))
        else:
            raise AssertionError(f"unsupported PPC word {word:08x} at +{index * 4:#x}")
    return regs, loads, stores


def expected_field(value):
    return tuple(value if (1 << bit) == IN_CTOR else prior for bit, prior in enumerate(FIELD))


def verify_get(code):
    regs, loads, stores = evaluate(code, 3)
    assert len(loads) == 1 and not stores
    assert regs[3] == (FIELD[27],) + (0,) * 31, "getter does not return CIH+0x0C bit 27"


def verify_set(code):
    _, loads, stores = evaluate(code, 3)
    assert len(loads) == len(stores) == 1 and loads[0] < stores[0][0]
    assert stores[0][1] == expected_field(ARG[0]), "setter must insert only r4 bit 0 and preserve every other flag"


def relative_call(word):
    return word & 0xFC000003 == 0x48000001


def field_pair(code, start, end):
    for i in range(end - 1, start - 1, -1):
        word = code[i]
        if word >> 26 == 36 and word & 0xFFFF == CIH_OFFSET:
            base = word >> 16 & 31
            for j in range(i - 1, start - 1, -1):
                load = code[j]
                if load >> 26 == 32 and load >> 16 & 31 == base and load & 0xFFFF == CIH_OFFSET:
                    return j, i, base
    raise AssertionError("missing CIH+0x0C load/store pair near callFunction")


def has_move(code, target, source):
    return (0x7C000378 | source << 21 | target << 16 | source << 11) in code or target == source


def verify_bracket(code, refs):
    [call] = [i for i, name in refs.items() if name.startswith(CALL_FUNCTION)]
    assert relative_call(code[call])
    setters = sorted(i for i, name in refs.items() if name.startswith(SET))
    if setters:
        before, after = setters
        assert before < call < after and all(relative_call(code[i]) for i in setters)
        assert 0x38800001 in code[max(0, before - 8):before]
        assert 0x38800000 in code[call + 1:after]
        receiver = [r for r in range(14, 32) if has_move(code[max(0, before - 8):before], 3, r)
                    and has_move(code[before + 1:call], 4, r) and has_move(code[call + 1:after], 3, r)]
        assert len(receiver) == 1, "setter calls must bracket the same CIH receiver"
        return {"mode": "setter calls", "set": hex(before * 4), "call": hex(call * 4), "clear": hex(after * 4)}

    before_load, before_store, base = field_pair(code, max(0, call - 12), call)
    after_load, after_store, after_base = field_pair(code, call + 1, min(len(code), call + 9))
    assert base == after_base and base >= 14, "CIH must survive the call in the same nonvolatile register"
    regs, loads, stores = evaluate(code[before_load:before_store + 1], base)
    assert len(loads) == len(stores) == 1 and stores[0][1] == expected_field(1), "wrong bracket set mask"
    assert regs[4] == REGISTER_INPUTS[base] or has_move(code[before_store + 1:call], 4, base)
    _, loads, stores = evaluate(code[after_load:after_store + 1], base)
    assert len(loads) == len(stores) == 1 and stores[0][1] == expected_field(0), "wrong bracket clear mask"
    return {"mode": "inlined", "set": hex(before_store * 4), "call": hex(call * 4), "clear": hex(after_store * 4)}


def verify_artist():
    data = ARTIST.read_bytes()
    assert sha(data) == ARTIST_SHA
    payload = struct.unpack_from(">I", data, 8)[0]
    assert data[payload:payload + 2] == b"MZ"

    def read(address, count):
        return struct.unpack_from(f">{count}I", data, payload + address - 0x82000000)

    verify_get(read(0x82AD50F8, 3))
    verify_set(read(0x82AD5108, 4))
    words = read(0x82B07620, 14)
    call = words[9]
    delta = call & 0x03FFFFFC
    assert 0x82B07644 + (delta if delta < 0x02000000 else delta - 0x04000000) == 0x82AE3C08
    return verify_bracket(words, {9: CALL_FUNCTION})


def negative_controls():
    invocation = json.loads((WAVE / "r142-link" / "invocation.json").read_text())
    paths = {Path(row["source"]).stem: row for row in invocation["objects"]}
    cih, display = [Path(paths[stem]["object"]) for stem in ("AptCIH", "AptDisplayList")]
    assert sha(cih.read_bytes()) == paths["AptCIH"]["sha256"]
    assert sha(display.read_bytes()) == paths["AptDisplayList"]["sha256"]
    get, _ = coff(cih, GET)
    set_, _ = coff(cih, SET)
    bracket, refs = coff(display, ASSOCIATE)
    [call] = [i for i, name in refs.items() if name.startswith(CALL_FUNCTION)]
    # Establish that all three controls really encode the old bit; the old
    # setter also turns any nonzero r4 into all-ones before merging its bit.
    assert get[1] == 0x5563E7FE
    assert set_[:4] == (0x21640000, 0x8143000C, 0x7D6B5910, 0x514B0734)
    assert 0x616B0010 in bracket[call - 12:call] and 0x556B0734 in bracket[call + 1:call + 9]
    rejected = {}
    for label, fn, args in (("get", verify_get, (get,)), ("set", verify_set, (set_,)),
                            ("bracket", verify_bracket, (bracket, refs))):
        try:
            fn(*args)
        except AssertionError as error:
            rejected[label] = str(error)
        else:
            raise AssertionError(f"R142 negative control passed: {label}")
    return rejected


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cih", type=Path)
    parser.add_argument("--display", type=Path)
    parser.add_argument("--find-child", type=Path)
    parser.add_argument("--self-test", action="store_true", help="only verify authentic ARTIST and reject R142")
    args = parser.parse_args()
    original = verify_artist()
    negative = negative_controls()
    if args.self_test:
        print(json.dumps({"artist": original, "rejected_r142": negative}, indent=2))
        return
    if not args.cih or not args.display:
        parser.error("provide both --cih and --display, or use --self-test")

    verify_get(coff(args.cih, GET)[0])
    verify_set(coff(args.cih, SET)[0])
    bracket = verify_bracket(*coff(args.display, ASSOCIATE))
    receipt = {
        "artist_sha256": ARTIST_SHA, "artist_bracket": original,
        "field_offset": hex(CIH_OFFSET), "field_mask": hex(IN_CTOR),
        "setter_input": "r4 bit 0 only", "other_field_bits_preserved": True,
        "objects_sha256": {str(path): sha(path.read_bytes()) for path in (args.cih, args.display)},
        "associate_inst_to_class": bracket, "rejected_r142": negative, "stage": "static COFF",
    }
    if args.find_child:
        code, refs = coff(args.find_child, "?AptResolveSpecialName@@")
        [call] = [i for i, name in refs.items() if name.startswith(GET)]
        assert relative_call(code[call])
        receipt["objects_sha256"][str(args.find_child)] = sha(args.find_child.read_bytes())
        receipt["find_child_getter_call"] = hex(call * 4)
    out = ROOT / "R143_CIH_COFF.json"
    with out.open("x", encoding="utf-8", newline="\n") as stream:
        json.dump(receipt, stream, indent=2)
        stream.write("\n")
    print(out, sha(out.read_bytes()))


if __name__ == "__main__":
    main()
