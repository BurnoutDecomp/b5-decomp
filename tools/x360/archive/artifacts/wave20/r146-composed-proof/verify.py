"""Verify composed R146 GUI, Freeburn HUD and Lion blend-state PPC objects."""

import json
import runpy
import struct
import subprocess
from pathlib import Path


ROOT = Path(__file__).resolve().parent
WAVE = ROOT.parent
COMPOSED = WAVE / "r146-composed-preflight"
HEAD = "199c405159ddc6dc5637111c1365b8e8b605c477"
TREE = "57da5aa21cbb1aa7959477b5ddd41cec69288541"
BASE = "43044d5e77737e18572b7848676c6691720d5b95"
ARTIST_SHA = "6822f1c1f5d49e0b5df6a34d9fcc619bd331f4a4a5fe3081a0b10b2a03399655"
TOOLS = runpy.run_path(str(WAVE / "r143-linked-proof/verify_linked.py"))
sha, coff, word = (TOOLS[name] for name in ("sha", "coff_functions", "word"))
expect = TOOLS["expect_words"]

FUNCTIONS = {
    "BrnGuiCache": ("Construct@GuiCache", "RecEvent@GuiCache"),
    "BrnInGame": ("Update@InGame",),
    "BrnCrashNavEnterOnline_wI_05": ("HandleEnteringJunkyard@CrashNavEnterOnlineBase",),
    "BrnCarSelectUnlock": ("OnEnter@CarSelectUnlock", "Update@CarSelectUnlock"),
    "GameBridgeGameStateToX_EventFlowGuiEvents": ("TranslateEventFlowGameActionToGuiEvent@BrnGame",),
    "BrnFBurnMainHudState": ("Update@FBurnMainHudState",),
    "BrnLionBlendRenderer": ("EndRendering@LionBlendRenderer",),
    "LionParticleRender": ("EndRendering@LionParticleRender",),
}
for methods in FUNCTIONS.values():
    for method in methods:
        coff.__globals__["PREFIX"][method] = f"?{method}@"


def load(path, digest=None):
    data = path.read_bytes()
    if digest:
        assert sha(data) == digest, path
    return json.loads(data)


def branches(code, offset, kind):
    op = word(code, offset)
    assert op >> 16 == kind
    return offset + TOOLS["signed"](op & 0xFFFC, 16)


def symbol_payload(obj, name):
    _, _, _, table, count, opt, _ = struct.unpack_from("<HHIIIHH", obj)
    assert opt == 0
    strings = table + count * 18
    index = 0
    while index < count:
        raw, value, section, _, _, aux = struct.unpack_from("<8sIhHBB", obj, table + index * 18)
        if raw[:4] == bytes(4):
            start = strings + struct.unpack_from("<I", raw, 4)[0]
            raw = obj[start:obj.index(b"\0", start)]
        if raw.rstrip(b"\0").decode("ascii", errors="replace") == name and section > 0:
            hdr = 20 + 40 * (section - 1)
            size, offset = struct.unpack_from("<II", obj, hdr + 16)
            return obj[offset + value:offset + size]
        index += 1 + aux
    return None


def read_symbol_names(obj):
    table, count = struct.unpack_from("<II", obj, 8)
    strings = table + count * 18
    index = 0
    while index < count:
        offset = table + index * 18
        raw = obj[offset:offset + 8]
        if raw[:4] == bytes(4):
            start = strings + struct.unpack_from("<I", raw, 4)[0]
            raw = obj[start:obj.index(b"\0", start)]
        yield raw.rstrip(b"\0").decode("ascii", errors="replace")
        index += 1 + obj[offset + 17]


report_path = COMPOSED / "compile-results.json"
report = load(report_path)
candidate = load(COMPOSED / "CANDIDATE.json", "ab05981af189381789665d2c675512f6723cc3ee4e8f57e14b6919c8a0353ea2")
assert (candidate["head"], candidate["tree"], candidate["base"]) == (HEAD, TREE, BASE)
assert (report["requested"], report["passed"], report["failed"]) == (8, 8, 0) and report["state"] == "complete"
assert {row["source"] for row in report["results"]} == set(candidate["sources"])
rows = {Path(row["source"]).stem: row for row in report["results"]}
assert rows.keys() == FUNCTIONS.keys() and all(row["machine"] == 0x1F2 and row["passed"] for row in rows.values())
src_arg = next(arg for arg in next(iter(rows.values()))["command"] if arg.startswith("/I"))
assert all(src_arg in row["command"] for row in rows.values())
checkout = Path(src_arg[2:]).parent
git = lambda *args: subprocess.check_output(["git", "--no-optional-locks", "-C", str(checkout), *args], text=True)
assert git("rev-parse", "HEAD", "HEAD^{tree}", "HEAD^").splitlines() == [HEAD, TREE, BASE]
assert not git("diff", "HEAD", "--name-only")
assert set(git("diff", "--name-only", BASE, HEAD).splitlines()) == set(candidate["paths"])

