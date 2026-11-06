"""Verify the offline junkyard spawn heading in ARTIST and the sealed R155 image."""

import argparse
import json
from pathlib import Path
import re
import struct
import subprocess


HERE = Path(__file__).resolve().parent
WAVE = HERE.parent
REPO = Path("C:/Users/adri1/.codex/worktrees/bp360/wave4-intake")
R154 = "13dfc2ea9153ab2f36c06ad09e2570998e0a2d6d"
R155 = "68208920c4ac83d0759649576cb2d0516cb57bf8"
TREE = "6bdcd7954fa913359b809b7506984f0dc05d90ee"
PRIVATE = "4a05b45a7771065ce635ffcaf0d61b7fc27fd13a"
INTAKE = "1d617fd4fdbacdc73faee7dfffd7a8d43cc6b9b1"
INTAKE_OBJECT_SHA = "7ff6735bbf9b6081bb8deece406fb0ed85d5b9c3c8333bcbd3424241a80e6273"
SOURCE = "src/GameSource/GameState/CarSelect/BrnCarSelectManager_CarChange.cpp"
SETUP = "?SetupSpawnLocations@CarSelectManager@BrnGameState@@AAAXXZ"
CACHED = "?GetLastActiveRaceCarInterface@GameStateModule@"
ACTIVE = "?IsPlayerCarActive@RCEntityActiveRaceCarOutputInterface@"
DIRECTION = "?GetPlayerDirection@RCEntityActiveRaceCarOutputInterface@"
SNAPSHOT_COPY = "??4RCEntityActiveRaceCarOutputInterface@"
CALLERS = (
    ("?OnProfileLoaded@GameStateModule@", "?EnterJunkyardAtStartOfGame@CarSelectManager@", 0x82397428, 0x82393080),
    ("?SendSetupPlayerCarEvent@GameStateModule@", "?EnterJunkyardAtStartOfGame@CarSelectManager@", 0x8239aa38, 0x82393080),
    ("?EnterJunkyardAtStartOfGame@CarSelectManager@", SETUP, 0x823930a8, 0x8236ea98),
    ("?EnterJunkyard@CarSelectManager@", SETUP, 0x82398630, 0x8236ea98),
)
R154_HASHES = {
    "native-game.pe": "8895808adbacbd4c6847d3c4f5322e5ce6421250ba8f02ddbb75942208669c45",
    "native-game.map": "85ea268fb1c72e005910f97c36b9347be5c8fb545539e285c3700d471c393013",
    "native-game.exe": "d477284ed9ff370e673fb67b0f848d9cc1d19125eb21e80cd53e6662c53885d1",
}
ASSERT_PATH_RELOCS = {(0xa0, 0x10), (0xa8, 0x11)}

helper = WAVE / "r128-batch/verify_matches.py"
readers = {"__file__": str(helper)}
exec(helper.read_text().split("\nif len(sys.argv)==2:", 1)[0], readers)
sha = readers["sha"]


def json_file(path):
    return json.loads(path.read_text())


def git(*args):
    return subprocess.check_output(["git", "-C", str(REPO), *args], text=True).strip()


def word(code, offset):
    return struct.unpack_from(">I", code, offset)[0]


def require(code, offset, words):
    expected = bytes.fromhex(words)
    assert code[offset:offset + len(expected)] == expected, hex(offset)


def direct_branch(pc, instruction):
    assert instruction & 0xfc000003 == 0x48000001, hex(pc)
    return readers["branch_target"](pc, instruction)


def coff(path, symbol):
    data = path.read_bytes()
    assert data[:2] == b"\xf2\x01"
    code, relocs = readers["collect"](path, symbol)
    names = {(offset, kind): readers["object_symbol"](data, index)[0]
             for offset, index, kind in relocs}
    return code, relocs, names


def assert_path(obj, relocs):
    data = obj.read_bytes()
    indices = {index for offset, index, kind in relocs if (offset, kind) in ASSERT_PATH_RELOCS}
    index, = indices
    value = readers["symbol_literal"](data, index)
    assert value.replace("\\", "/").endswith("/src/GameShared/GameClasses/System/Resource/CgsResourcePtr.h")
    return value


