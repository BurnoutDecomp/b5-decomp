"""Rebind both R167 Director bridges and the unbound score copy to official R168."""
import argparse
import hashlib
import json
from pathlib import Path
import runpy
import struct
import sys

sys.dont_write_bytecode = True
OUT = Path(__file__).resolve().parent
WAVE = OUT.parent
OLD = "76113b97467468cb447d283947fd782ee1260ac6"
OLD_PINS = "13f3ca9542abd3fe74700eb749e7fdee993209f10135277130dd372d778b6ee0"
CANDIDATE = "a524878627dd1c9e205b49b8d47e0df9c9f3f179"
TREE = "e7e3d90864f8c55b11d03e9e1a6d733cc829112b"
R167 = WAVE / "r167-gamestate-director-linked/verify_linked.py"
assert hashlib.sha256(R167.read_bytes()).hexdigest() == "cfe76ccfef97700dfae288bd04963956d7f1f674e0198d41bd6ccf760112b02a"
H = runpy.run_path(str(R167))
checked, sha, git, bind, calls, data_at, full_address, literal = (
    H[name] for name in ("checked", "sha", "git", "bind", "calls", "data_at", "full_address", "literal")
)
GAME, IO, REPLAY = (H[key] for key in ("GAME", "IO", "REPLAY"))
SCORE = "src/GameSource/GameState/BrnGameStateSharedIO.cpp"


def pinned_json(path, digest):
    return json.loads(checked(path, digest))


def load_build(round_name, digest, helper, Image):
    root = pinned_json(WAVE / round_name / "ROOT_LINK_PINS.json", digest)
    pins = root["pins"]
    data = {key: checked(pins[key]["path"], pins[key]["sha256"])
            for key in ("completion", "seal", "invocation", "result", "diagnostics", "pe", "map", "xex")}
    completion, seal, inv, result, diagnostics = (
        json.loads(data[key]) for key in ("completion", "seal", "invocation", "result", "diagnostics")
    )
    assert completion["head"] == seal["head"] == root["head"]
    assert completion["status"] == "SEALED_NO_REGRESSIONS"
    assert completion["passed"] == completion["requested"] == 3710 and completion["failed"] == 0
    assert inv["objects"] == seal["selected"] and inv["sealed_sha256"] == pins["seal"]["sha256"]
    assert result["exit_code"] == 0 and result["all_post_checks_pass"]
    assert diagnostics["unique_symbols"] == 0 and not diagnostics["duplicate_messages"]
    assert data["xex"][:4] == b"XEX2"
    for key in ("pe", "map", "xex"):
        [output] = [row for path, row in result["outputs"].items() if Path(path) == Path(pins[key]["path"])]
        assert output["sha256"] == pins[key]["sha256"]
    pe = Image(Path(pins["pe"]["path"]), Path(pins["map"]["path"]))
    assert pe.base == 0x82000000
    entries = helper["map_symbols"](data["map"])
    return root, seal, pe, entries, {row["source"]: row for row in inv["objects"]}