gui_digest = "f35473349de867ea0601e68a781c4cada8c5c90e20911e643eb1704016ba11cd"
gui = load(WAVE / "r146-gui-unlock-events/SOURCE_ONLY_CANDIDATE.json", gui_digest)
frozen = {Path(row["source"]).stem: (Path(row["artifacts"]["object"]["path"]), row["artifacts"]["object"]["sha256"])
          for row in gui["objects"]}
for stem, folder, digest in (
    ("BrnFBurnMainHudState", "r149-fburn-hud", "00f74f5dfa2ac8330fcdb476522345ccdd41c0b0fb1c85ecd217688b4b07e5a9"),
    ("BrnLionBlendRenderer", "r152-lion-blend", "b99c111987b32f0356c3b19d8f9a3365528a8bfe86e9f08bd9b594b971d0d34f"),
    ("LionParticleRender", "r152-lion-blend", "48e6ac2ae259717fff293401dbbe77cad721803476ae7fffe89587ab795edeB3"),
):
    frozen[stem] = WAVE / folder / Path(rows[stem]["object"]).name, digest.lower()

objects, functions, compared = {}, {}, {}
for stem, methods in FUNCTIONS.items():
    path = Path(rows[stem]["object"])
    assert path.parent == COMPOSED
    objects[stem] = data = path.read_bytes()
    old_path, old_sha = frozen[stem]
    original = old_path.read_bytes()
    assert sha(original) == old_sha, stem
    baseline = coff(original, methods)
    for method, (name, code, refs) in coff(data, methods).items():
        other_name, other_code, other_refs = baseline[method]
        assert TOOLS["stable_name"](name) == TOOLS["stable_name"](other_name) and code == other_code, method
        assert TOOLS["canonical_bindings"](refs) == TOOLS["canonical_bindings"](other_refs), method
        functions[method] = code, refs
        compared[method] = {"bytes": len(code), "relocations": len(refs)}

artist = (WAVE / "native-xenia-smoke/run-r74-artist-no-trace/game/native-game.xex").read_bytes()
assert sha(artist) == ARTIST_SHA
payload = struct.unpack_from(">I", artist, 8)[0]
assert artist[payload:payload + 2] == b"MZ"
raw = lambda addr, size: artist[payload + addr - 0x82000000:payload + addr - 0x82000000 + size]

ctor, _ = functions["Construct@GuiCache"]
assert 0x3BC00000 in (word(ctor, off) for off in range(0x44, 0x11C, 4))
expect(ctor, 0x11C, 0x9BDF4B74, 0x616B3B9C, 0xFBDF4B68)
expect(ctor, 0x148, 0x93BF4B70)
expect(raw(0x82505888, 4), 0, 0x3BC00000)
expect(raw(0x82506028, 4), 0, 0x9BDF4B74)
expect(raw(0x82506034, 4), 0, 0xFBDF4B68)

