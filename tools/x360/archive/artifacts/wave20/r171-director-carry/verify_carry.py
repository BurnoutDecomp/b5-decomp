"""Carry the 23 pinned R170 Director replay and score witnesses into R171."""
import argparse
import hashlib
from pathlib import Path
import runpy
import json
import struct
import sys

sys.dont_write_bytecode = True
OUT = Path(__file__).resolve().parent
WAVE = OUT.parent
HEAD = "b7b549c7f423d38c8ddc63ba089c11152e66d8fd"
CANDIDATE = "a8176256cbe97bbf91b1abc59fd7314361e5cbca"
TREE = "48c7575e7cabf2318abe1bc6e2c6f60256f02794"
PARENT = "95836632b166748b4b3ed0a1ed5d57be02122f29"
PARENT_PINS = "52f0813a547de6dc9c89d012df6e8f5e58ba20565e454642892944c7d4c270fd"
PREVIOUS = WAVE / "r170-director-carry"
PREVIOUS_PROOF = "c4ec1ad2d83fd3e996c2d243551eeb1c804258aebf07f6784c845bf2e569115d"
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
    args = parser.parse_args()
    tools = V["H"]
    checked(tools["PE_TOOL"], "d9162ed22d19a454c85c2ae961605e5a58e5fad9f7b658da704d8734d8dcfd78")
    checked(tools["COFF_TOOL"], "434c49d6c2f58252bd634051f259fd267184fce7d8568e01b55cafc1a53e0fba")
    helper = runpy.run_path(str(tools["COFF_TOOL"]))
    Image = runpy.run_path(str(tools["PE_TOOL"]))["Image"]
    old_root, _, old_pe, old_entries, old = load_build("r170-link", PARENT_PINS, helper, Image)
    root, seal, pe, entries, new = load_build("r171-link", args.root_pins_sha256.lower(), helper, Image)
    assert old_root["head"] == PARENT
    assert (root["head"], root["tree"], root["parent"]) == (HEAD, TREE, PARENT)
    for revision in (HEAD, CANDIDATE):
        assert git("rev-parse", f"{revision}^{{tree}}", f"{revision}^").decode().split() == [TREE, PARENT]

    prior = json.loads(checked(PREVIOUS / "RECEIPT.json", PREVIOUS_PROOF))
    assert prior["head"] == PARENT and prior["root_pins_sha256"] == PARENT_PINS
    for source, blob in prior["source_blobs"].items():
        assert source_blob(PARENT, source) == source_blob(HEAD, source) == blob, source
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
            assert source_data.replace(b"\r\n", b"\n") == git("show", f"{HEAD}:{source}").replace(b"\r\n", b"\n")
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
    rebuilt = {}
    for name in ("BrnGameModule.cpp", "GameBridgeWorldToX.cpp"):
        [source] = [source for source in owners if source.endswith("/" + name)]
        assert old[source]["sha256"] != new[source]["sha256"]
        assert "r171-intake" in Path(new[source]["object"]).parts
        rebuilt[source] = new[source]
    retained = {}
    for name in ("BrnMainDirector.cpp", "BrnGameStateModuleIO.cpp"):
        [source] = [source for source in owners if source.endswith("/" + name)]
        assert old[source]["sha256"] == new[source]["sha256"]
        assert Path(old[source]["object"]) == Path(new[source]["object"])
        retained[source] = new[source]

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
        "head": HEAD, "tree": TREE, "parent": PARENT, "candidate": CANDIDATE,
        "root_pins_sha256": args.root_pins_sha256.lower(), "r170_receipt_sha256": PREVIOUS_PROOF,
        "source_blobs": prior["source_blobs"], "records": records, "mode_targets": modes,
        "rebuilt_owners": rebuilt, "retained_owners": retained,
        "codec_table_va": hex(codec[0]), "codec_table_sha256": prior["codec_table_sha256"],
        "raw_score_bytes_copied": prior["raw_score_bytes_copied"],
        "raw_score_untouched_offsets": prior["raw_score_untouched_offsets"],
        "scope": "Static linked carry only.",
    }
    out = OUT / "RECEIPT.json"
    out.write_text(json.dumps(receipt, indent=2) + "\n", encoding="utf-8")
    print("PASS", len(records), "functions", sha(out.read_bytes()))


if __name__ == "__main__":
    main()
