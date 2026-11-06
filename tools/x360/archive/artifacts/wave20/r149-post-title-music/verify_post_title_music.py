"""Verify the R149 menu-stream producer and consumer in targeted PPC COFF."""
import json
from pathlib import Path
import struct
import subprocess
import sys

here = Path(__file__).resolve().parent
wave = here.parent
checkout = Path("C:/Users/adri1/.codex/worktrees/bp360/native_lion")
base = "bc7ae6234fb6d1993128a6018212cc9cfd86e13a"
base_tree = "292de86d70d0b034d502995355f6169a766a7d7e"
old_head = "7069e39809e380607c9bc04ea7e6bc7ca3846239"
helper = wave / "r128-batch/verify_matches.py"
r = {"__file__": str(helper)}
exec(helper.read_text().split("\nif len(sys.argv)==2:", 1)[0], r)
original = r["original"]
sha = r["sha"]
report = json.loads((here / "compile-results.json").read_text())
assert report["requested"] == 3 and report["state"] == "complete"
source_paths = {
    "PostTitle": "src/GameSource/Gui/Flow/HUD/States/BrnPostTitleScreenLoad.cpp",
    "Notify": "src/GameSource/Sound/Global/BrnMusicEffect.cpp",
    "BootLegal": "src/GameSource/Gui/Flow/HUD/States/BrnBootLegal.cpp",
}
symbols = {
    "PostTitle": "?Update@PostTitleScreenLoad@BrnGui@@UAAXXZ",
    "Notify": "?Notify@MusicEffect@Logic@BrnSound@@UAAXPBVMessageHeader@Io@CgsSound@@@Z",
    "BootLegal": "?Update@BootLegal@BrnGui@@UAAXXZ",
}
make_hash = "?MakeHash@Name@Playback@CgsSound@@SAIPBD@Z"
get_stream = "?GetStreamFromVideoName@MusicEffect@Logic@BrnSound@@ABAII@Z"
post_music = "??$OutputGuiEvent@VGuiEventPlayMusicOnMenuStream@CgsGui@@@StateInterface@CgsGui@@QAAXAAVGuiEventPlayMusicOnMenuStream@1@@Z"
title_movie = "?PlayAptMovie@StateInterface@CgsGui@@QAAXPBDH@Z"
objects = {row["source"]: row for row in report["results"]}
assert set(objects) == set(source_paths.values())

def words(code, offset, expected):
    expected = bytes.fromhex(expected)
    assert code[offset:offset+len(expected)] == expected, hex(offset)

def word(code, offset):
    return struct.unpack_from(">I", code, offset)[0]

def branch(read, address):
    return r["branch_target"](address, word(read(address, 4), 0))

def read_method(label):
    entry = objects[source_paths[label]]
    obj = Path(entry["object"])
    assert entry["passed"] and entry["machine"] == 0x1F2 and obj.parent == here
    raw = obj.read_bytes()
    code, rels = r["collect"](obj, symbols[label])
    named = {(off, kind): (index, *r["object_symbol"](raw, index))
             for off, index, kind in rels}
    assert len(named) == len(rels)
    return obj, raw, code, rels, named

def co_call(named, offset, symbol):
    assert named[(offset, 6)][1] == symbol

def co_literal(raw, named, upper, lower, value):
    index = named[(upper, 0x10)][0]
    assert index == named[(lower, 0x11)][0]
    assert r["symbol_literal"](raw, index) == value
    return named[(upper, 0x10)][1]

def artist_literal(upper):
    code = original(upper, 8)
    ptr = r["data_target"](code, 0, 4)
    return ptr, word(original(ptr, 4), 0)

link = wave / "r148-link"
inventory = json.loads((link / "source-inventory.json").read_text())
result = json.loads((link / "result.json").read_text())
assert inventory["head"] == base and result["exit_code"] == 0 and result["all_post_checks_pass"]
for name in ("native-game.map", "native-game.pe"):
    assert sha(link / name) == result["outputs"][str(link / name)]["sha256"]
