"""Bind the typed Director replay layout and codec plan to PPC COFF."""
import hashlib
import json
from pathlib import Path
import runpy
import struct
import sys

sys.dont_write_bytecode = True
OUT = Path(__file__).resolve().parent
WAVE = OUT.parent
HELPER = WAVE / "r148-boot-presentation-linked/verify_linked.py"
ROOT = Path("D:/Reverse/BP-Decomp_Workflow")
sys.path.insert(0, str(ROOT / "tools/re"))
import x360rd


def digest(data):
    return hashlib.sha256(data).hexdigest()


def read(path, sha):
    data = Path(path).read_bytes()
    assert digest(data) == sha, path
    return data


def compare(prefix, old, new, signature, diagnostic_offsets=()):
    a, old_body, old_refs = old.function(prefix)
    b, new_body, new_refs = new.function(prefix)
    assert a.name == b.name and old_body == new_body
    old_sig, new_sig = signature(a, old_refs), signature(b, new_refs)
    differences = {at for at in old_sig.keys() | new_sig.keys() if old_sig.get(at) != new_sig.get(at)}
    assert differences == set(diagnostic_offsets)
    for at in differences:
        assert old_refs[at][0] == new_refs[at][0] and old_refs[at][0] in (0x10, 0x11)
        for obj, refs in ((old, old_refs), (new, new_refs)):
            sym = refs[at][1]
            _, data, _, _ = obj.section(sym.section)
            path = data[sym.value:data.index(b"\0", sym.value)].replace(b"\\", b"/")
            assert path.endswith(b"/src/GameShared/GameClasses/SceneManager/CgsSceneManagerIO.h")
    return {
        "symbol": b.name, "bytes": len(new_body), "sha256": digest(new_body),
        "relocations": len(new_refs), "diagnostic_path_relocations": [hex(i) for i in sorted(differences)],
    }


def spans(obj):
    symbol = obj.definition("KA_BRIDGE_SPANS")
    return obj.literal(symbol, 80)


def main():
    helper_digest = "434c49d6c2f58252bd634051f259fd267184fce7d8568e01b55cafc1a53e0fba"
    read(HELPER, helper_digest)
    helper = runpy.run_path(str(HELPER))
    Coff, signature = helper["Coff"], helper["ref_signature"]
    invocation = json.loads((WAVE / "r165-link/invocation.json").read_text())
    baseline = {Path(row["object"]).name: row for row in invocation["objects"]}
    report = json.loads((OUT / "compile-results.json").read_text())
    candidate = {Path(row["object"]).name: row for row in report["results"]}
    for row in candidate.values():
        assert row["returncode"] == 0 and row["machine"] == 0x1F2

    objects = {}
    comparisons = {}
    plans = {
        "BrnReplayDirectorBridgeSerialiser-1ea958bd.obj": [
            ("?Construct@DirectorBridgeSerialiser", ()),
            ("?GetStaticLayout@DirectorBridgeSerialiser", ()),
            ("?SerialiseGameActionQueue@DirectorBridgeSerialiser", ()),
            ("?Read@DirectorBridgeSerialiser", ()),
            ("?Write@DirectorBridgeSerialiser", ()),
        ],
        "BrnGameModule-f766a093.obj": [
            ("?DoUpdate_Director@BrnGameModule", (0x27C, 0x284)),
            ("?DoUpdate_ReplaysPostSim@BrnGameModule", ()),
            ("?BridgeGameStateToDirector@BrnGameModule", ()),
        ],
    }
    for name, functions in plans.items():
        before, after = baseline[name], candidate[name]
        old = Coff(read(before["object"], before["sha256"]))
        new = Coff(read(after["object"], after["object_sha256"]))
        objects[name] = [before["sha256"], after["object_sha256"]]
        for prefix, offsets in functions:
            comparisons[prefix] = compare(prefix, old, new, signature, offsets)
        if name.startswith("BrnReplayDirectorBridgeSerialiser"):
            raw = spans(new)
            assert raw == spans(old)
            pairs = list(struct.iter_unpack(">II", raw))
            assert pairs == [(0, 1), (3, 8), (0x10, 10112), (0x5BA0, 48), (0x5BD0, 512),
                             (0x5DD0, 296), (0x5EF8, 1), (0x5EFC, 4), (2, 1), (1, 1)]

    probe = candidate["layout_probe.obj"]
    typed = Coff(read(probe["object"], probe["object_sha256"]))
    symbol, code, refs = typed.function("?ReadPlayerScoreData@DirectorBridgeSerialiser")
    assert symbol.name == "?ReadPlayerScoreData@DirectorBridgeSerialiser@BrnReplays@@QAAABVCarScoreData@GameStateModuleIO@BrnGameState@@XZ"
    assert len(code) == 0x58
    assert struct.unpack_from(">I", code, 0x40)[0] == 0x387F5DD0
    artist = x360rd.rd(0x823A8850, len(code))
    masks = {6: 0x03FFFFFC, 0x10: 0xFFFF, 0x11: 0xFFFF}
    changes = {}
    for at in range(0, len(code), 4):
        original, = struct.unpack_from(">I", artist, at)
        rebuilt, = struct.unpack_from(">I", code, at)
        mask = masks[refs[at][0]] if at in refs else 0
        if (original ^ rebuilt) & ~mask:
            changes[hex(at)] = [f"{original:08x}", f"{rebuilt:08x}"]
    assert changes == {"0x28": ["38a00136", "38a00070"]}, changes
    output = {
        "objects_r165_r167": objects, "coff_comparisons": comparisons,
        "stream_spans": [{"offset": hex(offset), "bytes": width} for offset, width in pairs],
        "stream_span_sha256": digest(raw), "snapshot_size": "0x5f00",
        "accessor": {
            "symbol": symbol.name, "artist_va": "0x823a8850", "offset": "0x5dd0",
            "coff_sha256": digest(code), "nonrelocated_word_changes": changes,
        },
    }
    file = OUT / "PPC_SCORE_PROOF.json"
    file.write_text(json.dumps(output, indent=2) + "\n", encoding="utf-8")
    print("PASS 8 carried bodies, 10 stream spans, typed accessor and layout", digest(file.read_bytes()))


if __name__ == "__main__":
    main()