events, erefs = functions["RecEvent@GuiCache"]
expect(events, 0x88, 0x3965FFF2)  # incoming ID - 14 for this compiler's first switch
expect(events, 0x1F40, 0x3965FE8B)  # incoming ID - 373 for its third switch
expect(raw(0x8250DE1C, 4), 0, 0x3965FFFC)  # ARTIST incoming ID - 4
expect(raw(0x8250F320, 4), 0, 0x3965FF0B)  # ARTIST incoming ID - 245
gui_events = {}
for event, target, artist_target, composed_words, artist_words in (
    (73, 0x1558, 0x8250F0C4, "39600001 997e4b74 e97f0000 f97e4b68", "39600001 997f4b74 e97e0000 f97f4b68"),
    (74, 0x157C, 0x8250F0E0, "39600000 997e4b74 f97e4b68", "39600000 997f4b74 f97f4b68"),
    (75, 0x2A4C, 0x82510F34, "38210130", "38210110"),
    (81, 0x1638, 0x8250EE20, "817f0000 917e4b70", "817e0000 917f4b70"),
    (374, 0x244C, 0x8250FFC4, "39600001 997e4b75", "39600001 997f4b75"),
):
    base, table = (14, erefs[0x94][2]) if event < 245 else (373, erefs[0x1F4C][2])
    ref = erefs[table + 4 * (event - base)]
    assert ref[0] == 2 and ref[2] == target
    art_base, art_table = (4, 0x8250DE40) if event < 245 else (245, 0x8250F350)
    assert word(raw(art_table + (event - art_base) * 4, 4), 0) == artist_target
    assert events[target:target + len(bytes.fromhex(composed_words))] == bytes.fromhex(composed_words)
    assert raw(artist_target, len(bytes.fromhex(artist_words))) == bytes.fromhex(artist_words)
    gui_events[str(event)] = {"artist_target": hex(artist_target), "coff_target": hex(target)}

producer, prefs = functions["TranslateEventFlowGameActionToGuiEvent@BrnGame"]
for off, artist_off, event, size in ((0x3E4, 0x823D78EC, 73, 8), (0x414, 0x823D79A4, 74, 1)):
    expect(producer, off, 0x38A00000 | event, 0x38C00000 | size)
    expect(raw(artist_off, 8), 0, 0x38C00000 | size, 0x38A00000 | event)
    assert prefs[off + 8][1].startswith("?AddEvent@?$VariableEventQueue@")
for method, off, address in (("HandleEnteringJunkyard@CrashNavEnterOnlineBase", 0xA8, 0x824CB070),
                             ("Update@InGame", 0x70C, 0x824E157C)):
    expect(functions[method][0], off, 0x896B4B74)
    assert word(raw(address, 4), 0) >> 26 == 34 and word(raw(address, 4), 0) & 0xFFFF == 0x4B74

hud, hrefs = functions["Update@FBurnMainHudState"]
hud_calls = {0x18: "StartMonitor", 0x60: "UpdateSetupState", 0x7C: "UpdateLoading", 0x98: "UpdateWFInit",
             0xB4: "UpdateRunning", 0xC8: "BeginAssert", 0x110: "FireAssert", 0x114: "EndAssert",
             0x134: "UpdatePermenant", 0x13C: "Clear", 0x144: "StopMonitor"}
for offset, name in hud_calls.items():
    assert hrefs[offset][0] == 6 and hrefs[offset][1].startswith(f"?{name}@")
assert hrefs[0xC][1] == hrefs[0x14][1] == hrefs[0x140][1] == "?miHudStateUpdate@GuiPerfmons@BrnGui@@2HA"
for state, (entry, artist_entry) in enumerate(zip((0x54, 0x70, 0x8C, 0xA8, 0xBC),
                                                  (0x82481A64, 0x82481A80, 0x82481A9C, 0x82481AB8, 0x82481ACC))):
    assert hrefs[0x40 + state * 4][2] == entry
    expect(hud, entry, 0x39600000 | state)
    expect(hud, entry + (4 if state == 4 else 8), 0x917E0038)
    assert word(raw(0x82481A50 + state * 4, 4), 0) == artist_entry
    expect(raw(artist_entry, 4), 0, 0x39600000 | state)
    expect(raw(artist_entry + (4 if state == 4 else 8), 4), 0, 0x917D0038)
for off in (0x6C, 0x88, 0xA4):
    assert branches(hud, off, 0x419A) == 0x124
expect(hud, 0x104, 0x38A00502)
expect(hud, 0x124, 0x817E0038, 0x2F0B0004, 0x419A000C)
expect(hud, 0x138, 0x807E0018)
expect(raw(0x82481B48, 4), 0, 0x38A00502)
expect(raw(0x82481B5C, 12), 0, 0x817D0038, 0x2F0B0004, 0x419A000C)
expect(raw(0x82481B70, 4), 0, 0x807D0018)
artist_calls = dict(zip((0x82481A28, 0x82481A70, 0x82481A8C, 0x82481AA8, 0x82481AC4, 0x82481AD8,
                         0x82481B54, 0x82481B58, 0x82481B6C, 0x82481B74, 0x82481B7C),
                        (0x821F1198, 0x82480EA0, 0x8247C640, 0x8247C710, 0x8247B660, 0x82817548,
                         0x82820810, 0x82817558, 0x824810F0, 0x8235F470, 0x821F1308)))
