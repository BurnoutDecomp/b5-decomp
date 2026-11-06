"""Verify the rebased camera lifetime against ARTIST, retained providers, and SDK 6534.4."""
import json
from pathlib import Path
import re
import struct
import subprocess
import sys

here = Path(__file__).resolve().parent
wave = here.parent
checkout = Path("C:/Users/adri1/.codex/worktrees/bp360/perfmon_ui_luna")
artist_candidate = "cf53c381cca7c0840ee1b608059036fd9efd5079"
r143_base = "c8ca8a670d2e3c3b112c7503d8b3cf6e559cb220"
base = "199c405159ddc6dc5637111c1365b8e8b605c477"
base_tree = "57da5aa21cbb1aa7959477b5ddd41cec69288541"
shared = wave / "r128-batch/verify_matches.py"
readers = {"__file__": str(shared)}
exec(shared.read_text().split("\nif len(sys.argv)==2:", 1)[0], readers)
sha = readers["sha"]
original = readers["original"]
report = json.loads((here / "compile-results.json").read_text())
assert report["state"] == "complete" and report["requested"] == 3
objects = {}
for row in report["results"]:
    assert row["passed"] and row["machine"] == 0x1F2
    path = Path(row["object"])
    assert path.parent == here
    objects[Path(row["source"]).stem] = path
camera = objects["BrnNetworkCameraX360"]
manager = objects["BrnNetworkManager_wN1_01"]

def function(obj, symbol):
    code, relocations = readers["collect"](obj, symbol)
    data = obj.read_bytes()
    branches = {offset: readers["object_symbol"](data, index)[0]
                for offset, index, kind in relocations if kind == 6}
    assert all(code[offset] & 0xFC == 0x48 for offset in branches), [(hex(i), code[i:i+4].hex(), n) for i,n in branches.items() if code[i] & 0xFC != 0x48]
    return code, relocations, branches

def require_words(code, offset, words):
    assert code[offset:offset + len(bytes.fromhex(words))] == bytes.fromhex(words), hex(offset)

def check_branch(code, offset, target):
    word, = struct.unpack_from(">I", code, offset)
    assert word >> 26 == 16
    displacement = word & 0xfffc
    if displacement & 0x8000:
        displacement -= 0x10000
    assert offset + displacement == target

def calls(code, named, labels):
    ignore = ("__savegprlr_", "__restgprlr_")
    actual = [name for _, name in sorted(named.items()) if not name.startswith(ignore)]
    assert len(actual) == len(labels)
    for name, label in zip(actual, labels):
        assert name == label or name.startswith("?" + label + "@"), (name, label)
    return actual

def artist_string(code, high, low):
    address = readers["data_target"](code, high, low)
    return original(address, 128).split(b"\0", 1)[0].decode()

symbols = {
    "ResetVideoPackets": "?ResetVideoPackets@CameraX360@BrnNetwork@@QAAXXZ",
    "Prepare": "?Prepare@CameraX360@BrnNetwork@@QAA_NPAVHeapMalloc@CgsMemory@@@Z",
    "Release": "?Release@CameraX360@BrnNetwork@@QAA_NXZ",
    "OpenSocket": "?OpenSocket@CameraX360@BrnNetwork@@AAAXXZ",
    "CloseSocket": "?CloseSocket@CameraX360@BrnNetwork@@AAAXXZ",
}
addresses = {
    "ResetVideoPackets": (0x825869F8, 0x50), "Prepare": (0x82593B28, 0x148),
    "Release": (0x8258B530, 0xE4), "OpenSocket": (0x82586AF8, 0x154),
    "CloseSocket": (0x82586C50, 0x100),
}
items = {}
for name, symbol in symbols.items():
    code, rels, branches = function(camera, symbol)
    va, size = addresses[name]
    items[name] = (code, rels, branches, original(va, size))

