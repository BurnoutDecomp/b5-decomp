"""Check the post-title sign-in gate in ARTIST, the R137 PE, and R140 COFF."""
import json
from pathlib import Path
import struct

here = Path(__file__).resolve().parent
wave = here.parent
helper = wave / "r128-batch/verify_matches.py"
ns = {"__file__": str(helper)}
exec(helper.read_text().split("\nif len(sys.argv)==2:", 1)[0], ns)

base = wave / "r137-link"
address, linked = ns["linked_reader"](base)
original = ns["original"]
report = json.loads((here / "compile-results.json").read_text())
items = {Path(item["source"]).stem: item for item in report["results"]}
assert report["passed"] == 3 and report["failed"] == 0
assert all(item["machine"] == 0x1F2 for item in items.values())
construct = "?Construct@GuiCache@BrnGui@@QAAXPAVGuiTracker@2@PAVSystemUserProfile@CgsGui@@@Z"
update = "?Update@PostTitleScreenLoad@BrnGui@@UAAXXZ"
signin = "?SigninStateChanged@ProfileManager@BrnGui@@EAAXW4EUserSigninState@SystemUserProfile@CgsGui@@@Z"
bind_user = "?SetSignedInUserIndex@SaveLoadSystem@CgsGui@@QAAXH@Z"
queue = "?AddEvent@?$VariableEventQueue@$0BAAA@$0BA@@CgsModule@@QAA_NPBUEvent@2@HH@Z"


def words(body, offset, expected):
    data = bytes.fromhex(expected)
    assert body[offset:offset + len(data)] == data, (hex(offset), expected)


def candidate(stem, symbol):
    obj = Path(items[stem]["object"])
    body, relocs = ns["collect"](obj, symbol)
    binary = obj.read_bytes()
    branches = [(offset, ns["object_symbol"](binary, index)[0])
                for offset, index, kind in relocs if kind == 6]
    return body, branches


def low_half_offsets(body, low):
    return [i for i, (word,) in enumerate(struct.iter_unpack(">I", body))
            if word & 0xFFFF == low]


# Construct clears the named flag, with the independent intro gate still at +0x4B78.
words(original(0x82505AC0, 4), 0, "9bdf4b55")
words(original(0x82505AF0, 4), 0, "997f4b78")
r137_ctor = linked(address(construct), 0x218)
assert not low_half_offsets(r137_ctor, 0x4B55)
words(r137_ctor, 0x14C, "9bbf4b78")
ctor, _ = candidate("BrnGuiCache", construct)
words(ctor, 0x88, "3bc00000")
words(ctor, 0x11C, "9bdf4b55")
words(ctor, 0x150, "9bbf4b78")
assert low_half_offsets(ctor, 0x4B55) == [0x11C // 4]

# The original writer tests the cache pointer, sets the flag, and then reads the intro gate.
words(original(0x8247E3F0, 0x24), 0, "817f0038 2b0b0000 419a00f4 3ba00001 "
      "9bab4b55 817f0038 896b4b78 2b0b0000 419a00a8")
r137_update = linked(address(update), 0x200)
assert not low_half_offsets(r137_update, 0x4B55)
words(r137_update, 0xE8, "817e0038 2b0b0000 419affe0 896b4b78")
post_title, _ = candidate("BrnPostTitleScreenLoad", update)
words(post_title, 0xC0, "817f0038 2b0b0000 419affe0 3ba00001 9bab4b55 817f0038 896b4b78")
assert low_half_offsets(post_title, 0x4B55) == [0xD0 // 4]

# ARTIST uses the selected SystemUserProfile index, then posts event 87 only with the gate set.
words(original(0x827E3000, 0x34), 0, "817d002c 387d105c 808b0000 48069205 817d0028 "
      "896b4b55 2b0b0000 419a006c 38c00001 38a00057 38810050 387d0044 4bab3061")
r137_signin = linked(address(signin), 0xD4)
words(r137_signin, 0x78, "38800000 387d105c 4bdc5151 38210080 484580c8")
assert not low_half_offsets(r137_signin, 0x4B55)

handler, calls = candidate("BrnGuiProfile", signin)
assert (0x84, bind_user) in calls
assert (0xCC, bind_user) in calls
assert (0xE4, queue) in calls
words(handler, 0x70, "2f1c0001 409a0034 817d002c 387d105c 808b0000")
words(handler, 0x88, "817d0028 896b4b55 2b0b0000 419a0054 39600000 38a00057 99610050 48000034")
assert low_half_offsets(handler, 0x4B55) == [0x8C // 4]

# The established R137 sign-out path still clears autosave, unbinds, and posts 86.
assert handler[0xA8:0xCC] == r137_signin[0x8C:0xB0]
words(r137_signin, 0xB4, "38c00001 9be10050 38a00056 38810050 387d0044")
words(handler, 0xD0, "38a00056 9be10050 38c00001 38810050 387d0044")
assert not any("UpdateUserSigninState" in name for _, name in calls)

receipt = {
    "scope": "ARTIST bytes, published R137 PE, and targeted Xbox 360 SDK COFF",
    "parent_commit": "2547909ec38b65d0fadcee2db903bd628730a5d8",
    "r137_commit": "ef3f9b543b9b2482e11d5c77914da04595d2b884",
    "r137_pe_sha256": ns["sha"](base / "native-game.pe"),
    "field": {"name": "mbIsAllowingSignInOutEvents", "gui_cache_offset": "0x4B55",
              "bytes": 1, "decfigs_header_line": 1673},
    "construct_clear": {"artist_va": "0x82505AC0", "candidate_offset": "0x11C"},
    "post_title_set": {"artist_va": "0x8247E400", "candidate_offset": "0xD0",
                       "before_intro_gate_at": "0x4B78"},
    "profile_gate_read": {"artist_va": "0x827E3014", "candidate_offset": "0x8C",
                          "event_id": 87, "event_payload_bytes": 1},
    "selected_user_index_load": {"artist_va": "0x827E3008", "candidate_offset": "0x80",
                                 "profile_manager_member": "0x2C", "system_user_profile_member": "0x0"},
    "sign_out": {"event_id": 86, "event_payload_bytes": 1, "r137_sequence_retained": True},
    "objects": {stem: {"path": item["object"], "sha256": ns["sha"](Path(item["object"]))}
                for stem, item in items.items()},
}
output = here / "R140_SIGNIN_GATE_COFF.json"
output.write_text(json.dumps(receipt, indent=2) + "\n", encoding="utf-8")
print(output, ns["sha"](output))