class Image:
    def __init__(self, folder, head, hashes):
        self.folder = folder.resolve()
        assert json_file(self.folder / "source-inventory.json")["head"] == head
        result = json_file(self.folder / "result.json")
        assert result["exit_code"] == 0 and result["all_post_checks_pass"]
        for name, expected in hashes.items():
            path = self.folder / name
            assert sha(path) == expected == result["outputs"][str(path)]["sha256"], name
        self.mapping = (self.folder / "native-game.map").read_text(errors="replace")
        self.address, self.read = readers["linked_reader"](self.folder)
        invocation = json_file(self.folder / "invocation.json")
        self.objects = {Path(item["object"]).name: item for item in invocation["objects"]}

    def method(self, prefix):
        pattern = rf"(?m)^\s+\w{{4}}:[0-9a-fA-F]+\s+({re.escape(prefix)}\S*)\s+([0-9a-fA-F]{{8}})\s+f\s+(?:i\s+)?(\S+\.obj)\s*$"
        (symbol, va, filename), = re.findall(pattern, self.mapping)
        va = int(va, 16)
        chosen = self.objects[filename]
        obj = Path(chosen["object"])
        assert sha(obj) == chosen["sha256"]
        code, relocs, names = coff(obj, symbol)
        body = self.read(va, len(code))
        readers["fixed_equal"](code, body, relocs)
        return symbol, va, chosen, code, body, names

    def edge(self, caller, target, offset=None):
        callee, destination, *_ = self.method(target)
        _, va, _, _, body, names = self.method(caller)
        if offset is None:
            offset, = [pos for (pos, kind), name in names.items() if kind == 6 and name == callee]
        assert names[offset, 6] == callee
        assert direct_branch(va + offset, word(body, offset)) == destination
        return [hex(va + offset), hex(destination)]


def check_candidate(code, names):
    assert len(code) == 0x2cc
    for offset, target in [(0x1e8, CACHED), (0x218, ACTIVE), (0x23c, DIRECTION)]:
        assert names[offset, 6].startswith(target)
    require(code, 0x21c, "546b063e 2b0b0000 419a0054")
    require(code, 0x228, "817d0038 39400010 7fe4fb78 38610050 13eb50cf")
    require(code, 0x240, "39610050 100058c3 39610050 141f05b0 100059c3")
    assert names[0x254, 0x10] == names[0x25c, 0x11] == "__real@00000000"
    require(code, 0x260, "ff0d0000 40980014 81770000 815d0038 917d0038 91570000")
    # BO=4, BI=24 branches when CR6 LT is clear, including equality and unordered.
    skip = word(code, 0x264)
    assert ((skip >> 21) & 31, (skip >> 16) & 31) == (4, 24)
    assert 0x224 + (word(code, 0x224) & 0xfffc) == 0x264 + (skip & 0xfffc) == 0x278


def check_artist():
    read = readers["original"]
    require(read(0x8236ece0, 32), 0, "817f2858 2f0bffff 7ecbb378 419a0008 897f2860 556b063e 2b0b0000 419a0074")
    for va, instruction in [(0x8236ed04, "813d0038"), (0x8236ed10, "39000010"), (0x8236ed2c, "13e940cf")]:
        require(read(va, 4), 0, instruction)
    require(read(0x8236ed34, 60), 0,
            "4bfa1665 11a018c3 15bf6db0 39610070 100058c3 1000028c 10006ec6 7d702026 556bcffe 2b0b0000 419a0014 817d0038 81570000 91770000 915d0038")
    assert direct_branch(0x8236ed34, word(read(0x8236ed34, 4), 0)) == 0x82310398
    for _, _, pc, destination in CALLERS:
        assert direct_branch(pc, word(read(pc, 4), 0)) == destination


