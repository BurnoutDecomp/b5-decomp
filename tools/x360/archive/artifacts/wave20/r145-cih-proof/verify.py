"""Verify native AptCIH packed fields in ARTIST and XDK PPC COFF."""

import argparse
import hashlib
import json
import runpy
import struct
import subprocess
from pathlib import Path


ROOT = Path(__file__).resolve().parent
WAVE = ROOT.parent
ARTIST_SHA = "6822f1c1f5d49e0b5df6a34d9fcc619bd331f4a4a5fe3081a0b10b2a03399655"
R143 = "c8ca8a670d2e3c3b112c7503d8b3cf6e559cb220"
CANDIDATE = "f6497958199fbe34daa57f98f199efda3a1af671"
TREE = "88764cd2333de6e26b4fd588dc0734a26c917a65"
MASK32 = 0xFFFFFFFF
STATE, CLASS, CTOR, REMOVAL, DIRTY = 0x60000000, 0x10000000, 0x08000000, 0x04000000, 0x02000000
GEN_SELF, GEN_CHILDREN, ZOMBIE, FRAME = 0x01000000, 0x00800000, 0x007FFF80, 0xFFFC0000
CIH, FRAME_WORD = 0xC, 0x10
previous = runpy.run_path(str(WAVE / "r143-cih-proof/verify.py"))
coff = previous["coff"]


def sha(data):
    return hashlib.sha256(data).hexdigest()


def signed(value, bits=32):
    sign = 1 << (bits - 1)
    return (value ^ sign) - sign


def mask(mb, me):
    if mb > me:
        return (MASK32 >> mb) | (MASK32 << (31 - me) & MASK32)
    return (MASK32 >> mb) & (MASK32 << (31 - me))


def rotate(value, n):
    return ((value << n) | (value >> (32 - n))) & MASK32


def step_leaf(code, fields, value, refs=None):
    """Evaluate PPC register operations on synthetic words. No code is run natively."""
    regs = [0] * 32
    regs[3], regs[4] = 0x100000, value & MASK32
    cr = [False] * 32
    carry = False
    accesses, calls = [], []
    memory = {0x100000 + offset: word for offset, word in fields.items()}
    pc = 0
    for _ in range(len(code) + 10):
        word = code[pc]
        op, rt, ra, rb = word >> 26, word >> 21 & 31, word >> 16 & 31, word >> 11 & 31
        imm = word & 0xFFFF
        xo = word >> 1 & 0x3FF
        update_cr = None
        if word == 0x4E800020:
            break
        if refs and pc in refs and word & 0xFC000002 == 0x48000000:
            assert refs[pc].startswith("?AptUpdateZombieVector@@"), refs[pc]
            calls.append((refs[pc], regs[3]))
            break
        if op in (32, 36):
            address = regs[ra] + signed(imm, 16)
            assert address in memory, f"unexpected access at {address:#x}"
            accesses.append((op, address - 0x100000))
            if op == 32:
                regs[rt] = memory[address]
            else:
                memory[address] = regs[rt]
        elif op in (20, 21):
            selected = mask(word >> 6 & 31, word >> 1 & 31)
            previous = regs[ra] if op == 20 else 0
            regs[ra] = (rotate(regs[rt], rb) & selected) | (previous & ~selected)
            if word & 1:
                update_cr = regs[ra]
        elif op in (14, 15, 12, 13):
            src = regs[ra] if ra or op in (12, 13) else 0
            result = src + (signed(imm, 16) << (16 if op == 15 else 0))
            regs[rt] = result & MASK32
            if op in (12, 13):
                carry = src + (signed(imm, 16) & MASK32) > MASK32
                if op == 13:
                    update_cr = regs[rt]
        elif op in (24, 25, 26, 27, 28, 29):
            rhs = imm << (16 if op in (25, 27, 29) else 0)
            regs[ra] = regs[rt] | rhs if op in (24, 25) else regs[rt] ^ rhs if op in (26, 27) else regs[rt] & rhs
            if op in (28, 29):
                update_cr = regs[ra]
        elif op in (10, 11):
            lhs, rhs = (signed(regs[ra]), signed(imm, 16)) if op == 11 else (regs[ra], imm)
            base = rt & 0x1C
            cr[base:base + 4] = [lhs < rhs, lhs > rhs, lhs == rhs, False]
        elif op == 8:
            rhs = signed(imm, 16) & MASK32
            carry = rhs >= regs[ra]
            regs[rt] = (rhs - regs[ra]) & MASK32
        elif op == 31 and xo in (26, 824, 922, 954, 444, 28, 316, 60, 136, 266, 40, 104):
            a, b = regs[rt], regs[rb]
            if xo in (26, 824, 922, 954, 444, 28, 316, 60):
                out = {26: lambda: 32 - a.bit_length(), 824: lambda: signed(a) >> rb,
                       922: lambda: signed(a & 0xFFFF, 16), 954: lambda: signed(a & 0xFF, 8),
                       444: lambda: a | b, 28: lambda: a & b, 316: lambda: a ^ b,
                       60: lambda: a & ~b}[xo]()
                dest = ra
            else:
                a = regs[ra]
                out = {136: lambda: b + (~a & MASK32) + carry, 266: lambda: a + b,
                       40: lambda: b - a, 104: lambda: -a}[xo]()
                if xo == 136:
                    carry = out > MASK32
                dest = rt
            regs[dest] = out & MASK32
            if word & 1:
                update_cr = regs[dest]
        elif op in (16, 18):
            assert not word & 3, "unsupported branch form"
            if op == 18:
                pc += signed(word & 0x03FFFFFC, 26) // 4
                continue
            assert rt & 4, "unexpected CTR branch"
            if rt & 16 or cr[ra] == bool(rt & 8):
                pc += signed(word & 0xFFFC, 16) // 4
                continue
        elif word >> 26 == 19 and xo == 16:
            assert rt & 4 and not word & 1
            if rt & 16 or cr[ra] == bool(rt & 8):
                break
        else:
            raise AssertionError(f"unsupported PPC word {word:08x} at +{pc * 4:#x}")
        if update_cr is not None:
            cr[:4] = [signed(update_cr) < 0, signed(update_cr) > 0, update_cr == 0, False]
        pc += 1
    else:
        raise AssertionError("leaf did not exit")
    return regs[3], {address - 0x100000: word for address, word in memory.items()}, accesses, calls


