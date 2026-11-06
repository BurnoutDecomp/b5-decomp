"""Verify the composed R139 PE against the frozen profile-flow object."""
import argparse
import json
from pathlib import Path
import struct
import subprocess


TREE = "95239ff4a27a14e2aafdd02d62a4959d4837df74"
SOURCE = "src/GameSource/GameFlowController/TopLevel/BrnGameMainFlowStates.cpp"
SOURCE_BLOB = "4c66340966ffda79f01660a084085a641c75c810"
PROOF_SHA = "6be7d9c89f3f75a163fbaa72132034a28125b8aa2528c9be1834b9b43c81d67a"
BINDING_SHA = "7a5db010f14dffcb4eda758ef8a879e28b07e5c2ef01ec092efebeaa7ce146e7"
BATCH = Path(__file__).resolve().parent
WAVE = BATCH.parent
FROZEN = WAVE / "r139-profile-flow/no-entry-log"

CARD_ENTER = "?OnEnter@MainGameFlowStateMemoryCard@@UAAXXZ"
CARD_LEAVE = "?OnLeave@MainGameFlowStateMemoryCard@@UAAXXZ"
CARD_UPDATE = "?Update@MainGameFlowStateMemoryCard@@UAAXXZ"
DISK_ENTER = "?OnEnter@MainGameFlowStateCheckDiskSpace@@UAAXXZ"
SAVE = "?SetSaveLoadState@GameMainFlowController@BrnGameMainFlowController@@QAAX_N@Z"
VIDEO = "?SetVideoState@GameMainFlowController@BrnGameMainFlowController@@QAAX_N@Z"
UPDATE = "?Update@LoadingScriptedState@@UAAXXZ"
SEND = "?SendEvent@GameMainFlowController@BrnGameMainFlowController@@QAAXW4EMainGameFlowEvent@2@@Z"


def read(path):
    return json.loads(path.read_text(encoding="utf-8-sig"))


def artifact_row(rows, name, key):
    row, = [item for item in rows if item[key] == name]
    return row


def load_helpers():
    helper = WAVE / "r128-batch/verify_matches.py"
    scope = {"__file__": str(helper)}
    exec(helper.read_text().split("\nif len(sys.argv)==2:", 1)[0], scope)
    return scope


def function(helpers, obj, symbol):
    body, relocs = helpers["collect"](obj, symbol)
    blob = obj.read_bytes()
    references = [(off, kind, helpers["object_symbol"](blob, index)[0])
                  for off, index, kind in relocs if kind != 18]
    return body, relocs, sorted(references)


