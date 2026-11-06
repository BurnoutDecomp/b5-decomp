"""Bind R155 replay/readiness carry and the corrected replay restore flag to the R156 strict image."""
import argparse
from functools import lru_cache
import hashlib
import json
from pathlib import Path
import runpy
import struct
import subprocess

WAVE = Path("D:/Reverse/BP-X360-Experimental-Artifacts/wave20")
GIT_ROOT = Path("C:/Users/adri1/.codex/worktrees/bp360/native-sound")
OUT = WAVE / "r156-replay-linked"
OLD_LINK = WAVE / "r155-link"
NEW_LINK = WAVE / "r156-link"
HEAD = "8157850d32d5d15c6187b22969e939f5ca7c201d"
TREE = "f49d1bf3c086e3a3ac5e5aaae120c1acae04b25a"
OLD_HEAD = "1d617fd4fdbacdc73faee7dfffd7a8d43cc6b9b1"
REPLAY_SOURCE = "src/GameSource/Replays/BrnReplayModule.cpp"
COFF_HELPER = WAVE / "r148-boot-presentation-linked/verify_linked.py"
FOCUSED = WAVE / "r154-racecar-prescene-linked/verify_linked.py"
OLD_RECEIPT = WAVE / "r155-replay-linked/RECEIPT.json"
R153_RECEIPT = WAVE / "r153-replay-linked/RECEIPT.json"
PINS = {
    "seal": "seal.json", "invocation": "invocation.json", "result": "result.json",
    "xex": "native-game.exe", "pe": "native-game.pe", "map": "native-game.map",
}
RELOC_MASKS = {2: 0xFFFFFFFF, 6: 0x03FFFFFC, 0x10: 0xFFFF, 0x11: 0xFFFF}


def digest(data):
    return hashlib.sha256(data).hexdigest()


def sha(path):
    return digest(Path(path).read_bytes())


def load(path):
    return json.loads(path.read_text(encoding="utf-8-sig"))


def git(*args):
    return subprocess.check_output(["git", "--no-optional-locks", "-C", str(GIT_ROOT), *args])


def pinned(path, expected):
    assert sha(path) == expected, path
    return path


def word(data, offset):
    return struct.unpack_from(">I", data, offset)[0]


def object_rows(rows):
    by_source = {row["source"]: row for row in rows}
    assert len(by_source) == len(rows)
    return by_source


class Build:
    def __init__(self, path, pins, helpers, focused):
        for key, name in PINS.items():
            pinned(path / name, pins[key])
        self.seal = load(path / PINS["seal"])
        self.invocation = load(path / PINS["invocation"])
        self.result = load(path / PINS["result"])
        assert self.invocation["sealed_sha256"] == pins["seal"]
        assert self.result["exit_code"] == 0 and self.result["all_post_checks_pass"]
        for key in ("xex", "pe", "map"):
            [row] = [row for output, row in self.result["outputs"].items() if Path(output).name == PINS[key]]
            assert row["sha256"] == pins[key], key
        self.sources = object_rows(self.invocation["objects"])
        assert self.invocation["objects"] == self.seal["selected"]
        self.coff_class = helpers["Coff"]
        self.ref_signature = helpers["ref_signature"]
        self.image = focused["Image"](path / PINS["pe"], path / PINS["map"])
        assert self.image.base == 0x82000000
        self.map = helpers["map_symbols"]((path / PINS["map"]).read_bytes())

    @lru_cache(maxsize=None)
    def coff(self, source):
        row = self.sources[source]
        path = Path(row["object"])
        data = path.read_bytes()
        assert digest(data) == row["sha256"], source
        return self.coff_class(data)

    def function(self, source, name):
        return self.coff(source).function(name)

    def bind(self, source, symbol, code, refs):
        owner = Path(self.sources[source]["object"]).name
        [address] = {address for address, module in self.map[symbol.name] if module.lower() == owner.lower()}
        assert self.image.symbols[symbol.name] == address, (symbol.name, owner)
        linked = b"".join(self.image.word(pc).to_bytes(4, "big") for pc in range(address, address + len(code), 4))
        masks = {offset: RELOC_MASKS[kind] for offset, (kind, _) in refs.items()}
        for offset in range(0, len(code), 4):
            assert not ((word(code, offset) ^ word(linked, offset)) & ~masks.get(offset, 0)), (symbol.name, offset)
        return address, linked, masks

    @lru_cache(maxsize=None)
    def source_proof(self, source):
        [record] = [
            row for row in self.seal["sources"]
            if row["path"].replace("\\", "/").lower().endswith("/" + source.lower())
        ]
        data = Path(record["path"]).read_bytes()
        assert digest(data) == record["sha256"].lower()
        assert data.replace(b"\r\n", b"\n") == git("show", f"{HEAD}:{source}").replace(b"\r\n", b"\n")
        blob = git("rev-parse", f"{HEAD}:{source}").decode().strip()
        return blob, record["sha256"].lower()