def source_blob(revision, path):
    return git("rev-parse", f"{revision}:{path}").decode().strip()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root-pins-sha256", required=True)
    args = parser.parse_args()
    for key, digest in (
        ("PE_TOOL", "d9162ed22d19a454c85c2ae961605e5a58e5fad9f7b658da704d8734d8dcfd78"),
        ("COFF_TOOL", "434c49d6c2f58252bd634051f259fd267184fce7d8568e01b55cafc1a53e0fba"),
    ):
        checked(H[key], digest)
    helper = runpy.run_path(str(H["COFF_TOOL"]))
    Image = runpy.run_path(str(H["PE_TOOL"]))["Image"]
    old_root, _, old_pe, old_entries, old = load_build("r167-link", OLD_PINS, helper, Image)
    root, seal, pe, entries, new = load_build("r168-link", args.root_pins_sha256, helper, Image)
    assert old_root["head"] == OLD and (root["tree"], root["parent"]) == (TREE, OLD)
    for revision in (root["head"], CANDIDATE):
        assert git("rev-parse", f"{revision}^{{tree}}", f"{revision}^").decode().split() == [TREE, OLD]

    prior = {
        "gamestate": pinned_json(WAVE / "r167-gamestate-director-linked/RECEIPT.json",
                                 "1ba06cc7c59e8469b9fde43458152924c468318559eaff9696c0ee8e8b561525"),
        "world": pinned_json(WAVE / "r167-world-director-linked/RECEIPT.json",
                             "6d58c6fda4bb7969b9bb2b24e82272e4be9a7beca7c637f6f44ae62df2735f86"),
        "score": pinned_json(WAVE / "r167-score-raw-preflight/R167_SCORE_LINKED.json",
                             "7d891d97990edd448c95118facc28f8982b5d7e442101eda3af3dcdc9ec799d6"),
    }
    assert all(item["head"] == OLD and item["root_pins_sha256"] == OLD_PINS for item in prior.values())
    proof = pinned_json(H["BATCH"] / "COMPOSED_PPC_CHECK.json", H["PPC_SHA"])
    headers = (*[path for path in prior["score"]["source_blobs"] if path.endswith(".h")],
               REPLAY.removesuffix(".cpp") + ".h",
               "src/GameSource/Director/DirectorModule/BrnDirectorModuleIO.h",
               "src/GameSource/Game/BrnGameModule.hpp")
    source_paths = {path: source_blob(root["head"], path) for path in headers}
    assert all(source_blob(OLD, path) == blob for path, blob in source_paths.items())

    owners = {}
    records = {}

    def carry(label, source, symbol, old_receipt=None, edges=()):
        if source not in owners:
            blob = source_blob(root["head"], source)
            assert blob == source_blob(OLD, source)
            [sealed] = [row for row in seal["sources"]
                        if Path(row["path"]).as_posix().lower().endswith("/" + source.lower())]
            source_data = checked(sealed["path"], sealed["sha256"])
            assert source_data.replace(b"\r\n", b"\n") == git("show", f"{root['head']}:{source}").replace(b"\r\n", b"\n")
            source_paths[source] = blob
            owners[source] = [helper["Coff"](checked(row["object"], row["sha256"])) for row in (old[source], new[source])]
        before, after = owners[source]
        old_va, old_body, old_symbol, _ = bind(before, before, old_pe, old_entries, old[source], symbol, helper)
        va, linked, new_symbol, refs = bind(after, before, pe, entries, new[source], symbol, helper)
        assert old_symbol.name == new_symbol.name
        if old_receipt is not None:
            assert old_va == int(old_receipt[0], 16) and sha(old_body) == old_receipt[1]
        for at, name in edges:
            calls(old_pe, old_entries, old_va, [(at, name)])
        linked_edges = calls(pe, entries, va, edges)
        records[label] = {
            "source": source, "symbol": new_symbol.name, "old_va": hex(old_va), "new_va": hex(va),
            "old_object_sha256": old[source]["sha256"], "new_object_sha256": new[source]["sha256"],
            "bytes": len(linked), "new_linked_sha256": sha(linked), "calls": linked_edges,
        }
        return old_va, va, old_body, linked, refs

    game = prior["gamestate"]
    game_edges = [(int(row["offset"], 16), row["name"]) for row in proof["bridge"]["calls"]]
    old_game, new_game, _, _, _ = carry("gamestate_bridge", GAME, "?BridgeGameStateToDirector@",
                                      (game["bridge_va"], game["bridge_body_sha256"]), game_edges)
    for hi, lo in ((0xA0, 0xB8), (0xC4, 0xC8), (0x264, 0x26C), (0x2C8, 0x2D0),
                   (0x2D4, 0x2DC), (0x3A0, 0x3A8), (0x3AC, 0x3B0)):
        assert literal(old_pe, full_address(old_pe, old_game, hi, lo)) == literal(pe, full_address(pe, new_game, hi, lo))
    for at, line in zip((0xFC, 0x268, 0x304, 0x3A4), game["assert_lines"], strict=True):
        assert pe.word(new_game + at) == 0x38A00000 | line
    _, new_score, _, _, _ = carry("typed_replay_score_getter", GAME, "?ReadPlayerScoreData@DirectorBridgeSerialiser")
    assert pe.word(new_score + 0x40) == 0x387F5DD0

    world = prior["world"]
    for label, item in world["methods"].items():
        edges = [(int(at, 16), target["symbol"]) for at, target in item["calls"].items()]
        _, va, _, _, _ = carry(label, item["source"], item["symbol"], (item["linked_va"], item["linked_sha256"]), edges)
        for at, value in item["words"].items():
            assert pe.word(va + int(at, 16)) == int(value, 16)
    mode = records["mode_dispatch"]
    old_world, new_world = (int(mode[key], 16) for key in ("old_va", "new_va"))
    for from_va, to_va in world["linked_continuation_branches"].items():
        at = new_world + int(from_va, 16) - old_world
        assert pe.target(at, pe.word(at)) == new_world + int(to_va, 16) - old_world
    for label, old_va, va, hi, lo, destinations in (
        ("gamestate", old_game, new_game, 0x40, 0x44, list(game["modes"].values())),
        ("world", old_world, new_world, 0xB4, 0xB8,
         [item["target_va"] for item in world["linked_mode_table"].values()]),
    ):
        table = full_address(pe, va, hi, lo)
        assert table == va + full_address(old_pe, old_va, hi, lo) - old_va
        assert [pe.word(table + index * 4) for index in range(8)] == [va + int(item, 16) - old_va for item in destinations]
        records[label + "_mode_table"] = hex(table)
    live = int(records["live_publish"]["new_va"], 16)
    for hi, lo, value in ((0xAEC, 0xB00, "34000000"), (0xB10, 0xB14, "b4000000")):
        assert data_at(pe, full_address(pe, live, hi, lo), 4).hex() == value

    score = prior["score"]
    for label, item in score["functions"].items():
        source = IO if label == "unbound_accessor" else SCORE
        assert old[source]["sha256"] == item["selected_object_sha256"]
        _, _, old_body, new_body, refs = carry(label, source, item["symbol"], (item["va"], item["body_sha256"]))
        assert not refs and old_body == new_body and len(new_body) == item["bytes"]

    codec_tables = []
    for label, hi, lo in (("Read", 0xBC, 0xC0), ("Write", 0x4C, 0x50)):
        _, va, _, _, _ = carry("codec_" + label, REPLAY, f"?{label}@DirectorBridgeSerialiser")
        codec_tables.append(full_address(pe, va, hi, lo))
    assert codec_tables[0] == codec_tables[1]
    raw_spans = data_at(pe, codec_tables[0], 80)
    assert sha(raw_spans) == game["codec_table_sha256"]
    assert [{"offset": hex(at), "bytes": width} for at, width in struct.iter_unpack(">II", raw_spans)] == proof["replay"]["codec_spans"]

    result = {
        "head": root["head"], "tree": TREE, "parent": OLD, "candidate": CANDIDATE,
        "root_pins_sha256": args.root_pins_sha256, "r167_root_pins_sha256": OLD_PINS,
        "source_blobs": source_paths, "records": records, "codec_table_va": hex(codec_tables[0]),
        "codec_table_sha256": sha(raw_spans), "raw_score_bytes_copied": score["copied_bytes"],
        "raw_score_untouched_offsets": score["untouched_offsets"], "scope": "Static linked carry only.",
    }
    out = OUT / "RECEIPT.json"
    out.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print("PASS", len(records) - 2, "functions", sha(out.read_bytes()))


if __name__ == "__main__":
    main()

