"""Bind the reviewed GameState-to-Director path to the official R167 PPC image."""
import argparse
import hashlib
import json
from pathlib import Path
import runpy
import struct
import subprocess

OUT = Path(__file__).resolve().parent
WAVE = OUT.parent
GIT = Path("C:/Users/adri1/.codex/worktrees/bp360/wave4-intake")
CANDIDATE = "aa0986c0b7b33cc978b2945dd8fbc4577f181cf7"
TREE = "54cb3dd0e5592344bdc40ea616b6c5800ba6c674"
PARENT = "135948fd62910b9dc8911e865cf476555d05460e"
GAME = "src/GameSource/Game/BrnGameModule.cpp"
IO = "src/GameSource/GameState/BrnGameStateModuleIO.cpp"
REPLAY = "src/GameSource/Replays/Serialisers/BrnReplayDirectorBridgeSerialiser.cpp"
BATCH = WAVE / "r167-batch-v3"
PPC_SHA = "d29386addaf3ed54af942d467d8aed42ea9a62f8c316124baf9530309cbe5d12"
PE_TOOL = WAVE / "r154-racecar-prescene-linked/verify_linked.py"
COFF_TOOL = WAVE / "r148-boot-presentation-linked/verify_linked.py"
RELOC_MASKS = {2: 0xFFFFFFFF, 6: 0x03FFFFFC, 0x10: 0xFFFF, 0x11: 0xFFFF}


def sha(data):
    return hashlib.sha256(data).hexdigest()


def checked(path, digest):
    data = Path(path).read_bytes()
    assert sha(data) == digest, path
    return data


def git(*args):
    return subprocess.check_output(["git", "--no-optional-locks", "-C", str(GIT), *args])


def word(data, at):
    return struct.unpack_from(">I", data, at)[0]


def signed16(value):
    low = value & 0xFFFF
    return low - 0x10000 if low & 0x8000 else low


def data_at(pe, address, size):
    rva = address - pe.base
    [result] = [pe.data[raw + rva - start:raw + rva - start + size]
                for _, _, start, length, raw in pe.sections
                if start <= rva and rva + size <= start + length]
    return result


def full_address(pe, entry, high, low):
    hi, lo = pe.word(entry + high), pe.word(entry + low)
    assert hi >> 26 == 15 and hi >> 16 & 31 == 0
    assert lo >> 26 in (14, 48)
    assert hi >> 21 & 31 == lo >> 16 & 31
    return ((hi & 0xFFFF) * 0x10000 + signed16(lo)) & 0xFFFFFFFF


def literal(pe, address):
    return data_at(pe, address, 140).split(b"\0", 1)[0].decode("ascii")


def bind(coff, frozen, pe, entries, selected, prefix, helper):
    a, preflight, old_refs = frozen.function(prefix)
    b, actual, refs = coff.function(prefix)
    assert a.name == b.name and preflight == actual, prefix
    old_sig, new_sig = helper["ref_signature"](a, old_refs), helper["ref_signature"](b, refs)
    for at in {at for at in old_sig.keys() | new_sig.keys() if old_sig.get(at) != new_sig.get(at)}:
        assert old_refs[at][0] == refs[at][0] and refs[at][0] in (0x10, 0x11), (prefix, hex(at))
        tails = []
        for obj, table in ((frozen, old_refs), (coff, refs)):
            target = table[at][1]
            _, data, _, _ = obj.section(target.section)
            path = data[target.value:data.index(b"\0", target.value)].replace(b"\\", b"/")
            assert b"/src/" in path and path.endswith((b".h", b".cpp")), (prefix, path)
            tails.append(path.split(b"/src/", 1)[1])
        assert tails[0] == tails[1]
    owner = Path(selected["object"]).name.lower()
    [va] = {va for va, obj in entries[b.name] if obj.lower() == owner}
    assert pe.symbols[b.name] == va
    linked = data_at(pe, va, len(actual))
    for at in range(0, len(actual), 4):
        mask = RELOC_MASKS[refs[at][0]] if at in refs else 0
        assert not ((word(actual, at) ^ word(linked, at)) & ~mask), (prefix, hex(at))
    return va, linked, b, refs