for name, labels in {
    "ResetVideoPackets": ["Construct", "memset"],
    "Prepare": ["PrintConsoleMemory", "ResetVideoPackets", "ResetRemotePlayers", "PrepareTextures",
                "XCamCreateStreamEngine", "BeginAssert", "FireAssert", "EndAssert", "OpenSocket",
                "PrintConsoleMemory", "SetFloatVal"],
    "Release": ["Release", "Release", "CloseSocket", "ResetVideoPackets", "ResetRemotePlayers", "SetFloatVal"],
}.items():
    code, rels, branches, artist = items[name]
    calls(code, branches, labels)
    if name in ("ResetVideoPackets", "Release"):
        readers["fixed_equal"](artist, code, rels)

reset, _, branches, artist = items["ResetVideoPackets"]
assert branches[0x1C] == "?Construct@CgsXOverlapped@CgsSystem@@QAAXXZ"
require_words(reset, 0x0C, "3be3000a 3bc00100 3ba00000 387f0406")
require_words(reset, 0x30, "3bdeffff b3bffffe 9bbf0422 2b1e0000 3bff0430 409affd4")
# The receive pump independently passes packet+0xA and loads the length at +8.
require_words(original(0x8258BD14, 0x2C), 0, "3a5f0410")
require_words(original(0x8258BD14, 0x2C), 8, "393f000a")
require_words(original(0x8258BD14, 0x2C), 0x28, "a15f0008")

prepare, _, prep_calls, artist = items["Prepare"]
assert prep_calls[0xB4] == "XCamCreateStreamEngine"
# Both use the same parameter pointer and out pointer. SDK clears 52 bytes before
# filling all ten nonzero members. ARTIST clears only the final three words.
require_words(prepare, 0x38, "3ba00000 39610050 7faaeb78 39200006 7d2903a6 f94b0000 396b0008 4200fff8 914b0000")
require_words(prepare, 0x5C, "3d6000a0 39400400 616b0078 3bc00001 3c9f0004 38610050 38843000 91410068 91610050 396003e8 93c10070 93c10074 91610054 39600040 91610058 39600080 9161005c 3960001e 91610060 39600004 91610064 9161006c")
require_words(artist, 0x38, "3d6000a0 3bc00000 616b0078 39400400 3ba00001 3c9f0004 38610050 93c10078 91610050 39600080 38843000 91410068 93a10070 93a10074 93c1007c 9161005c 39600040 93c10080 91610058 396003e8 91610054 3960001e 91610060 39600004 91610064 9161006c")
for new, old in [(0xB8, 0xA4), (0xC8, 0xB4), (0xE8, 0xD4), (0x10C, 0xF8), (0x11C, 0x108)]:
    assert prepare[new:new+4] == artist[old:old+4]
check_branch(prepare, 0xBC, 0xE0)
check_branch(artist, 0xA8, 0xCC)
require_words(prepare, 0xFC, "7fbf592e 7fdf51ae")
require_words(artist, 0xE8, "7fdf592e 7fbf51ae")
assert prepare[0x11C:0x144] == artist[0x108:0x130]
require_words(prepare, 0x144, "7d5f592e 7fdf492e 7fbf41ae 7fbf39ae")
require_words(artist, 0x130, "7d5f592e 7fbf492e 7fdf41ae 7fdf39ae")
require_words(items["Release"][0], 0x48, "3d600004 616b3000 7c7f582e 81630000 816b0004 7d6903a6 4e800421")

socket_names = {"socket", "bind", "ioctlsocket", "closesocket", "WSAGetLastError"}
for name, expected_api, branches_to in (
    ("OpenSocket", ["socket", "bind", "WSAGetLastError", "ioctlsocket"], [(0x58, 0x78), (0x8C, 0xFC)]),
    ("CloseSocket", ["closesocket", "WSAGetLastError"], [(0x24, 0x44), (0x50, 0xB4)]),
):
    code, _, named, artist = items[name]
    assert [name for _, name in sorted(named.items()) if name in socket_names] == expected_api
    for offset, target in branches_to:
        check_branch(code, offset, target)
    for assert_name in ("BeginAssert", "FireAssert", "EndAssert"):
        assert sum(sym.startswith("?" + assert_name + "@") for sym in named.values()) == 2

