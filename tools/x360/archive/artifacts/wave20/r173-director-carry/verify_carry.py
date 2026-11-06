"""Carry the 23 pinned R172 Director replay and score witnesses into R173."""
import argparse
import hashlib
from pathlib import Path
import runpy
import json
import sys

sys.dont_write_bytecode = True
OUT = Path(__file__).resolve().parent
WAVE = OUT.parent
CANDIDATE = "40c362823a27c6eb262e851266258648fcedf927"
TREE = "088a9731a493058f90c755dbe4b9e3636abb12b7"
PARENT_TREE = "27d9d594a2eea753b0057137f4898cbc1d3c3900"
PARENT = "b44620b618ed440be3fcb3770002131be370f198"
PARENT_PINS = "cb675722d8d0a4cb857bb47c399d18c301d2734dcb84ea424e87843beb4db68d"
PREVIOUS = WAVE / "r172-director-carry"
PREVIOUS_PROOF = "b9d1bec348d4dba350bee54af83681e810f428fe302ba78b7e7ecfe58c96ba76"
PROJECTION = "1a2d60bdb19b404f88735cc535c0130fd3e38047edba99ff9f885ca60206a559"
PREVIOUS_SCRIPT = WAVE / "r168-director-carry/verify_carry.py"
assert hashlib.sha256(PREVIOUS_SCRIPT.read_bytes()).hexdigest() == "258a2f24a9529bd0e6d2e577839891fa124966131fa46671f9e8d2ecef605cd5"
V = runpy.run_path(str(PREVIOUS_SCRIPT))
checked, sha, git, bind, calls, data_at, full_address, literal, load_build, source_blob = (
    V[name] for name in ("checked", "sha", "git", "bind", "calls", "data_at",
                         "full_address", "literal", "load_build", "source_blob")
)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root-pins-sha256", required=True)
    parser.add_argument("--head", required=True)
    parser.add_argument("--tree", required=True)
    parser.add_argument("--xex-sha256", required=True)
    args = parser.parse_args()
    assert args.tree == TREE
    tools = V["H"]
    checked(tools["PE_TOOL"], "d9162ed22d19a454c85c2ae961605e5a58e5fad9f7b658da704d8734d8dcfd78")
    checked(tools["COFF_TOOL"], "434c49d6c2f58252bd634051f259fd267184fce7d8568e01b55cafc1a53e0fba")
    helper = runpy.run_path(str(tools["COFF_TOOL"]))
    Image = runpy.run_path(str(tools["PE_TOOL"]))["Image"]
    old_root, _, old_pe, old_entries, old = load_build("r172-link", PARENT_PINS, helper, Image)
    root, seal, pe, entries, new = load_build("r173-link", args.root_pins_sha256.lower(), helper, Image)
    assert (old_root["head"], old_root["tree"]) == (PARENT, PARENT_TREE)
    assert (root["head"], root["tree"], root["parent"]) == (args.head, TREE, PARENT)
    assert root["pins"]["xex"]["sha256"] == args.xex_sha256.lower()
    for revision in (args.head, CANDIDATE):
        assert git("rev-parse", f"{revision}^{{tree}}", f"{revision}^").decode().split() == [TREE, PARENT]

    prior = json.loads(checked(PREVIOUS / "RECEIPT.json", PREVIOUS_PROOF))
    assert prior["head"] == PARENT and prior["root_pins_sha256"] == PARENT_PINS
    for source, blob in prior["source_blobs"].items():
        assert source_blob(PARENT, source) == source_blob(args.head, source) == blob, source
    sealed = {Path(row["path"]).as_posix().lower(): row for row in seal["sources"]}
    records = {}
    owners = {}
    for label, previous in prior["records"].items():
        if not isinstance(previous, dict):
            continue
        source, symbol = previous["source"], previous["symbol"]
        if source not in owners:
            [record] = [row for path, row in sealed.items() if path.endswith("/" + source.lower())]
            source_data = checked(record["path"], record["sha256"])
            assert source_data.replace(b"\r\n", b"\n") == git("show", f"{args.head}:{source}").replace(b"\r\n", b"\n")
            owners[source] = [helper["Coff"](checked(row["object"], row["sha256"])) for row in (old[source], new[source])]
        before, after = owners[source]
        assert old[source]["sha256"] == previous["new_object_sha256"]
        old_va, old_body, _, old_refs = bind(before, before, old_pe, old_entries, old[source], symbol, helper)
        va, linked, _, refs = bind(after, before, pe, entries, new[source], symbol, helper)
        assert old_va == int(previous["new_va"], 16) and sha(old_body) == previous["new_linked_sha256"]
        assert len(old_body) == len(linked) == previous["bytes"]
        edges = [(int(at, 16), old_refs[int(at, 16)][1].name) for at in previous["calls"]]
        assert calls(old_pe, old_entries, old_va, edges) == previous["calls"]
        linked_edges = calls(pe, entries, va, edges)
        if label in ("raw_copy", "typed_assignment", "unbound_accessor"):
            assert not refs and old_body == linked
        records[label] = {
            "source": source, "symbol": symbol, "old_va": hex(old_va), "new_va": hex(va),
            "old_object_sha256": old[source]["sha256"], "new_object_sha256": new[source]["sha256"],
            "bytes": len(linked), "new_linked_sha256": sha(linked), "calls": linked_edges,
        }
    assert len(records) == 23
    projection = json.loads(checked(WAVE / "r173-intake/projection.json", PROJECTION))
    assert projection["base"] == PARENT
    projected = set(projection["selected_sources"]) & owners.keys()
    assert projected == {"src/GameSource/Game/BrnGameModule.cpp", "src/GameSource/Game/GameBridgeWorldToX.cpp"}
    rebuilt, retained = {}, {}
    for source in owners:
        old_row, new_row = old[source], new[source]
        if source in projected:
            assert old_row["sha256"] != new_row["sha256"], source
            assert "r173-intake" in Path(new_row["object"]).parts, source
            rebuilt[source] = new_row
        else:
            assert old_row == new_row, source
            retained[source] = new_row
    assert len(rebuilt) == 2 and len(retained) == 6

    def entries_for(label):
        return tuple(int(records[label][key], 16) for key in ("old_va", "new_va"))

    modes = {}
    for label, record, hi, lo in (("gamestate", "gamestate_bridge", 0x40, 0x44),
                                  ("world", "mode_dispatch", 0xB4, 0xB8)):
        before, after = entries_for(record)
        old_table = full_address(old_pe, before, hi, lo)
        table = full_address(pe, after, hi, lo)
        assert [hex(old_pe.word(old_table + index * 4)) for index in range(8)] == prior["mode_targets"][label]
        assert table - after == old_table - before
        targets = [pe.word(table + index * 4) for index in range(8)]
        assert targets == [old_pe.word(old_table + index * 4) - before + after for index in range(8)]
        modes[label] = [hex(value) for value in targets]

    before, after = entries_for("gamestate_bridge")
    for hi, lo in ((0xA0, 0xB8), (0xC4, 0xC8), (0x264, 0x26C), (0x2C8, 0x2D0),
                   (0x2D4, 0x2DC), (0x3A0, 0x3A8), (0x3AC, 0x3B0)):
        assert literal(old_pe, full_address(old_pe, before, hi, lo)) == literal(pe, full_address(pe, after, hi, lo))
    for at, line in ((0xFC, 228), (0x268, 256), (0x304, 39), (0x3A4, 285)):
        assert pe.word(after + at) == 0x38A00000 | line

    before, after = entries_for("live_publish")
    for hi, lo, value in ((0xAEC, 0xB00, "34000000"), (0xB10, 0xB14, "b4000000")):
        assert data_at(pe, full_address(pe, after, hi, lo), 4).hex() == value
        assert data_at(old_pe, full_address(old_pe, before, hi, lo), 4).hex() == value
    codec = []
    for label, hi, lo in (("codec_Read", 0xBC, 0xC0), ("codec_Write", 0x4C, 0x50)):
        before, after = entries_for(label)
        assert full_address(old_pe, before, hi, lo) == int(prior["codec_table_va"], 16)
        codec.append(full_address(pe, after, hi, lo))
    assert codec[0] == codec[1]
    assert sha(data_at(pe, codec[0], 80)) == prior["codec_table_sha256"]

    receipt = {
        "head": args.head, "tree": TREE, "parent": PARENT, "candidate": CANDIDATE,
        "xex_sha256": args.xex_sha256.lower(), "projection_sha256": PROJECTION,
        "root_pins_sha256": args.root_pins_sha256.lower(), "r172_receipt_sha256": PREVIOUS_PROOF,
        "source_blobs": prior["source_blobs"], "records": records, "mode_targets": modes,
        "rebuilt_owners": rebuilt, "retained_owners": retained,
        "codec_table_va": hex(codec[0]), "codec_table_sha256": prior["codec_table_sha256"],
        "raw_score_bytes_copied": prior["raw_score_bytes_copied"],
        "raw_score_untouched_offsets": prior["raw_score_untouched_offsets"],
        "scope": "Static linked carry only.",
    }
    out = OUT / "RECEIPT.json"
    text = json.dumps(receipt, indent=2) + "\n"
    if out.exists():
        assert out.read_text(encoding="utf-8") == text
    else:
        with out.open("x", encoding="utf-8") as stream:
            stream.write(text)
    print("PASS", len(records), "functions", sha(out.read_bytes()))


if __name__ == "__main__":
    main()