def calls(pe, entries, entry, checks):
    result = {}
    for at, name in checks:
        instruction = pe.word(entry + at)
        target = pe.target(entry + at, instruction)
        assert instruction >> 26 == 18 and target in {va for va, _ in entries[name]}, (name, hex(at), target)
        result[hex(at)] = hex(target)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root-pins-sha256", required=True)
    args = parser.parse_args()
    root = json.loads(checked(WAVE / "r167-link/ROOT_LINK_PINS.json", args.root_pins_sha256))
    head = root["head"]
    assert (root["tree"], root["parent"]) == (TREE, PARENT)
    for revision in (head, CANDIDATE):
        assert git("rev-parse", f"{revision}^{{tree}}", f"{revision}^").decode().split() == [TREE, PARENT]
    pins = root["pins"]
    raw = {k: checked(pins[k]["path"], pins[k]["sha256"])
           for k in ("completion", "seal", "invocation", "result", "diagnostics", "pe", "map", "xex")}
    completion, seal, invocation, result, diag = (
        json.loads(raw[k]) for k in ("completion", "seal", "invocation", "result", "diagnostics"))
    assert completion["head"] == seal["head"] == head
    assert completion["status"] == "SEALED_NO_REGRESSIONS" and completion["passed"] == completion["requested"] == 3710
    assert completion["failed"] == 0 and invocation["objects"] == seal["selected"]
    assert invocation["sealed_sha256"] == pins["seal"]["sha256"] and result["exit_code"] == 0
    assert result["all_post_checks_pass"] and diag["unique_symbols"] == 0 and not diag["duplicate_messages"]
    assert raw["xex"][:4] == b"XEX2"
    for key in ("pe", "map", "xex"):
        [output] = [row for path, row in result["outputs"].items() if Path(path) == Path(pins[key]["path"])]
        assert output["sha256"] == pins[key]["sha256"]
    checked(PE_TOOL, "d9162ed22d19a454c85c2ae961605e5a58e5fad9f7b658da704d8734d8dcfd78")
    checked(COFF_TOOL, "434c49d6c2f58252bd634051f259fd267184fce7d8568e01b55cafc1a53e0fba")
    pe = runpy.run_path(str(PE_TOOL))["Image"](Path(pins["pe"]["path"]), Path(pins["map"]["path"]))
    helper = runpy.run_path(str(COFF_TOOL))
    entries = helper["map_symbols"](raw["map"])
    proof = json.loads(checked(BATCH / "COMPOSED_PPC_CHECK.json", PPC_SHA))
    targeted = json.loads(checked(BATCH / "TARGETED_OBJECT_BINDING.json", proof["targeted_binding_sha256"]))
    assert proof["head"] == targeted["head"] == CANDIDATE and proof["tree"] == targeted["tree"] == TREE
    selected = {row["source"]: row for row in invocation["objects"]}
    preflight = {row["source"]: row for row in targeted["results"]}

    objects = {}
    for source in (GAME, IO, REPLAY):
        before, after = preflight[source], selected[source]
        assert git("rev-parse", f"{head}:{source}").decode().strip() == before["git_blob"]
        [sealed] = [row for row in seal["sources"] if Path(row["path"]).as_posix().lower().endswith("/" + source.lower())]
        source_bytes = checked(sealed["path"], sealed["sha256"])
        assert source_bytes.replace(b"\r\n", b"\n") == git("show", f"{head}:{source}").replace(b"\r\n", b"\n")
        artifact = before["artifacts"]["object"]
        objects[source] = (helper["Coff"](checked(after["object"], after["sha256"])),
                           helper["Coff"](checked(artifact["path"], artifact["sha256"])))

    def function(source, prefix):
        return bind(*objects[source], pe, entries, selected[source], prefix, helper)

    bridge, body, symbol, refs = function(GAME, "?BridgeGameStateToDirector@")
    assert len(body) == proof["bridge"]["bytes"]
    table = full_address(pe, bridge, 0x40, 0x44)
    assert table == bridge + 0x58
    expected_targets = [0x78, 0x78, 0x78, 0x78, 0x330, 0x35C, 0x3F0, 0x330]
    mode_table = {}
    for row, expected in zip(proof["bridge"]["mode_table"], expected_targets, strict=True):
        at = int(row["offset"], 16)
        mode = row["mode"]
        kind, target = refs[at]
        assert table + 4 * mode == bridge + at
        assert kind == 2 and target.section == symbol.section and target.name == row["label"]
        assert target.value == expected and pe.word(bridge + at) == bridge + expected
        mode_table[mode] = hex(bridge + expected)
    witnesses = {
        0x14: 0x3E1F009A, 0x1C: 0x3A101070, 0x34: 0x81700000, 0x38: 0x2B0B0007,
        0x48: 0x5560103A, 0x4C: 0x7C0C002E, 0x50: 0x7C0903A6, 0x54: 0x4E800420,
        0x88: 0x83170A34, 0x164: 0x3BF32790, 0x1D8: 0x38743110, 0x1EC: 0x1D780128,
        0x1F8: 0x89770A88, 0x200: 0x99743258, 0x214: 0x38635DD0, 0x248: 0x9BE35EF8,
        0x284: 0xE97F09F8, 0x288: 0x556B063E, 0x298: 0x99747AD5, 0x338: 0x616B10C8,
        0x340: 0x614A10C9, 0x348: 0x7E3F59AE, 0x34C: 0x7E3F51AE, 0x3CC: 0x3BF32790,
        0x3E8: 0x38743110,
    }
    for at, expected in witnesses.items():
        assert pe.word(bridge + at) == expected, hex(at)
    branches = {0x3C: 0x3F0, 0x160: 0x184, 0x1BC: 0x1D0, 0x1DC: 0x1EC, 0x20C: 0x234,
                0x218: 0x228, 0x280: 0x290, 0x360: 0x3F0}
    for at, expected in branches.items():
        assert pe.target(bridge + at, pe.word(bridge + at)) == bridge + expected
    bridge_calls = calls(pe, entries, bridge, ((int(row["offset"], 16), row["name"]) for row in proof["bridge"]["calls"]))
    assert len(bridge_calls) == len(proof["bridge"]["calls"])

    source_path = r"d:\p4\b5_main\burnout\main\code\gamesource\unity\../Game/GameBridgeGameStateToX.cpp"
    killer = "lePlayerKillerRaceCarIndex != E_ACTIVE_RACE_CAR_INDEX_INVALID"
    asserts = [
        (0xFC, 228, (0xC4, 0xC8), killer, (0xA0, 0xB8), source_path),
        (0x268, 256, (0x264, 0x26C), "lpGameStateOutput->GetScoringOutputInterface()", (0xA0, 0xB8), source_path),
        (0x304, 39, (0x2C8, 0x2D0), "leEnumIndex <= E_ACTIVE_RACE_CAR_INDEX_COUNT",
         (0x2D4, 0x2DC), r"..\..\..\GameSource\BurnoutConstants.h"),
        (0x3A4, 285, (0x3AC, 0x3B0), killer, (0x3A0, 0x3A8), source_path),
    ]
    for at, line, msg, text, path, filename in asserts:
        assert pe.word(bridge + at) == 0x38A00000 | line
        assert literal(pe, full_address(pe, bridge, *msg)) == text
        assert literal(pe, full_address(pe, bridge, *path)) == filename
    assert [a[1] for a in asserts] == proof["bridge"]["artist_assert_lines"]

    getter, getter_body, _, _ = function(IO, "?GetUnboundPlayerScoreBytes@")
    assert getter_body == bytes.fromhex("3c6300033863a3904e800020")
    score, score_body, _, _ = function(GAME, "?ReadPlayerScoreData@DirectorBridgeSerialiser")
    assert word(score_body, 0x40) == 0x387F5DD0
    caller, _, _, _ = function(GAME, "?DoUpdate_Director@")
    frame_calls = calls(pe, entries, caller, ((int(at, 16), name) for at, name in
                        proof["world_bridge"]["methods"]["frame_caller"]["edges"].items()))
    codec = []
    for name, hi, lo in (("Read", 0xBC, 0xC0), ("Write", 0x4C, 0x50)):
        va, _, _, _ = function(REPLAY, f"?{name}@DirectorBridgeSerialiser")
        codec.append(full_address(pe, va, hi, lo))
    assert codec[0] == codec[1]
    spans = data_at(pe, codec[0], 80)
    assert sha(spans) == proof["replay"]["codec_table_sha256"]
    assert [{"offset": hex(at), "bytes": width} for at, width in struct.iter_unpack(">II", spans)] == proof["replay"]["codec_spans"]
    receipt = {
        "head": head, "tree": TREE, "parent": PARENT, "candidate": CANDIDATE,
        "root_pins_sha256": args.root_pins_sha256, "composed_ppc_sha256": PPC_SHA,
        "selected": {source: selected[source] for source in objects},
        "bridge_va": hex(bridge), "bridge_body_sha256": sha(body), "modes": mode_table,
        "direct_edges": bridge_calls, "frame_edges": frame_calls,
        "assert_lines": [a[1] for a in asserts], "unbound_record_getter": hex(getter),
        "typed_score_getter": hex(score), "raw_unbound_elimination": ["0x9f8", "low octet", "0x7ad5"],
        "codec_table_va": hex(codec[0]), "codec_table_sha256": sha(spans),
        "scope": "Static linked bridge and replay codec. Raw copy body and runtime are separate.",
    }
    out = OUT / "RECEIPT.json"
    out.write_text(json.dumps(receipt, indent=2) + "\n", encoding="utf-8")
    print("PASS", hex(bridge), sha(out.read_bytes()))


if __name__ == "__main__":
    main()