opened, _, _, art_open = items["OpenSocket"]
require_words(opened, 0x18, "38a000fe 38800002 38600002 f94b0000 f94b0008 39600002 b1610070 396003e9 b1610072")
require_words(opened, 0xFC, "39600001 807d0000 3c808004 38a10050 6084667e 91610050")
for new, old in [(0x64, 0x6C), (0x78, 0x80), (0xDC, 0x11C), (0x104, 0x138), (0x10C, 0x140)]:
    assert opened[new:new+4] == art_open[old:old+4]
closed, _, _, art_close = items["CloseSocket"]
require_words(closed, 0xB4, "3960ffff 917e0000")
for new, old in [(0x30, 0x30), (0xA0, 0xDC), (0xB4, 0xF0)]:
    assert closed[new:new+4] == art_close[old:old+4]

artist = items["Prepare"][3]
for artist_code, offsets, value in [
    (artist, (0xB0, 0xB8), r"d:\p4\b5_main\burnout\main\code\gamesource\unity\../Network/X360/BrnNetworkCameraX360.cpp"),
    (artist, (0xBC, 0xC0), "Unable to create XVID Streaming Engine!\n"),
    (art_open, (0x68, 0x70), "socket failed\n"),
    (art_open, (0xCC, 0xD4), "sendsock bind failed, error "),
    (art_close, (0x2C, 0x34), "mVDPSocket != INVALID_SOCKET"),
    (art_close, (0x90, 0x98), "close socket bind failed, error "),
]:
    assert artist_string(artist_code, *offsets) == value, offsets

remote, _, remote_calls = function(camera, "?ResetRemotePlayers@CameraX360@BrnNetwork@@QAAXXZ")
art_remote = original(0x82586A48, 0xB0)
# r31 points at the player ID (+0x34); both programs clear +0x6C and +0x6D.
require_words(remote, 0x14, "3ffb0004 3bc00000 3bff303c 3b80ffff")
require_words(art_remote, 0x14, "3ffb0004 3bc00000 3bff303c 3b80ffff")
require_words(remote, 0x58, "9bdf0038 9bdf0039")
require_words(art_remote, 0x50, "9bdf0038 9bdf0039")
# ARTIST's four qwords and one word in the descriptor and the XUID qword use
# the same zero register. The candidate uses a byte loop over the descriptor.
require_words(art_remote, 0x24, "fbdfffcc 387f001c fbdfffd4 fbdfffdc fbdfffe4 93dfffec")
require_words(art_remote, 0x4C, "fbdffff4")
require_words(remote, 0x24, "397fffcc 7fc9f378 39400024 7d4903a6 992b0000 396b0001 4200fff8")
require_words(remote, 0x44, "fbdffff4")
calls(remote, remote_calls, ["Release", "Construct", "Construct", "Construct", "Construct", "Construct", "Construct"])
camera_data = camera.read_bytes()
for method, values in {
    "Prepare": ["Unable to create XVID Streaming Engine!\n"],
    "OpenSocket": ["socket failed\n", "sendsock bind failed, error ", "\n"],
    "CloseSocket": ["mVDPSocket != INVALID_SOCKET", "close socket bind failed, error ", "\n"],
}.items():
    _, rels, _, _ = items[method]
    for index in {index for _, index, kind in rels if kind == 0x10}:
        symbol = readers["object_symbol"](camera_data, index)[0]
        if symbol.startswith("??_C@") or "KAC_CAMERA_FILE" in symbol:
            value = readers["symbol_literal"](camera_data, index)
            if "KAC_CAMERA_FILE" in symbol:
                assert value == r"d:\p4\b5_main\burnout\main\code\gamesource\unity\../Network/X360/BrnNetworkCameraX360.cpp"
            else:
                values.remove(value)
    assert not values

# Confirm in-object callers and their manager stage/teardown paths.
owners = {
    "CameraConstruct": (camera, "?Construct@CameraX360@BrnNetwork@@QAAXPAVBrnNetworkManager@2@PAVNetworkTextureDXTCompress@CgsNetwork@@@Z",
                        0x8C, "ResetVideoPackets"),
    "ManagerPrepare": (manager, "?Prepare@BrnNetworkManager@BrnNetwork@@QAA_NW4EFrameRate@CgsSystem@@PBULaunchData@BrnHW@@PAVHeapMalloc@CgsMemory@@@Z",
                       0x2F0, "Prepare"),
    "ManagerRelease": (manager, "?Release@BrnNetworkManager@BrnNetwork@@QAA_NXZ", 0x468, "Release"),
}
for obj, owner, offset, callee in owners.values():
    _, _, branches = function(obj, owner)
    assert branches[offset] == symbols[callee]