def carry(source, name, expected, old, new, windows=None):
    assert expected["r155_object_sha256"] == old.sources[source]["sha256"]
    old_symbol, old_code, old_refs = old.function(source, name)
    symbol, code, refs = new.function(source, name)
    assert old_symbol.name == symbol.name == name
    assert old_code == code and digest(code) == expected.get("instruction_sha256", expected.get("body_sha256")), name
    assert old.ref_signature(old_symbol, old_refs) == new.ref_signature(symbol, refs), name
    assert len(refs) == expected["canonical_relocations"], name
    old_address, old_linked, _ = old.bind(source, old_symbol, old_code, old_refs)
    assert old_address == int(expected["linked_va"], 16)
    assert digest(old_linked) == expected["linked_body_sha256"]
    address, linked, masks = new.bind(source, symbol, code, refs)
    blob, source_sha = new.source_proof(source)
    assert blob == expected["git_blob"] and source_sha == expected["source_sha256"], source
    if windows:
        for offset, value in windows["fixed_words"].items():
            offset = int(offset, 16)
            assert offset not in masks and word(linked, offset) == int(value, 16), (name, hex(offset))
        targets = [
            new.image.target(address + offset, word(linked, offset))
            for offset, (kind, _) in sorted(refs.items()) if kind == 6
        ]
        pos = 0
        for prefix in windows["checked_calls"]:
            target = new.image.symbols.get(prefix)
            if target is None:
                target = new.image.address(prefix)
            pos = targets.index(target, pos) + 1
    extra = {}
    if name.startswith("?SetActiveRaceCarInterface@InputBuffer_PreScene@WorldEntityIO"):
        [copy_length] = [
            address + offset for offset in range(0, len(linked), 4)
            if word(linked, offset) >> 26 == 14 and word(linked, offset) & 0xFFFF == 0x28F0
        ]
        extra["copy_length_witness"] = f"{copy_length:#010x}:{new.image.word(copy_length):08x}"
    return {
        **extra, "symbol": name, "source": source, "git_blob": blob, "source_sha256": source_sha,
        "r155_object": old.sources[source]["object"], "r155_object_sha256": old.sources[source]["sha256"],
        "r156_object": new.sources[source]["object"], "r156_object_sha256": new.sources[source]["sha256"],
        "coff_body_sha256": digest(code), "canonical_relocations": len(refs),
        "linked_va": hex(address), "linked_body_sha256": digest(linked),
    }