def verify(link):
    helpers = load_helpers()
    sha = helpers["sha"]
    proof_path = FROZEN / "R139_PROFILE_FLOW_COFF.json"
    binding_path = FROZEN / "CANDIDATE_BINDING.json"
    assert sha(proof_path) == PROOF_SHA and sha(binding_path) == BINDING_SHA
    proof, binding = read(proof_path), read(binding_path)
    assert binding["source"] == SOURCE and binding["source_blob"] == SOURCE_BLOB
    frozen_obj = Path(proof["candidate_coff"])
    assert sha(frozen_obj) == proof["candidate_coff_sha256"] == binding["artifacts"][frozen_obj.name]

    manifest_path = BATCH / "MANIFEST.json"
    manifest = read(manifest_path)
    assert manifest["projected_tree"] == TREE
    assert artifact_row(manifest["final_paths"], SOURCE, "path")["new"].split()[-1] == SOURCE_BLOB
    group = artifact_row(manifest["groups"], "profile-flow", "name")
    assert group["paths"] == [SOURCE]
    pins = {Path(row["path"]).name: row["sha256"] for row in group["receipts"]}
    assert pins[proof_path.name] == PROOF_SHA and pins[binding_path.name] == BINDING_SHA

    seal_path = link / "seal.json"
    invocation_path = link / "invocation.json"
    result_path = link / "result.json"
    seal, invocation, result = read(seal_path), read(invocation_path), read(result_path)
    assert seal["schema"] == "r139-native-link-seal-v1"
    assert invocation["sealed_sha256"] == sha(seal_path)
    assert result["exit_code"] == 0 and result["all_post_checks_pass"]
    head = seal["head"]

    def git(*args):
        return subprocess.check_output(["git", "--no-optional-locks", *args],
                                       cwd=seal["source_root"]).strip().decode()

    assert git("rev-parse", "--verify", f"{head}^{{tree}}") == TREE
    assert git("rev-parse", "--verify", f"{head}:{SOURCE}") == SOURCE_BLOB
    provider = artifact_row(invocation["objects"], SOURCE, "source")
    assert provider == artifact_row(seal["selected"], SOURCE, "source")
    official_obj = Path(provider["object"])
    assert sha(official_obj) == provider["sha256"]
    outputs = {Path(path).name: row for path, row in result["outputs"].items()}
    for name in ("native-game.exe", "native-game.pe", "native-game.map"):
        assert sha(link / name) == outputs[name]["sha256"]
    image = read(link / "image-verification.json")
    assert image["head"] == head and image["magic"] == "XEX2"
    assert image["image_sha256"] == outputs["native-game.exe"]["sha256"]

    raw = helpers["original"]
    checks = {
        0x823AAC78: "9969e28e",
        0x823AAC84: "7d0939ae 812b02d0 7d0931ae 812b02d0 7ca9512e",
        0x823AAC9C: "7d2b20ae 2b090000 4d9a0020 3d200087 61298259 7d2b48ae "
                    "2b090000 4c9a0020 39200005 7d2b512e",
        0x823AACE4: "7d2a41ae 816b02d0 7d2b39ae",
        0x823AAAEC: "616a0644 39600001 997fe28e 3d608301 816b02d0 7d2b512e "
                    "38210060 8181fff8 7d8803a6 ebe1fff0 4e800020",
        0x823F2FA4: "4bfff335",
        0x823F2FB0: "2f0b0008 409a0080",
        0x823F2FC4: "61688258 816a02d0 7d0b40ae 2b080000 419a001c 3d000087 "
                    "61088259 7d0b40ae 2b080000 409a0008 39200001",
        0x823F2FF4: "61080648 7d0b40ae 2b080000",
        0x823F3018: "61290625 38800002 7d0b49ae 816a02d0 3c6b009a 38630664 4bfd3501",
    }
    for va, expected in checks.items():
        data = bytes.fromhex(expected)
        assert raw(va, len(data)) == data, hex(va)

    address, linked = helpers["linked_reader"](link)
    specs = {
        CARD_ENTER: (0x823AAC48, [(0x38, SAVE), (0x44, VIDEO)]),
        CARD_LEAVE: (0x823AACC8, [(0x2C, SAVE), (0x3C, VIDEO)]),
        DISK_ENTER: (0x823AAA98, [(0x2C, "?BeginAssert@Assert@CgsDev@@YAXXZ"),
                                (0x44, "?FireAssert@Assert@CgsDev@@YAXPBD0H@Z"),
                                (0x48, "?EndAssert@Assert@CgsDev@@YAXXZ")]),
        CARD_UPDATE: (0x823F2F98, [(0x0C, UPDATE), (0x9C, SEND)]),
    }
    functions = {}
    for symbol, (artist_va, expected_branches) in specs.items():
        frozen, _, frozen_refs = function(helpers, frozen_obj, symbol)
        official, relocs, refs = function(helpers, official_obj, symbol)
        assert official == frozen and refs == frozen_refs, symbol
        va = address(symbol)
        bound = linked(va, len(official))
        helpers["fixed_equal"](official, bound, relocs)
        assert [(off, name) for off, kind, name in refs if kind == 6] == expected_branches
        highs = {}
        for off, kind, name in refs:
            if kind == 6:
                (word,) = struct.unpack_from(">I", bound, off)
                assert helpers["branch_target"](va + off, word) == address(name), (symbol, name)
            elif kind == 16:
                highs[name] = off
            elif kind == 17:
                assert helpers["data_target"](bound, highs[name], off) == address(name), (symbol, name)
            else:
                raise AssertionError((symbol, hex(off), kind, name))
        functions[symbol] = {"artist_va": hex(artist_va), "linked_va": hex(va), "bytes": len(bound),
                             "branch_vas": {hex(va + off): hex(address(name))
                                            for off, name in expected_branches}}

    assert linked(address(SAVE), 8) == bytes.fromhex("98830054 4e800020")
    assert linked(address(VIDEO), 8) == bytes.fromhex("98830055 4e800020")
    vtable = address("??_7MainGameFlowStateMemoryCard@@6B@")
    assert struct.unpack(">III", linked(vtable, 12)) == tuple(
        address(symbol) for symbol in (CARD_ENTER, CARD_LEAVE, CARD_UPDATE))
    assert struct.unpack(">I", linked(address("??_7MainGameFlowStateCheckDiskSpace@@6B@"), 4))[0] == address(DISK_ENTER)

    return {
        "scope": "composed R139 profile flow in the strict linked PE",
        "head": head,
        "tree": TREE,
        "source_blob": SOURCE_BLOB,
        "xex_sha256": outputs["native-game.exe"]["sha256"],
        "pe_sha256": outputs["native-game.pe"]["sha256"],
        "map_sha256": outputs["native-game.map"]["sha256"],
        "link_result_sha256": sha(result_path),
        "seal_sha256": sha(seal_path),
        "frozen_proof_sha256": PROOF_SHA,
        "frozen_coff_sha256": proof["candidate_coff_sha256"],
        "linked_object_sha256": provider["sha256"],
        "functions": functions,
        "game_offsets": {"flow_controller": "0x9A0664", "save_load": "0x9A06B8",
                         "video": "0x9A06B9", "pending_gui_stage": "0x9A0644",
                         "phase_complete": "0x9A0648", "streaming_wait": "0x9A0625"},
        "invite_offsets_from_gui_module": {"artist": proof["artist_invite_fields_from_gui_module"],
                                           "linked": proof["candidate_invite_fields_from_gui_module"]},
        "memory_card_vtable": hex(vtable),
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("link", type=Path, help="directory with the completed R139 strict link")
    parser.add_argument("--output", type=Path, help="write a new receipt file instead of stdout")
    args = parser.parse_args()
    if not __debug__:
        parser.error("verification requires Python assertions")
    receipt = json.dumps(verify(args.link.resolve()), indent=2) + "\n"
    if args.output is None:
        print(receipt, end="")
    else:
        with args.output.open("x", encoding="utf-8") as output:
            output.write(receipt)


if __name__ == "__main__":
    main()