def check_preflight(base):
    assert git("rev-parse", f"{R155}^{{tree}}") == TREE
    assert git("rev-parse", f"{R155}:{SOURCE}") == git("rev-parse", f"{PRIVATE}:{SOURCE}")
    assert git("rev-parse", "codex/experimental-x360-r156-offline-junkyard-exit") == "43ba142ff2345d82c11fcec94dd9ac025271bf1a"

    private_receipt = WAVE / "r154-offline-boot-audit/candidate/R154_OFFLINE_SPAWN_HEADING_COFF.json"
    assert sha(private_receipt) == "18cd616784a0d2af09dbff0c7db79fb17a76bfcd4ae596120b970cba84c60ab9"
    private = json_file(private_receipt)
    private_obj = Path(private["object"])
    assert private["head"] == PRIVATE and sha(private_obj) == private["object_sha256"]

    binding_file = WAVE / "r155-batch/TARGETED_OBJECT_BINDING.json"
    assert sha(binding_file) == "a52fee9e789efa7ae7fe18c7c6e4e83bd227c6f3346fe8ef0632c87972f745db"
    binding = json_file(binding_file)
    assert (binding["head"], binding["tree"]) == (R155, TREE)
    report = binding["compile_report"]
    assert sha(Path(report["path"])) == report["sha256"]
    row, = [item for item in binding["results"] if item["source"] == SOURCE]
    assert row["git_blob"] == git("rev-parse", f"{R155}:{SOURCE}") and row["machine"] == 0x1f2
    artifact = row["artifacts"]["object"]
    obj = Path(artifact["path"])
    assert sha(obj) == artifact["sha256"]
    code, relocs, names = coff(obj, SETUP)
    private_code, private_relocs, private_names = coff(private_obj, SETUP)
    assert private_code == code and private_names.keys() == names.keys()
    # These two relocation records carry the expected worktree-specific assertion path.
    assert {key for key in names if names[key] != private_names[key]} == ASSERT_PATH_RELOCS
    assert assert_path(private_obj, private_relocs) != assert_path(obj, relocs)
    check_candidate(code, names)

    _, va, owner, old_code, old_body, old_names = base.method(SETUP)
    assert va == 0x82392988 and owner["source"] == SOURCE and len(old_code) == 596
    assert not any(name.startswith((ACTIVE, DIRECTION)) for name in old_names.values())
    assert bytes.fromhex("141f05b0") not in old_body and bytes.fromhex("15bf6db0") not in old_body
    base.edge(SETUP, CACHED, 0x1e0)
    for caller, target, _, _ in CALLERS:
        base.edge(caller, target)
    check_artist()

    intake = WAVE / "r155-intake"
    assert json_file(intake / "completion.json")["head"] == INTAKE
    assert git("rev-parse", f"{INTAKE}^{{tree}}") == TREE
    official_obj = intake / "native-selected" / obj.name
    assert sha(official_obj) == INTAKE_OBJECT_SHA
    official_code, official_relocs, official_names = coff(official_obj, SETUP)
    assert official_code == code and official_relocs == relocs
    assert {key for key in names if names[key] != official_names[key]} == ASSERT_PATH_RELOCS
    literal = assert_path(official_obj, official_relocs)
    assert literal.replace("\\", "/").startswith(REPO.as_posix() + "/")
    return obj, official_obj, official_code, official_relocs, official_names, literal