LEAVES = {
    "GetCIHState": (0x82AD5128, 3, CIH, STATE, 29, "get"),
    "SetCIHState": (0x82AD5118, 4, CIH, STATE, 29, "set"),
    "GetHasClass": (0x82AD7438, 3, CIH, CLASS, 28, "get"),
    "SetHasClass": (0x82AD7418, 7, CIH, CLASS, 28, "bool"),
    "IsInCtor": (0x82AD50F8, 3, CIH, CTOR, 27, "get"),
    "SetInCtor": (0x82AD5108, 4, CIH, CTOR, 27, "set"),
    "GetInRemList": (0x82AD5188, 3, CIH, REMOVAL, 26, "get"),
    "SetInRemList": (0x82AD5170, 5, CIH, REMOVAL, 26, "set"),
    "GetDirtyState": (0x82AD5C10, 3, CIH, DIRTY, 25, "get"),
    "GetCreatedOnFrame": (0x82AD5198, 3, FRAME_WORD, FRAME, 18, "signed"),
    "SetCreatedOnFrame": (0x82AD51A8, 4, FRAME_WORD, FRAME, 18, "set"),
    "GetZombieCount": (None, None, CIH, ZOMBIE, 7, "signed16"),
    "IncZombieCount": (0x82AD5138, 7, CIH, ZOMBIE, 7, "inc"),
    "DecZombieCount": (None, None, CIH, ZOMBIE, 7, "dec"),
}


def verify_leaf(name, code, refs=None, candidate=False):
    _, _, offset, field, shift, operation = LEAVES[name]
    width = (field >> shift).bit_length()
    vals = {0, MASK32, 0xA5C31E69, 0x5A3CE196}
    vals.update(1 << bit for bit in range(32))
    vals.update(MASK32 ^ (1 << bit) for bit in range(32))
    vals.update((count << shift) | (~field & 0xA5C31E69) for count in (0, 1, 2, (1 << (width - 1)) - 1,
                                                                      1 << (width - 1), (1 << width) - 1))
    args = (0, 1, 2, 3, 0x100, 0x10001, 0x80000000, MASK32, 0xAAAA5555)
    if candidate and name == "SetInRemList":
        args = (0, 1)  # declared bool argument
    tested = 0
    for before in vals:
        current = (before & field) >> shift
        for arg in args if operation in ("set", "bool") else (0,):
            ret, memory, access, calls = step_leaf(code, {offset: before}, arg, refs)
            if operation in ("get", "signed", "signed16"):
                expected = current if operation == "get" else signed(current, width) & MASK32
                if operation == "signed16":
                    # The return is short. XDK may leave the upper register bits unspecified.
                    ret, expected = ret & 0xFFFF, expected & 0xFFFF
                assert ret == expected and memory[offset] == before and (36, offset) not in access
            else:
                replacement = int(bool(arg)) if operation == "bool" else arg
                if operation in ("inc", "dec"):
                    replacement = current + (1 if operation == "inc" else -1)
                expected = (before & ~field) | ((replacement << shift) & field)
                assert memory[offset] == expected and access.count((36, offset)) == 1
                assert bool(calls) == (operation == "dec" and not expected & field)
                assert all(arg == 0 for _, arg in calls)
            assert (32, offset) in access
            tested += 1
    return tested