link = wave / "r145-link"
link_head = "43044d5e77737e18572b7848676c6691720d5b95"
inventory = json.loads((link / "source-inventory.json").read_text())
assert inventory["head"] == link_head
link_result = json.loads((link / "result.json").read_text())
assert link_result["exit_code"] == 0 and link_result["all_post_checks_pass"]
for name in ("native-game.map", "native-game.pe"):
    assert sha(link / name) == link_result["outputs"][str(link / name)]["sha256"]
get_address, load = readers["linked_reader"](link)
stub = get_address(symbols["Prepare"])
assert stub == get_address(symbols["Release"])
assert load(stub, 8).hex() == "386000014e800020"

# Close over every undefined symbol referenced by the five lifetime methods and
# their camera-local texture and player-reset helpers, including assert data.
selected = dict(symbols)
selected["ResetRemotePlayers"] = "?ResetRemotePlayers@CameraX360@BrnNetwork@@QAAXXZ"
selected["PrepareTextures"] = "?PrepareTextures@CameraX360@BrnNetwork@@QAAXPAVHeapMalloc@CgsMemory@@@Z"
frozen_dir = wave / "r151-xcam-lifetime"
frozen_report = json.loads((frozen_dir / "compile-results.json").read_text())
frozen_receipt = json.loads((frozen_dir / "R151_XCAM_LIFETIME_COFF.json").read_text())
assert frozen_receipt["head"] == artist_candidate
frozen_camera = Path(frozen_report["results"][0]["object"])
assert sha(frozen_camera) == frozen_receipt["target_objects"]["BrnNetworkCameraX360"]

def named_relocations(obj, rels):
    data = obj.read_bytes()
    return [(offset, kind, readers["object_symbol"](data, index)[0])
            for offset, index, kind in rels]

undefined = set()
for symbol in selected.values():
    code, rels, _ = function(camera, symbol)
    frozen_code, frozen_rels, _ = function(frozen_camera, symbol)
    assert code == frozen_code
    assert named_relocations(camera, rels) == named_relocations(frozen_camera, frozen_rels)
    for _, index, kind in rels:
        name, _, section = readers["object_symbol"](camera_data, index)
        if section == 0:
            undefined.add(name)
        elif section < 0:
            assert name == "@comp.id" and kind == 0x12

sdk = Path(report["sdk"])
searched = json.loads((link / "searched-libraries.json").read_text())
map_text = (link / "native-game.map").read_text(errors="replace")
providers = {}
git_paths_to_check = set()
for name in sorted(undefined - {"XCamCreateStreamEngine"}):
    address = get_address(name)
    owner, = re.findall(rf"(?m)^\s+[0-9a-fA-F]{{4}}:[0-9a-fA-F]+\s+{re.escape(name)}\s+{address:08x}\s+(?:f\s+)?(\S+)", map_text)
    if name in socket_names:
        assert owner == "xnet:sockapi.obj"
    if ":" in owner:
        library = sdk / ("lib/xbox/" + owner.split(":", 1)[0] + ".lib")
        assert str(library) in searched and library.is_file()
    else:
        git_paths_to_check.add(owner)
    providers[name] = {"R145_va": hex(address), "owner": owner}