address, load = r["linked_reader"](link)

art_ptr, art_text = artist_literal(0x8247E388)
assert (art_ptr, art_text) == (0x82F2CE5C, 0x820A9ADC)
assert artist_literal(0x8247782C) == (art_ptr, art_text)
assert original(art_text, 15) == b"Guns_And_Roses\0"
assert branch(original, 0x8247E390) == branch(original, 0x82477834) == 0x82689A50
assert branch(original, 0x8247E3AC) == branch(original, 0x8247784C)
assert branch(original, 0x826BBFA0) == 0x826BB9B0
words(original(0x826BBFA4, 0x20), 0, "81630000 917e0220 897f0014 997e0245")

post_obj, post_raw, post, post_rels, post_named = read_method("PostTitle")
post_va = address(symbols["PostTitle"])
post_r148 = load(post_va, len(post))
r["fixed_equal"](post_r148, post, post_rels)
wrong_va = r["data_target"](post_r148, 0x60, 0x64)
assert load(wrong_va, 13) == b"GunsAndRoses\0"
assert branch(load, post_va + 0x68) == address(make_hash)
assert branch(load, post_va + 0x84) == address(post_music)
literal_symbol = co_literal(post_raw, post_named, 0x60, 0x64, "Guns_And_Roses")
co_call(post_named, 0x68, make_hash)
co_call(post_named, 0x84, post_music)
words(post, 0x54, "897f0044 2b0b0000 419a004c")
words(post, 0x6C, "3bc00000 38810050 90610050 807f001c 9bc10054 9bc10055")
for offset, symbol_prefix in [(0x90, "??$PostCommand16@$0BD@@"), (0x9C, "??$PostCommand16@$0EG@@")]:
    assert post_named[(offset, 6)][1].startswith(symbol_prefix)
words(post, 0xA0, "39600002 917f0040")

boot_obj, boot_raw, boot, boot_rels, boot_named = read_method("BootLegal")
boot_va = address(symbols["BootLegal"])
boot_r148 = load(boot_va, len(boot))
# Kind 2 resolves a full ADDR32 jump table word. Check every local target,
# then compare all other code with the established relocation masks.
jump_count = 0
for offset, index, kind in boot_rels:
    if kind == 2:
        name, value, section = r["object_symbol"](boot_raw, index)
        assert name.startswith("$LN") and section > 0
        assert word(boot_r148, offset) == boot_va + value + word(boot, offset)
        jump_count += 1
assert jump_count == 148
r["masks"][2] = 0xffffffff
r["fixed_equal"](boot_r148, boot, boot_rels)
assert co_literal(boot_raw, boot_named, 0x54C, 0x550, "Guns_And_Roses") == literal_symbol
assert load(r["data_target"](boot_r148, 0x54C, 0x550), 15) == b"Guns_And_Roses\0"
co_call(boot_named, 0x548, title_movie)
co_call(boot_named, 0x554, make_hash)
co_call(boot_named, 0x56C, post_music)
assert branch(load, boot_va + 0x548) == address(title_movie)
assert branch(load, boot_va + 0x554) == address(make_hash)
assert branch(load, boot_va + 0x56C) == address(post_music)

music_va = address(post_music)
words(load(music_va, 0x54), 0x10, "38c00014 38a00028 38810050 3863000c")
words(load(music_va, 0x54), 0x38, "39600008 91610050 39600017 91610054 3960000c 91610058")

notify_obj, notify_raw, notify, notify_rels, notify_named = read_method("Notify")
notify_va = address(symbols["Notify"])
assert branch(load, notify_va + 0x340) == address(get_stream)
assert branch(load, notify_va + 0x354) == branch(load, notify_va + 0x368) == address(make_hash)
assert r["data_target"](load(notify_va, 0x36C), 0x344, 0x34C) == wrong_va
co_call(notify_named, 0x33C, get_stream)
words(notify, 0x330, "3960000c 387ffffc 917f0268")
assert word(notify, 0x33C) & 0xFC000003 == 0x48000001
words(notify, 0x340, "57cb063e 907f025c 9bdf0275 2b0b0000 9bbf0276")
assert not any("GunsAndRoses" in symbol[1] for symbol in notify_named.values())
other_hash_calls = [offset for (offset, kind), (_, name, _, _) in notify_named.items()
                    if kind == 6 and name == make_hash]
