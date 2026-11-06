"""Verify the four R157 traffic codec bodies and their direct linked PPC callers."""
from pathlib import Path
import json
import runpy
import struct

OUT = Path(__file__).resolve().parent
WAVE = OUT.parent
LINK = WAVE / "r157-link"
PHASE = runpy.run_path(str(OUT / "verify_phase.py"))
load, sha, bind = (PHASE[key] for key in ("load", "sha", "bind"))
CODEC, S, B = (PHASE[key] for key in ("TRAFFIC_CODEC", "S", "B"))

SPECS = {
    "read": (CODEC, "?Read" + S, {0x10: "?Lock" + B, 0x160: "?ReadFrame" + S,
                                  0x190: "?Unlock" + B}, {}),
    "write": (CODEC, "?Write" + S, {0x10: "?Lock" + B, 0x110: "?WriteFrame" + S,
                                    0x140: "?Unlock" + B}, {}),
    "read_frame": (CODEC, "?ReadFrame" + S, {0x18C: "?ReadAsQuatPos" + S,
                                            0x1FC: "?ReadPhysicsInfo" + S}, {}),
    "write_frame": (CODEC, "?WriteFrame" + S, {0x18C: "?WriteAsQuatPos" + S,
                                              0x1F8: "?WritePhysicsInfo" + S}, {}),
}

PINS = {
    LINK / "image-verification.json": "688d9955e2e4d9e54d8ead8afd80e1fa60ce81eecfe26a5cf446c9731b9a777e",
    WAVE / "r157-intake/completion.json": "138c67b429421794d64b5c7a60afee91f3bac5de3ea853a94ad6a30fcfdf2e60",
    OUT / "RECEIPT.json": "0098daf8c1e425524e5dab4eae50ef33489757d6d268523450f30532934e507d",
}


def main():
    for path, expected in PINS.items():
        assert sha(path) == expected, path
    prior = load(OUT / "RECEIPT.json")
    linked = prior["strict_link"]
    assert sha(LINK / "seal.json") == linked["seal_sha256"]
    for name in ("native-game.map", "native-game.pe", "native-game.exe"):
        assert sha(LINK / name) == linked["outputs"][name]["sha256"]
    seal = load(LINK / "seal.json")
    assert seal["head"] == linked["head"]
    assert load(LINK / "image-verification.json")["image_sha256"] == linked["outputs"]["native-game.exe"]["sha256"]
    completion = load(WAVE / "r157-intake/completion.json")
    assert completion["head"] == linked["head"] and completion["passed"] == completion["requested"] == 3710

    objects = {row["source"]: row for row in seal["selected"]}
    image = PHASE["IMAGE"](LINK / "native-game.pe", LINK / "native-game.map")
    entries = PHASE["COFF"]["map_symbols"]((LINK / "native-game.map").read_bytes())
    root = Path(seal["source_root"])
    sources = {Path(row["path"]).relative_to(root).as_posix().lower(): row for row in seal["sources"]
               if Path(row["path"]).is_relative_to(root)}
    functions = {name: bind(name, spec, objects, (image, entries, sources, linked["head"]))
                 for name, spec in SPECS.items()}
    target_vas = {int(row["linked_va"], 16): name for name, row in functions.items()}
    branches = {name: [] for name in functions}
    [text] = [row for row in image.sections if row[0].rstrip(b"\0") == b".text"]
    _, size, start, _, offset = text
    for index, (word,) in enumerate(struct.iter_unpack(">I", image.data[offset:offset + size])):
        if word >> 26 not in (16, 18):
            continue
        pc = image.base + start + 4 * index
        target = image.target(pc, word)
        if target in target_vas:
            branches[target_vas[target]].append(hex(pc))

    expected = {
        "read": [prior["functions"]["pre_scene"]["call_edges"]["0x90"]["site_va"]],
        "write": [prior["functions"]["post_physics"]["call_edges"]["0x7f8"]["site_va"]],
        "read_frame": [functions["read"]["call_edges"]["0x160"]["site_va"]],
        "write_frame": [functions["write"]["call_edges"]["0x110"]["site_va"]],
    }
    assert branches == expected, (branches, expected)
    receipt = {
        "head": linked["head"], "tree": linked["tree"],
        "pins": {str(path): value for path, value in PINS.items()},
        "functions": functions, "direct_text_callers": branches,
        "text_range": [hex(image.base + start), hex(image.base + start + size)],
        "scope": "All four bodies and their direct PPC branch edges are present. The phase callers retain the ARTIST skip guard. Indirect reachability and runtime traffic-frame emission are unverified.",
    }
    path = OUT / "CODEC_EDGES.json"
    document = json.dumps(receipt, indent=2) + "\n"
    if path.exists():
        assert path.read_text(encoding="utf-8") == document
    else:
        with path.open("x", encoding="utf-8") as stream:
            stream.write(document)
    print("PASS", len(functions), "linked codec bodies", sha(path))


if __name__ == "__main__":
    main()