archive_path = sdk / "lib/xbox/xcam.lib"
archive = archive_path.read_bytes()
assert archive.startswith(b"!<arch>\n/ ")
count, = struct.unpack_from(">I", archive, 68)
member_index = archive[72 + count * 4:].split(b"\0").index(b"XCamCreateStreamEngine")
member_offset, = struct.unpack_from(">I", archive, 72 + 4 * member_index)
assert archive[member_offset + 58:member_offset + 62] == b"`\n\xf2\x01"
assert str(archive_path) in searched
providers["XCamCreateStreamEngine"] = {
    "sdk_member_offset": hex(member_offset), "archive": str(archive_path), "sha256": sha(archive_path),
    "xcam_header_sha256": sha(sdk / "include/xbox/xcam.h"),
}
assert undefined == set(providers)
git = lambda *args: subprocess.check_output(["git", "-C", str(checkout), *args])
head = git("rev-parse", "HEAD").decode().strip()
assert git("rev-parse", "HEAD^").decode().strip() == base
assert git("rev-parse", base + "^{tree}").decode().strip() == base_tree
assert git("rev-parse", "codex/experimental-x360-r151-xcam-lifetime").decode().strip() == artist_candidate
assert not git("status", "--porcelain")
paths = git("diff-tree", "--no-commit-id", "--name-only", "-r", head).decode().splitlines()
assert set(paths) == {"src/GameSource/Network/Managers/X360/BrnNetworkCameraX360" + suffix
                      for suffix in (".cpp", ".h")}
blobs = {}
for path in paths:
    rev_parse = lambda rev: git("rev-parse", rev + ":" + path).decode().strip()
    assert rev_parse(r143_base) == rev_parse(base)
    assert rev_parse(artist_candidate) == rev_parse(head)
    assert git("show", head + ":" + path) == (checkout / path).read_bytes()
    blobs[path] = {"R143_R146": rev_parse(base), "R151_R147": rev_parse(head)}

# The source TUs defining those retained non-library symbols have identical
# Git blobs between the linked R145 and the exact private R146 parent.
trees = git("ls-tree", "-r", "--name-only", base, "src").decode().splitlines()
provider_sources = {}
for owner in sorted(git_paths_to_check):
    stem = re.sub(r"-[0-9a-f]{8}\.obj$", "", owner)
    provider, = [path for path in trees if Path(path).stem == stem and Path(path).suffix == ".cpp"]
    old = git("rev-parse", link_head + ":" + provider).decode().strip()
    new = git("rev-parse", base + ":" + provider).decode().strip()
    assert old == new
    provider_sources[owner] = {"source": provider, "blob": old}
receipt = {
    "state": "pass",
    "base": base, "head": head, "tree": git("rev-parse", "HEAD^{tree}").decode().strip(),
    "exact_camera_path_blobs": blobs,
    "retained_map": {"head": link_head, "sha256": sha(link / "native-game.map")},
    "frozen_R151_camera_sections_identical": list(selected),
    "source_sha256": {path: sha(checkout / path) for path in paths},
    "compile_report_sha256": sha(here / "compile-results.json"),
    "target_objects": {name: sha(path) for name, path in objects.items()},
    "layout": {"packet_count": 256, "packet_stride": "0x430", "length": "0x8", "payload": "0xa",
               "payload_bytes_reset": 1024, "overlapped": "0x410", "in_use": "0x42c",
               "stream_engine": "0x43000", "socket": "0x43b4c", "camera_size": "0x43ba8"},
    "prepare_sdk_words": [0x00A00078, 1000, 64, 128, 30, 4, 1024, 4, 1, 1, 0, 0, 0],
    "methods": {name: {"artist_va": hex(addresses[name][0]), "coff_bytes": len(code),
                       "exact_except_relocations": name in ("ResetVideoPackets", "Release"),
                       "coff_branches": {hex(offset): value for offset, value in named.items()}}
                for name, (code, _, named, _) in items.items()},
    "entry_paths": {name: {"owner": owner, "call_offset": hex(offset), "target": callee}
                    for name, (_, owner, offset, callee) in owners.items()},
    "remote_reset": {
        "artist_va": "0x82586a48",
        "artist_flag_stores": ["0x82586a98", "0x82586a9c"],
        "coff_flag_stores": ["0x58", "0x5c"],
        "flag_offsets": {"mbFeedActive": "0x6c", "mbFeedRequested": "0x6d"},
        "descriptor_and_xuid": "zero",
    },
    "providers": providers,
    "retained_provider_source_blobs": provider_sources,
    "scope": "Targeted native compile and ARTIST/COFF/provider checks only.",
}
output = json.dumps(receipt, indent=2) + "\n"
if "--write" in sys.argv:
    (here / "R147_CAMERA_PREFLIGHT.json").write_text(output)
print(output)