def check_spans(code, spans):
    for offset, words in spans.items():
        expected = tuple(int(word, 16) for word in words.split())
        assert code[offset // 4:offset // 4 + len(expected)] == expected, hex(offset)
    return [hex(offset) for offset in spans]


def branch_target(code, offset, prefix):
    word = code[offset // 4]
    assert word >> 16 == prefix, (hex(offset), hex(word))
    return offset + signed(word & 0xFFFC, 16)


def verify_consumers(paths, artist, image_base):
    originals = {
        0x82AD76F4: "8163000c 54aa063f 656b0200 9163000c",
        0x82AD7710: "814b000c 392b000c 5548018d 4c820020 654a0200 816b001c 91490000",
        0x82AD7738: "8163000c 556b01ca 9163000c",
        0x82ADCA68: "8169000c 556b020c 9169000c 556a0211",
        0x82ADCA90: "8163000c 556a0211 40820060 556b01cf 40820058",
        0x82ADCAC0: "8149000c 8169001c 654a0100 9149000c",
        0x82ADCAD4: "814b000c 392b000c 55480211 40820018 654a0080 816b001c 91490000",
        0x82B0069C: "3d800b80 817f000c 38800001 618c007f 7fe3fb78 7d6b6038 917f000c",
        0x82B006BC: "817f0010 38800000 656bfffc 917f0010",
        0x82B006D0: "817f000c 7f83e378 93bf0024 556b0146 93bf0018 93bf0014 917f000c",
        0x82ADD2FC: "817f000c 55690044 7f095040 409a0070 574a063f 4082000c 556b0271 40820060",
        0x82ADD330: "815f000c 39200004 38800000 554a00c0 7fe3fb78 915f000c",
    }
    for address, words in originals.items():
        expected = bytes.fromhex(words)
        assert artist[image_base + address:image_base + address + len(expected)] == expected, hex(address)

    cih = paths["AptCIH"]
    tick, _ = coff(cih, "?tick@AptCIH@@")
    tick_sites = check_spans(tick, {0x10: "815c000c 554b018c"})
    dirty, _ = coff(cih, "?SetDirtyState@AptCIH@@")
    dirty_sites = check_spans(dirty, {
        0x40: "8163000c 54aa063e 656b0200 2b0a0000 9163000c",
        0x64: "814b000c 392b000c 5548018c",
        0x78: "654a0200 816b001c 2b0b0000 91490000",
        0x90: "8163000c 556b01ca 9163000c",
    })
    generalized, _ = coff(paths["AptCIHBehaviour"], "?SetGeneralizedProcessDirtyState@AptCIH@@")
    generalized_sites = check_spans(generalized, {
        0x1C: "8148000c 8168001c 654a0100 2b0b0000 9148000c",
        0x34: "814b000c 392b000c 55470210",
        0x48: "654a0080 816b001c 2b0b0000 91490000",
        0x70: "8168000c 556b020c 556a0210 2b0a0000 9168000c",
        0xA4: "8163000c 556b01d0",
    })
    ctor, ctor_refs = coff(cih, "??0AptCIH@@")
    ctor_sites = check_spans(ctor, {
        0x6C: originals[0x82B0069C],
        0x8C: "817f0010 38800000 7fe3fb78 656bfffc 917f0010",
        0xA4: originals[0x82B006D0].replace("7f83e378 93bf0024", "7fa3eb78 939f0024")
        .replace("93bf0018 93bf0014", "939f0018 939f0014"),
    })
    assert ctor_refs[0x88 // 4].startswith("?setGCRoot@AptValue@@")
    assert ctor_refs[0xA0 // 4].startswith("?SetAllowDelayedDeletion@AptValue@@")

    reaper, refs = coff(paths["AptGC"], "?AptUpdateZombieVector@@")
    for offset, symbol in [(0x58, "GetCIHState"), (0x74, "GetZombieCount"), (0xD8, "SetCIHState")]:
        assert refs[offset // 4].startswith(f"?{symbol}@AptCIH@@")
    reaper_sites = check_spans(reaper, {
        0x5C: "2b030001",  # state == 1
        0x64: "7eeb0774 2f0b0000",  # forced reaper bypasses count test
        0x78: "7c6b0734 2f0b0000",  # signed count == 0, including negative nonzero
        0xD0: "38800000",  # clear only the state field through the verified setter
    })
    assert branch_target(reaper, 0x60, 0x409A) == branch_target(reaper, 0x80, 0x409A) == 0x11C
    assert branch_target(reaper, 0x6C, 0x409A) == 0x84
    bracket = previous["verify_bracket"](*coff(paths["AptDisplayList"], previous["ASSOCIATE"]))
    return {"artist_complex_spans": [hex(a) for a in originals], "constructor": ctor_sites,
            "dirty_set_clear_propagate": dirty_sites, "dirty_get_inlined_in_tick": tick_sites,
            "generalized_self_subtree": generalized_sites, "reaper_nonzero_skip": reaper_sites,
            "associate_inst_to_class": bracket}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--objects", type=Path, help="candidate directory with compile-results.json and object files")
    parser.add_argument("--self-test", action="store_true", help="verify ARTIST and pinned R143 only")
    args = parser.parse_args()
    artist = (WAVE / "native-xenia-smoke/run-r74-artist-no-trace/game/native-game.xex").read_bytes()
    assert sha(artist) == ARTIST_SHA
    image_base = struct.unpack_from(">I", artist, 8)[0] - 0x82000000
    assert artist[image_base + 0x82000000:image_base + 0x82000000 + 2] == b"MZ"
    originals = {name: verify_leaf(name, struct.unpack_from(f">{count}I", artist, image_base + address))
                 for name, (address, count, *_) in LEAVES.items() if address}
    link = WAVE / "r143-link"
    assert json.loads((link / "image-verification.json").read_text())["head"] == R143
    [row] = [row for row in json.loads((link / "invocation.json").read_text())["objects"]
             if Path(row["source"]).stem == "AptCIH"]
    old = Path(row["object"])
    assert sha(old.read_bytes()) == row["sha256"]
    negative = {}
    for name in ("GetCIHState", "SetCIHState", "GetHasClass", "GetCreatedOnFrame", "IncZombieCount"):
        code, refs = coff(old, f"?{name}@AptCIH@@")
        try:
            verify_leaf(name, code, refs)
        except AssertionError:
            negative[name] = "rejected"
        else:
            raise AssertionError(f"unexpected R143 pass: {name}")
    receipt = {"artist_sha256": ARTIST_SHA, "artist_cases": originals,
               "negative_head": R143, "negative_cases": negative}
    if args.self_test:
        print(json.dumps(receipt, indent=2))
        return
    if not args.objects:
        parser.error("provide --objects or use --self-test")
    candidate = json.loads((args.objects / "receipt.json").read_text())
    assert (candidate["candidate"], candidate["tree"], candidate["parent"]) == (CANDIDATE, TREE, R143)
    report_path = args.objects / "compile-results.json"
    report = json.loads(report_path.read_text())
    rows = {Path(row["source"]).stem: row for row in report["results"]}
    assert len(rows) == 7 and all(row["passed"] and row["machine"] == 0x1F2 for row in rows.values())
    src_arg = next(arg for arg in next(iter(rows.values()))["command"] if arg.startswith("/I"))
    checkout = Path(src_arg[2:]).parent
    git = lambda *args: subprocess.check_output(["git", "--no-optional-locks", "-C", str(checkout), *args], text=True)
    assert git("rev-parse", "HEAD", "HEAD^{tree}", "HEAD^").splitlines() == [CANDIDATE, TREE, R143]
    assert not git("diff", "HEAD", "--name-only")
    paths = {stem: args.objects / f"{stem}.obj" for stem in rows}
    receipt["candidate"] = CANDIDATE
    receipt["tree"] = TREE
    receipt["compile_report_sha256"] = sha(report_path.read_bytes())
    receipt["candidate_cases"] = {
        name: verify_leaf(name, *coff(paths["AptAnimationTarget" if "RemList" in name else "AptCIH"], f"?{name}@AptCIH@@"),
                          candidate=True) for name in LEAVES if name != "GetDirtyState"
    }
    receipt["consumers"] = verify_consumers(paths, artist, image_base)
    receipt["objects_sha256"] = {stem: sha(path.read_bytes()) for stem, path in paths.items()}
    output = ROOT / "R145_CIH_COFF.json"
    with output.open("x", encoding="utf-8", newline="\n") as stream:
        json.dump(receipt, stream, indent=2)
        stream.write("\n")
    print(output, sha(output.read_bytes()))


if __name__ == "__main__":
    main()