for address, target in artist_calls.items():
    op = word(raw(address, 4), 0)
    assert op & 0xFC000003 == 0x48000001
    assert address + TOOLS["signed"](op & 0x03FFFFFC, 26) == target
hud_obj = objects["BrnFBurnMainHudState"]
assert symbol_payload(hud_obj, hrefs[0x100][1]).startswith(b"..\\..\\..\\GameSource\\Gui/Flow/HUD/States/BrnFBurnMainHudState.cpp\0")
assert symbol_payload(hud_obj, hrefs[0xE4][1]).startswith(b"Should never call update in the following state")

state_lib = "?mgStateLibrary@ImRendererBase@CgsGraphics@@1UStateLibrary@12@A"
lion, lrefs = functions["EndRendering@LionBlendRenderer"]
assert lrefs[0xC][:2] == (16, state_lib) and lrefs[0x1C][:2] == (17, state_lib)
expect(lion, 0x1C, 0x836B0000)  # load standard slot at +0
expect(lion, 0x60, 0x7F63DB78)
assert lrefs[0x64][1].startswith("?SetState@Device@shadow@@")
assert all("dword_83010F20" not in name for stem in ("BrnLionBlendRenderer", "LionParticleRender")
           for name in read_symbol_names(objects[stem]))
def artist_address(high, low):
    return ((word(raw(high, 4), 0) & 0xFFFF) << 16) + TOOLS["signed"](word(raw(low, 4), 0) & 0xFFFF, 16)
assert artist_address(0x8227E628, 0x8227E634) == artist_address(0x827F1C48, 0x827F1C50) == 0x83010F20
expect(raw(0x827F1C34, 12), 0, 0x38C00000, 0x38A00007, 0x38800006)
expect(raw(0x827F1C5C, 4), 0, 0x907E0000)

r145 = WAVE / "r145-link"
assert load(r145 / "image-verification.json")["head"] == BASE
[provider] = [row for row in load(r145 / "invocation.json")["objects"] if Path(row["source"]).stem == "CgsImRenderer"]
provider_data = Path(provider["object"]).read_bytes()
assert sha(provider_data) == provider["sha256"]
coff.__globals__["PREFIX"]["RenderProvider"] = "?ConstructOnceOnly@ImRendererBase@"
_, render, rrefs = coff(provider_data, ("RenderProvider",))["RenderProvider"]
expect(render, 0x14, 0x38C00000, 0x38A00007, 0x38800006)
assert rrefs[0x24][1].startswith("?ConstructBlendState@ImRendererBase@")
assert rrefs[0x28][:2] == (16, state_lib) and rrefs[0x30][:2] == (17, state_lib)
expect(render, 0x3C, 0x907E0000)

receipt = {
    "head": HEAD, "tree": TREE, "parent": BASE, "artist_sha256": ARTIST_SHA,
    "candidate_sha256": sha((COMPOSED / "CANDIDATE.json").read_bytes()),
    "compile_report_sha256": sha(report_path.read_bytes()), "compiled": report["passed"],
    "composed_objects_sha256": {stem: sha(data) for stem, data in objects.items()},
    "component_objects_sha256": {stem: digest for stem, (_, digest) in frozen.items()},
    "compared_comdats": compared,
    "gui": {"init_stores": ["0x4b74", "0x4b68"], "dispatch": gui_events,
            "wire_payload_bytes": {"73": 8, "74": 1}, "consumer_offset": "0x4b74"},
    "freeburn_hud": {"phase_offset": "0x38", "queue_offset": "0x18", "assert_line": 1282,
                     "call_order_offsets": {hex(k): v for k, v in hud_calls.items()},
                     "artist_call_targets": {hex(k): hex(v) for k, v in artist_calls.items()}},
    "lion_blend": {"artist_address": "0x83010f20", "provider_symbol": state_lib,
                   "provider_r145_selected_sha256": provider["sha256"], "state_offset": 0, "shadow_symbol_absent": True},
    "stage": "static COFF",
}
out = ROOT / "R146_COMPOSED_COFF.json"
with out.open("x", encoding="utf-8", newline="\n") as stream:
    json.dump(receipt, stream, indent=2)
    stream.write("\n")
print(out, sha(out.read_bytes()))
