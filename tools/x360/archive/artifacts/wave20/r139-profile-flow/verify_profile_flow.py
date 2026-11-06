"""Verify R139 profile flow against ARTIST and the published R137 PE."""
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
source = "src/GameSource/GameFlowController/TopLevel/BrnGameMainFlowStates.cpp"
report = json.loads((here / "compile-results.json").read_text())
item, = report["results"]
assert report["passed"] == 1 and item["machine"] == 0x1F2 and item["source"] == source
candidate = Path(item["object"])
prior_record = next(row for row in json.loads((base / "invocation.json").read_text())["objects"]
                    if row["source"] == source)
prior = Path(prior_record["object"])
assert ns["sha"](prior) == prior_record["sha256"]

card_enter = "?OnEnter@MainGameFlowStateMemoryCard@@UAAXXZ"
disk_enter = "?OnEnter@MainGameFlowStateCheckDiskSpace@@UAAXXZ"
card_update = "?Update@MainGameFlowStateMemoryCard@@UAAXXZ"
save = "?SetSaveLoadState@GameMainFlowController@BrnGameMainFlowController@@QAAX_N@Z"
video = "?SetVideoState@GameMainFlowController@BrnGameMainFlowController@@QAAX_N@Z"
send = "?SendEvent@GameMainFlowController@BrnGameMainFlowController@@QAAXW4EMainGameFlowEvent@2@@Z"
scripted_update = "?Update@LoadingScriptedState@@UAAXXZ"
pause = "?gBrnScriptedLoadPaused@@3_NA"


def code_and_refs(obj, symbol):
    code, relocations = ns["collect"](obj, symbol)
    data = obj.read_bytes()
    refs = [(off, kind, ns["object_symbol"](data, index)[0])
            for off, index, kind in relocations if kind != 18]
    return code, relocations, refs


def words(data, offset, expected):
    actual = data[offset:offset + len(expected.split()) * 4]
    assert actual == bytes.fromhex(expected), (hex(offset), actual.hex(), expected)


old = {}
for symbol in (card_enter, disk_enter, card_update):
    code, relocations, refs = code_and_refs(prior, symbol)
    old[symbol] = refs
    va = address(symbol)
    linked_code = linked(va, len(code))
    ns["fixed_equal"](code, linked_code, relocations)
    for offset, kind, name in refs:
        if kind == 6 and name in (save, video, send, scripted_update):
            word, = struct.unpack_from(">I", linked_code, offset)
            assert ns["branch_target"](va + offset, word) == address(name)
assert not any(name in (save, video) for _, _, name in old[card_enter])
assert [(offset, name) for offset, kind, name in old[disk_enter]
        if kind == 6 and name in (save, video)] == [(0x84, save), (0x94, video)]
words(linked(address(save), 8), 0, "98830054 4e800020")
words(linked(address(video), 8), 0, "98830055 4e800020")

# ARTIST MemoryCard enters in the order unpark, save/load, video, GUI stage 4.
# It then overrides the stage to 5 only for an invite that needs no login.
enter_artist = original(0x823AAC48, 0x80)
words(enter_artist, 0x08, "616706b8 3d60009a 39000001 616606b9")
words(enter_artist, 0x20, "616a0644 3d600087 61648258 39600000 9969e28e")
words(enter_artist, 0x3C, "7d0939ae 812b02d0 7d0931ae 812b02d0 7ca9512e")
words(enter_artist, 0x54, "7d2b20ae 2b090000 4d9a0020 3d200087 61298259 "
      "7d2b48ae 2b090000 4c9a0020 39200005 7d2b512e")
words(original(0x823AAB00, 0x18), 0, "7d2b512e 38210060 8181fff8 7d8803a6 ebe1fff0 4e800020")

enter, _, refs = code_and_refs(candidate, card_enter)
assert (0x30, 17, pause) in refs
assert [(offset, name) for offset, kind, name in refs if kind == 6 and name in (save, video)] == [
    (0x38, save), (0x44, video)]