assert all(not 0x330 <= offset < 0x358 for offset in other_hash_calls)

def hash_name(name):
    value = 0x09b66e03
    for byte in name.encode():
        value = ((0x09b66c41 * (value + byte)) ^ 0x1de13c89) & 0xffffffff
    return value | 1

def git(*args):
    return subprocess.check_output(["git", "-C", str(checkout), *args])

def rev(name):
    return git("rev-parse", name).decode().strip()

assert rev(base + "^{tree}") == base_tree
assert rev("codex/experimental-x360-preserve-native-lion-7069e3980") == old_head
boot_source = source_paths["BootLegal"]
assert git("show", base + ":" + boot_source).replace(b"\r\n", b"\n") == (checkout / boot_source).read_bytes().replace(b"\r\n", b"\n")
head = rev("HEAD")
if "--write" in sys.argv:
    assert rev("HEAD^") == base and not git("status", "--porcelain")
    changed = git("diff-tree", "--no-commit-id", "--name-only", "-r", head).decode().splitlines()
    assert set(changed) == {source_paths["PostTitle"], source_paths["Notify"]}
    for path in source_paths.values():
        assert git("show", head + ":" + path).replace(b"\r\n", b"\n") == (checkout / path).read_bytes().replace(b"\r\n", b"\n")

receipt = {
    "state": "pass",
    "base": base,
    "base_tree": base_tree,
    "head": head,
    "tree": rev("HEAD^{tree}"),
    "retained_head": old_head,
    "compile_report_sha256": sha(here / "compile-results.json"),
    "objects": {label: {"path": objects[path]["object"], "sha256": sha(Path(objects[path]["object"])),
                         "source_sha256": sha(checkout / path)}
                for label, path in source_paths.items()},
    "artist": {"post_title_literal_pointer_cell": hex(art_ptr), "post_title_literal": hex(art_text),
               "post_title_hash_call": "0x8247e390", "boot_legal_hash_call": "0x82477834",
               "notify_lookup_call": "0x826bbfa0", "notify_stream_name_store": "0x826bbfa8"},
    "r148_linked": {"post_title_hash_call": hex(post_va + 0x68), "wrong_literal": hex(wrong_va),
                    "notify_extra_hash_calls": [hex(notify_va + offset) for offset in (0x354, 0x368)],
                    "boot_legal_hash_call": hex(boot_va + 0x554), "music_wrapper": hex(music_va)},
    "coff": {
        "PostTitle": {"hash_call": "0x68", "post_typed_music": "0x84", "post_loading_screen": "0x90",
                      "post_phase_complete": "0x9c", "finished_state_store": "0xa4",
                      "same_fixed_words_as_R148": True},
        "Notify": {"normal_lookup_call": "0x33c", "direct_result_store": "0x344", "alias_hash_calls": 0,
                   "other_hash_calls": [hex(offset) for offset in other_hash_calls]},
        "BootLegal": {"hash_call": "0x554", "post_typed_music": "0x56c",
                      "same_fixed_words_as_R148": True, "same_jump_table_targets": jump_count},
    },
    "gui_event": {"wrapper_header": [8, 23, 12], "channel": 40, "size": 20},
    "name_hashes": {name: hex(hash_name(name)) for name in ("Guns_And_Roses", "GunsAndRoses")},
    "scope": "Three compile-only native PPC objects and static comparison against ARTIST and the sealed R148 map and PE.",
}
output = json.dumps(receipt, indent=2) + "\n"
if "--write" in sys.argv:
    with (here / "R149_POST_TITLE_MUSIC_COFF.json").open("x", encoding="utf-8") as stream:
        stream.write(output)
print(output, end="")
