"""Bind the offline junkyard heading divergence to ARTIST and sealed R153 PPC."""
import json
from pathlib import Path
import re
import struct
import sys

here = Path(__file__).resolve().parent
wave = here.parent
helper = wave / "r128-batch/verify_matches.py"
r = {"__file__": str(helper)}
exec(helper.read_text().split("\nif len(sys.argv)==2:", 1)[0], r)
sha = r["sha"]
art = r["original"]
folder = wave / "r153-link"
head = "bcc3e02a41cdb7bba938b91d7ab638c46962540d"
inventory = json.loads((folder / "source-inventory.json").read_text())
result = json.loads((folder / "result.json").read_text())
assert inventory["head"] == head and result["exit_code"] == 0 and result["all_post_checks_pass"]
images = {}
for name in ("native-game.map", "native-game.pe"):
    path = folder / name
    images[name] = sha(path)
    assert result["outputs"][str(path)]["sha256"] == images[name]
address, load = r["linked_reader"](folder)
invocation = json.loads((folder / "invocation.json").read_text())
mapping = (folder / "native-game.map").read_text(errors="replace")

def symbol(marker):
    item, = re.findall(rf"(?m)^\s+\w{{4}}:[0-9a-fA-F]+\s+({re.escape(marker)}\S+)\s+[0-9a-fA-F]{{8}}\s", mapping)
    return item

def branch(read, va):
    word, = struct.unpack(">I", read(va, 4))
    assert word & 0xfc000003 == 0x48000001
    return r["branch_target"](va, word)

def exact(read, va, expected):
    expected = bytes.fromhex(expected)
    assert read(va, len(expected)) == expected
    return expected.hex(" ", 4)

active_original = exact(art, 0x8236ECE0, "817f2858 2f0bffff 7ecbb378 419a0008 897f2860 556b063e 2b0b0000 419a0074")
swap_original = exact(art, 0x8236ED34, "4bfa1665 11a018c3 15bf6db0 39610070 100058c3 1000028c 10006ec6 7d702026 556bcffe 2b0b0000 419a0014 817d0038 81570000 91770000 915d0038")
assert branch(art, 0x8236ED34) == 0x82310398
assert art(0x8236ED04, 4).hex() == "813d0038"
assert art(0x8236ED10, 4).hex() == "39000010"
assert art(0x8236ED2C, 4).hex() == "13e940cf"

selected, = [row for row in invocation["objects"]
             if row["source"].endswith("/BrnCarSelectManager_CarChange.cpp")]
obj = Path(selected["object"])
assert sha(obj) == selected["sha256"]
setup_symbol = symbol("?SetupSpawnLocations@CarSelectManager@")
setup_va = address(setup_symbol)
assert setup_va == 0x82392898
code, rels = r["collect"](obj, setup_symbol)
assert len(code) == 596
# The remaining code moves from the cached-interface assert directly to a
# five-slot assertion loop. The slots were already filled by this point.
assert branch(load, 0x82392A78) == address(symbol("?GetLastActiveRaceCarInterface@GameStateModule@"))
linked_resume = exact(load, 0x82392A7C,
    "2b030000 409a0020 4be12f35 3d608205 38a0045f 386b3f04 7e84a378 4be12f41 4be12f2d "
    "3d608205 7efebb78 3be00005 3bab3ec4 817e0000")
object_data = obj.read_bytes()
targets = {
    r["object_symbol"](object_data, index)[0]
    for _, index, kind in rels if kind == 6
}
assert not any(name.startswith(("?IsPlayerCarActive@", "?GetPlayerDirection@")) for name in targets)
assert "15bf6db0" not in load(setup_va, len(code)).hex()

edges = (
    ("OnProfileLoaded to EnterJunkyardAtStartOfGame", 0x82397428, 0x82393080, 0x82389840, 0x82391760),
    ("SendSetupPlayerCarEvent to EnterJunkyardAtStartOfGame", 0x8239AA38, 0x82393080, 0x82386EB0, 0x82391760),
    ("EnterJunkyardAtStartOfGame to SetupSpawnLocations", 0x823930A8, 0x8236EA98, 0x82391788, 0x82392898),
    ("EnterJunkyard to SetupSpawnLocations", 0x82398630, 0x8236EA98, 0x823916A4, 0x82392898),
)
for label, original_caller, original_target, linked_caller, linked_target in edges:
    assert branch(art, original_caller) == original_target, label
    assert branch(load, linked_caller) == linked_target, label

trace = wave / "native-xenia-smoke/run-r74-artist-no-trace"
staged = json.loads((trace / "staged.json").read_text())
assert staged["image_sha256"] == staged["reference_image_sha256"]
with (trace / "host-output.log").open(encoding="utf-8", errors="replace", newline="\n") as log:
    trace_line, = [index for index, line in enumerate(log, 1)
                   if index == 211613 and "Title_Screen02.bundle" in line]
assert trace_line == 211613
receipt = {
    "state": "verified static divergence",
    "head": head,
    "tree": "610a39be8529a22eeb2d463669c53ede9e46a2fe",
    "linked_image_hashes": images,
    "selected": selected,
    "artist": {
        "setup": "0x8236ea98",
        "active_car_gate_at_0x8236ece0": active_original,
        "slot4_direction_load": "0x8236ed2c",
        "get_player_direction": "0x82310398",
        "comparison_and_swap_at_0x8236ed34": swap_original,
        "operation": "Swap slots 0 and 4 iff there is a valid cached player car and dot3(slot4 direction, player direction) is negative.",
    },
    "linked": {"setup": hex(setup_va), "cached_interface_call": "0x82392a78",
               "resume_to_slot_checks_at_0x82392a7c": linked_resume,
               "has_direction_or_active_provider_call": False},
    "caller_edges": [{"route": label, "artist": [hex(ac), hex(at)], "R153": [hex(lc), hex(lt)]}
                     for label, ac, at, lc, lt in edges],
    "providers": {
        "IsPlayerCarActive": hex(address(symbol("?IsPlayerCarActive@RCEntityActiveRaceCarOutputInterface@"))),
        "GetPlayerDirection": hex(address(symbol("?GetPlayerDirection@RCEntityActiveRaceCarOutputInterface@"))),
    },
    "historical_prioritization": {"reference_image_sha256": staged["reference_image_sha256"],
                                  "log": str(trace / "host-output.log"),
                                  "title_bundle_line": trace_line,
                                  "scope": "Historical ARTIST boot/title evidence only."},
    "repair_scope": "One CarSelectManager TU. Replace the two false stand-ins with the named active-car check and slot 4 direction dot product, preserving assertions and the existing swap.",
    "reachability_limit": "Both offline setup callers are linked. The original swap requires a valid cached post-world active car, so the proof does not establish a first cold-frame or current runtime blocker.",
}
text = json.dumps(receipt, indent=2) + "\n"
if "--write" in sys.argv:
    with (here / "R153_OFFLINE_SPAWN_HEADING.json").open("x", encoding="utf-8") as stream:
        stream.write(text)
print(text, end="")