def verify_restore(old, new):
    blob, source_sha = new.source_proof(REPLAY_SOURCE)
    old_blob = git("rev-parse", f"{OLD_HEAD}:{REPLAY_SOURCE}").decode().strip()
    assert old_blob == "cfb4b0a1b885c86f441957df051a3f86a31504bc"
    assert blob == "1bc8cd80c059f056ffac570b2f3cdd6b4eb8e555"
    proof = {
        "source": REPLAY_SOURCE, "r155_git_blob": old_blob, "r156_git_blob": blob,
        "r156_source_sha256": source_sha, "r155_object": old.sources[REPLAY_SOURCE],
        "r156_object": new.sources[REPLAY_SOURCE], "functions": {},
        "artist_words": {"0x8264b550": "9bab0059", "0x8265d1ec": "89290059"},
    }
    assert proof["r155_object"]["sha256"] != proof["r156_object"]["sha256"]
    for name in ("BeginRestoring", "UpdateRestoring_PostSim"):
        prefix = f"?{name}@ReplayModule@"
        old_symbol, old_code, old_refs = old.function(REPLAY_SOURCE, prefix)
        symbol, code, refs = new.function(REPLAY_SOURCE, prefix)
        assert old_symbol.name == symbol.name and len(old_code) == len(code)
        assert old.ref_signature(old_symbol, old_refs) == new.ref_signature(symbol, refs)
        old_va, old_linked, _ = old.bind(REPLAY_SOURCE, old_symbol, old_code, old_refs)
        va, linked, masks = new.bind(REPLAY_SOURCE, symbol, code, refs)
        changes = [offset for offset in range(0, len(code), 4) if word(old_code, offset) != word(code, offset)]
        if name == "BeginRestoring":
            assert changes == [0x38] and 0x38 not in masks
            assert word(old_linked, 0x38) == 0x9B9F0058
            assert word(linked, 0x38) == 0x9B9F0059
            assert [word(linked, offset) for offset in (0x10, 0x14, 0x18, 0x2C)] == [
                0x3BC3084C, 0x3BA0000B, 0x3B800000, 0x38800007,
            ]
            assert new.image.has_call(va + 0x34, "?SetMode@BaseSerialiser@")
            witness_offsets = (0x10, 0x14, 0x18, 0x2C, 0x34, 0x38)
        else:
            assert not changes and 0x34 not in masks
            assert word(old_linked, 0x34) == word(linked, 0x34) == 0x89290059
            witness_offsets = (0x34,)
        proof["functions"][name] = {
            "symbol": symbol.name, "r155_linked_va": hex(old_va), "r156_linked_va": hex(va),
            "r156_linked_body_sha256": digest(linked), "canonical_relocations": len(refs),
            "changed_coff_words": [{"offset": hex(offset), "r155": f"{word(old_code, offset):08x}",
                                    "r156": f"{word(code, offset):08x}"} for offset in changes],
            "r156_linked_words": {hex(va + offset): f"{word(linked, offset):08x}" for offset in witness_offsets},
        }
    return proof


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--head", required=True, choices=[HEAD])
    for name in PINS:
        parser.add_argument("--" + name, required=True)
    args = parser.parse_args()
    assert git("rev-parse", f"{args.head}^{{tree}}").decode().strip() == TREE
    pins = {name: getattr(args, name) for name in PINS}
    helpers = runpy.run_path(str(pinned(COFF_HELPER, "434c49d6c2f58252bd634051f259fd267184fce7d8568e01b55cafc1a53e0fba")))
    focused = runpy.run_path(str(pinned(FOCUSED, "d9162ed22d19a454c85c2ae961605e5a58e5fad9f7b658da704d8734d8dcfd78")))
    prior = load(pinned(OLD_RECEIPT, "bd571c8d1103a4826c5830b4e0117517b3591cb6b48c4e71bf3e5a805c54cf7f"))
    windows = load(pinned(R153_RECEIPT, "cd6fb09171dd1258f9d5c2ab6c973c62bac0a81fb13126df3009ad7460e6ff9f"))["functions"]
    old = Build(OLD_LINK, prior["pins"], helpers, focused)
    new = Build(NEW_LINK, pins, helpers, focused)
    assert prior["head"] == old.seal["head"] == OLD_HEAD and new.seal["head"] == HEAD

    functions = {}
    for name, previous in prior["functions"].items():
        preceding_windows = None if name.startswith(focused["PRE"]) else windows[name]
        functions[name] = carry(previous["source"], name, previous, old, new, preceding_windows)
    assert len(functions) == 16
    boundaries = {}
    for key, previous in prior["boundary_owners"].items():
        boundaries[key] = carry(previous["source"], previous["symbol"], previous, old, new)
    assert len(boundaries) == 7
    checks = {}
    for key in prior["focused_checks"]:
        pcs = focused[key](new.image)
        assert len(pcs) == len(prior["focused_checks"][key]), key
        checks[key] = [f"{pc:#010x}:{new.image.word(pc):08x}" for pc in pcs]

    receipt = {
        "head": HEAD, "tree": TREE, "pins": pins, "prior_receipt_sha256": sha(OLD_RECEIPT),
        "functions": functions, "boundary_owners": boundaries, "focused_checks": checks,
        "focused_prescene_verifier_sha256": sha(FOCUSED),
        "restored_flag": verify_restore(old, new),
        "copy_witnesses": prior["copy_witnesses"],
        "scope": "Static linked replay and readiness carry plus restored flag correction. No runtime proof.",
    }
    output = OUT / "RECEIPT.json"
    output.write_text(json.dumps(receipt, indent=2) + "\n", encoding="utf-8")
    print("PASS", sha(output), len(functions), "carried functions", len(boundaries), "boundary owners")


if __name__ == "__main__":
    main()

