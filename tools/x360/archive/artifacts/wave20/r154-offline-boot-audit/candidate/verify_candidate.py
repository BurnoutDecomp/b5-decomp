"""Check the private offline spawn-heading repair using targeted XDK COFF."""
import json
from pathlib import Path
import struct
import subprocess
import sys

here = Path(__file__).resolve().parent
audit = here.parent
wave = audit.parent
checkout = Path("C:/Users/adri1/.codex/worktrees/bp360/native_lion")
helper = wave / "r128-batch/verify_matches.py"
r = {"__file__": str(helper)}
exec(helper.read_text().split("\nif len(sys.argv)==2:", 1)[0], r)
sha = r["sha"]
art = r["original"]
baseline_path = audit / "R153_OFFLINE_SPAWN_HEADING.json"
baseline = json.loads(baseline_path.read_text())
assert baseline["state"] == "verified static divergence"

report_path = here / "compile-results.json"
report = json.loads(report_path.read_text())
row, = report["results"]
assert report["state"] == "complete" and report["requested"] == 1
source = "src/GameSource/GameState/CarSelect/BrnCarSelectManager_CarChange.cpp"
assert row["source"] == source and row["passed"] and row["machine"] == 0x1F2
obj = Path(row["object"])
assert obj.parent == here
data = obj.read_bytes()
assert data[:2] == b"\xf2\x01"
symbol = "?SetupSpawnLocations@CarSelectManager@BrnGameState@@AAAXXZ"
code, rels = r["collect"](obj, symbol)
named = {(offset, kind): r["object_symbol"](data, index)[0]
         for offset, index, kind in rels}

def require(blob, offset, words):
    expected = bytes.fromhex(words)
    assert blob[offset:offset+len(expected)] == expected, hex(offset)

def op(blob, offset):
    return struct.unpack_from(">I", blob, offset)[0]

def ppc_branch_target(pc, word):
    return r["branch_target"](pc, word)

assert named[(0x218, 6)].startswith("?IsPlayerCarActive@RCEntityActiveRaceCarOutputInterface@")
assert named[(0x23C, 6)].startswith("?GetPlayerDirection@RCEntityActiveRaceCarOutputInterface@")
require(code, 0x21C, "546b063e 2b0b0000 419a0054")
require(code, 0x228, "817d0038 39400010 7fe4fb78 38610050 13eb50cf")
require(code, 0x240, "39610050 100058c3 39610050 141f05b0 100059c3")
assert named[(0x254, 0x10)] == named[(0x25C, 0x11)] == "__real@00000000"
require(code, 0x260, "ff0d0000 40980014 81770000 815d0038 917d0038 91570000")
# 0x4098... has BO=4 and BI=24. The branch is taken if CR6 LT is clear.
# fcmpu sets CR6 LT only for ordered negative, so +/-0 and unordered skip.
word = op(code, 0x264)
assert (word >> 21) & 31 == 4 and (word >> 16) & 31 == 24
assert 0x264 + (word & 0xfffc) == 0x278
assert 0x224 + (op(code, 0x224) & 0xfffc) == 0x278

folder = wave / "r153-link"
for name, digest in baseline["linked_image_hashes"].items():
    assert sha(folder / name) == digest
_, load = r["linked_reader"](folder)
assert ppc_branch_target(0x823A1480, op(load(0x823A1480, 4), 0)) == 0x826BEAA0
require(load(0x826BEBEC, 0x2C), 0x0C, "91362858")
require(load(0x826BEC08, 0x10), 0, "89112860 99162860 89112861 99162861")
require(load(0x826BF600, 0x14), 0x10, "887f2860")
require(load(0x826BFB60, 8), 0, "817f2858 39400540")
assert ppc_branch_target(0x8236ED34, op(art(0x8236ED34, 4), 0)) == 0x82310398

def git(*args):
    return subprocess.check_output(["git", "-C", str(checkout), *args])

def resolve(rev):
    return git("rev-parse", rev).decode().strip()

assert resolve("codex/experimental-x360-r149-post-title-music") == "669d15949e3aa83c046e15d1553ce4c758b94310"
assert resolve("codex/experimental-x360-preserve-native-lion-7069e3980") == "7069e39809e380607c9bc04ea7e6bc7ca3846239"
if "--write" in sys.argv:
    assert resolve("HEAD^") == baseline["head"] and not git("status", "--porcelain")
    assert git("diff-tree", "--no-commit-id", "--name-only", "-r", "HEAD").decode().splitlines() == [source]
    assert git("show", "HEAD:" + source).replace(b"\r\n", b"\n") == (checkout/source).read_bytes().replace(b"\r\n", b"\n")

receipt = {
    "state": "pass",
    "head": resolve("HEAD"),
    "tree": resolve("HEAD^{tree}"),
    "base": baseline["head"],
    "baseline_static_audit_sha256": sha(baseline_path),
    "source": source,
    "source_sha256": sha(checkout / source),
    "object": str(obj),
    "object_sha256": sha(obj),
    "compile_report_sha256": sha(report_path),
    "coff": {"active_car_call": "0x218", "invalid_skip": "0x224 -> 0x278",
             "slot4_pointer_and_direction": ["0x228", "0x238"],
             "get_player_direction_call": "0x23c", "native_vmx_dot3": "0x24c",
             "ordered_compare": "0x260", "skip_when_CR6_LT_is_clear": "0x264 -> 0x278",
             "pointer_exchange_stores": ["0x270", "0x274"]},
    "retained_snapshot_producer": {
        "post_world_to_snapshot_copy_call": "0x823a1480",
        "native_snapshot_active_flag_copy": "0x826bec08/0x826bec0c",
        "active_player_flag_reader": "0x826bf610",
        "player_direction_row_access": "0x826bfb60/0x826bfb64",
    },
    "scope": "Compile-only proof. ARTIST uses a VMX comparison, while the SDK emits an equivalent ordered scalar comparison after native VMX dot3. The compiler reorders the two independent swap stores.",
}
output = json.dumps(receipt, indent=2) + "\n"
if "--write" in sys.argv:
    with (here / "R154_OFFLINE_SPAWN_HEADING_COFF.json").open("x", encoding="utf-8") as stream:
        stream.write(output)
print(output, end="")