def linked_proof(base, final, composed):
    _, obj, code, relocs, names, assert_literal = composed
    _, va, owner, _, body, final_names = final.method(SETUP)
    assert owner["source"] == SOURCE and owner["sha256"] == sha(obj)
    assert names == final_names
    readers["fixed_equal"](code, body, relocs)
    check_candidate(body, names)
    literal_address = readers["data_target"](body, 0xa0, 0xa8)
    expected_literal = assert_literal.encode() + b"\0"
    assert final.read(literal_address, len(expected_literal)) == expected_literal
    zero_address = readers["data_target"](body, 0x254, 0x25c)
    assert final.read(zero_address, 4) == bytes(4)
    provider_edges = [final.edge(SETUP, target, offset)
                      for offset, target in [(0x1e8, CACHED), (0x218, ACTIVE), (0x23c, DIRECTION)]]

    retained = (CACHED, ACTIVE, DIRECTION, SNAPSHOT_COPY, "?PostWorldUpdate@GameStateModule@")
    for prefix in retained + tuple(caller for caller, _, _, _ in CALLERS):
        old = base.method(prefix)
        new = final.method(prefix)
        assert old[2]["sha256"] == new[2]["sha256"] and old[3] == new[3] and old[5] == new[5], prefix
    snapshot = final.edge("?PostWorldUpdate@GameStateModule@", SNAPSHOT_COPY, 0x58)
    require(final.method(CACHED)[4], 0, "3c630004 386397e0 4e800020")
    require(final.method(SNAPSHOT_COPY)[4], 0x168, "89112860 99162860 89112861 99162861")
    require(final.method(ACTIVE)[4], 0x50, "887f2860")
    require(final.method(DIRECTION)[4], 0xa8, "817f2858 39400540")

    # SpawnInStartCar is in the rebuilt owner TU. Its code and relocations are retained.
    spawn = "?SpawnInStartCar@CarSelectManager@"
    old_spawn = base.method(spawn)
    new_spawn = final.method(spawn)
    assert old_spawn[3] == new_spawn[3] and old_spawn[5] == new_spawn[5]
    callers = [
        {"artist": [hex(pc), hex(dst)], "r154": base.edge(caller, target), "r155": final.edge(caller, target)}
        for caller, target, pc, dst in CALLERS
    ]
    return {
        "linked_setup": hex(va),
        "linked_owner": owner,
        "provider_edges": provider_edges,
        "snapshot_copy_edge": snapshot,
        "caller_edges": callers,
        "unchanged_spawn_in_start_car": {"r154": hex(old_spawn[1]), "r155": hex(new_spawn[1])},
        "ordered_negative_skip_and_slot_stores": [hex(va + offset) for offset in (0x260, 0x264, 0x270, 0x274)],
        "payload_effect": "The only stores in the conditional exchange are slot pointers 0 and 4. Authored spawn position bytes and downstream placement instructions are unchanged.",
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("link", nargs="?", type=Path)
    parser.add_argument("--expected-head")
    parser.add_argument("--expected-seal-sha256")
    parser.add_argument("--expected-pe-sha256")
    parser.add_argument("--expected-map-sha256")
    parser.add_argument("--expected-xex-sha256")
    parser.add_argument("--write", action="store_true")
    args = parser.parse_args()
    base = Image(WAVE / "r154-link", R154, R154_HASHES)
    composed = check_preflight(base)
    proof = {"state": "preflight pass", "base": R154, "composed_head": R155, "tree": TREE,
             "source": SOURCE, "composed_coff_sha256": sha(composed[0]), "official_coff_sha256": sha(composed[1])}
    if args.link:
        hashes = {"native-game.pe": args.expected_pe_sha256, "native-game.map": args.expected_map_sha256,
                  "native-game.exe": args.expected_xex_sha256}
        if not args.expected_head or not args.expected_seal_sha256 or not all(hashes.values()):
            parser.error("a link folder requires expected HEAD, seal, PE, map, and XEX pins")
        assert args.expected_head == INTAKE and git("rev-parse", f"{args.expected_head}^{{tree}}") == TREE
        assert git("rev-parse", f"{args.expected_head}^") == R154
        assert sha(args.link / "seal.json") == args.expected_seal_sha256
        assert json_file(args.link / "invocation.json")["sealed_sha256"] == args.expected_seal_sha256
        final = Image(args.link, args.expected_head, hashes)
        proof.update(state="linked pass", linked_head=args.expected_head,
                     seal_sha256=args.expected_seal_sha256, image_sha256=hashes)
        proof.update(linked_proof(base, final, composed))
    if args.write:
        if not args.link:
            parser.error("--write requires the sealed link folder")
        with (HERE / "R155_OFFLINE_SPAWN_LINKED.json").open("x", encoding="utf-8", newline="\n") as stream:
            json.dump(proof, stream, indent=2)
            stream.write("\n")
    print(json.dumps(proof, indent=2))


if __name__ == "__main__":
    main()