words(enter, 0x1C, "38800001 83eb0000 39600000 3fdf009a 3bde0664 996a0000 7fc3f378")
words(enter, 0x3C, "38800001 7fc3f378")
words(enter, 0x48, "3d5f009a 3d7f006f 3d000018 39200004 394a0644 396baa20 6108d828 "
      "912a0000 7d2b40ae 2b090000 419a0024 3d200018 6129d829 7d6b48ae "
      "2b0b0000 409a0010 39600005 916a0000")

disk, _, refs = code_and_refs(candidate, disk_enter)
assert not any(name in (save, video) for _, _, name in refs)
assert [(offset, kind) for offset, kind, name in refs if name == pause] == [
    (0x14, 16), (0x18, 17), (0x5C, 17)]
words(disk, 0x4C, "3d60009a 39200004 616a0644 39600001 997f0000 3d600000 816b0000 7d2b512e")

# Both invite fields come from the named native GUI provider. The existing
# GuiModule layout puts them 16 bytes earlier than ARTIST, for readers and writer.
words(original(0x825113F8, 0x28), 0, "3d7f0019 396bd838 894b0000 2b0a0000 "
      "409a00a8 3d400018 3ba00001 614ad839 9bab0000 7d5f50ae")
words(linked(0x82424F28, 0x28), 0, "3d7f0019 396bd828 894b0000 2b0a0000 "
      "409a00a8 3d400018 3ba00001 614ad829 9bab0000 7d5f50ae")

update_artist = original(0x823F2F98, 0xAC)
words(update_artist, 0x18, "2f0b0008 409a0080")
words(update_artist, 0x2C, "61688258 816a02d0 7d0b40ae 2b080000 419a001c 3d000087 "
      "61088259 7d0b40ae 2b080000 409a0008 39200001")
words(update_artist, 0x5C, "61080648 7d0b40ae 2b080000 409a0010 5529063e "
      "2b090000 419a0028")
words(update_artist, 0x80, "61290625 38800002 7d0b49ae")

update, _, refs = code_and_refs(candidate, card_update)
assert [(off, name) for off, kind, name in refs if kind == 6] == [
    (0x0C, scripted_update), (0x9C, send)]
words(update, 0x18, "2f0b0008 409a0084")
words(update, 0x28, "3d600018 6169d828 3d6a006f 396baa20 7d2b48ae 2b090000 419a001c "
      "3d200018 6129d829 7d6b48ae 2b0b0000 39600001 419a0008 39600000")
words(update, 0x60, "3d20009a 61290648 7d2a48ae 2b090000 409a0010 556b063e "
      "2b0b0000 419a0024")
words(update, 0x80, "3d60009a 39200001 616b0625 3c6a009a 38800002 38630664 7d2a59ae")

receipt = {
    "scope": "ARTIST bytes, published R137 PE, and targeted Xbox 360 SDK COFF",
    "source": source,
    "r137_commit": "ef3f9b543b9b2482e11d5c77914da04595d2b884",
    "r137_pe_sha256": ns["sha"](base / "native-game.pe"),
    "r137_coff_sha256": ns["sha"](prior),
    "candidate_coff": str(candidate),
    "candidate_coff_sha256": ns["sha"](candidate),
    "artist_memory_card_enter_va": "0x823AAC48",
    "artist_check_disk_enter_va": "0x823AAA98",
    "artist_memory_card_update_va": "0x823F2F98",
    "artist_invite_producer_va": "0x825113E8",
    "r137_invite_producer_va": "0x82424F18",
    "artist_invite_fields_from_gui_module": ["0x18D838", "0x18D839"],
    "candidate_invite_fields_from_gui_module": ["0x18D828", "0x18D829"],
    "memory_card_enter": ["unpark", "save/load flag", "video flag", "GUI stage 4",
                          "GUI stage 5 for invite without login"],
    "disk_space_enter": ["park clear", "START assert", "park set", "GUI stage 4"],
    "memory_card_update": ["scripted update", "stage 8 gate", "phase complete or invite without login",
                           "streaming wait", "STATEEND"],
}
output = here / "R139_PROFILE_FLOW_COFF.json"
output.write_text(json.dumps(receipt, indent=2) + "\n", encoding="utf-8")
print(output, ns["sha"](output))

