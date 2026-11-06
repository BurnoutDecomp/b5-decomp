from pathlib import Path
import hashlib
import json
import struct

root = Path(__file__).parent
parser_source = (root.parent / "r137-intake/verify_netconn.py").read_text()
exec(parser_source[parser_source.index("def coff("):parser_source.index("worker=")])
objects = root / "native-selected"
playback = objects / "CgsSoundPlaybackModule-46f08158.obj"
factory = objects / "CgsAemsFactory-223642e7.obj"
interface = objects / "CgsAemsInterfaceImplementation-c3edfff6.obj"
assign = "??4?$Handle@VAemsFactory@Playback@CgsSound@@@Playback@CgsSound@@QAAAAU012@ABU012@@Z"
release = "?Release@AemsRWSampleFactory@Playback@CgsSound@@UAAXXZ"
vtable = "??_7AemsFactory@Playback@CgsSound@@6BIAemsSamplePlayerFactory@Snd9@@@"

def get_symbol(obj, symbol):
    symbols, sections = coff(obj.read_bytes())
    (data, relocations), start = find(symbol, symbols, sections)
    return data[start:], [(off - start, typ, name) for off, typ, name in relocations if off >= start]

def check_bytes(obj, symbol, offset, expected):
    data, _ = get_symbol(obj, symbol)
    expected = bytes.fromhex(expected)
    assert data[offset:offset + len(expected)] == expected, (symbol, hex(offset))

def check_rel(obj, symbol, offset, typ, target):
    _, relocations = get_symbol(obj, symbol)
    assert (offset, typ, target) in relocations, (symbol, hex(offset), relocations)

playback_release = "?Release@Module@1Playback@CgsSound@@UAA_NXZ"
playback_prepare = "?Prepare@Module@1Playback@CgsSound@@UAA_NPAUIResourceAllocator@rw@@PAVLinearMalloc@CgsMemory@@@Z"
check_bytes(playback, assign, 0x1c, """
    817e0000 2b0b0000 419a0010 814b0008 394a0001 914b0008
    807f0000 2b030000 419a0014 81630000 816b0004 7d6903a6 4e800421
    817e0000 7fe3fb78 917f0000
""")
check_bytes(playback, playback_release, 0x188, "38810050 93e10050 387e2260 93fe225c 4bfffe69")
check_rel(playback, playback_release, 0x198, 6, assign)
check_bytes(playback, playback_prepare, 0x278, """
    3bbe2260 38810058 7fa3eb78 4bfffd7d
    80610058 2b030000 419a0014 81630000 816b0004 7d6903a6 4e800421
""")
check_rel(playback, playback_prepare, 0x284, 6, assign)
assert any(off == 4 and name == release for off, _, name in get_symbol(factory, vtable)[1])
check_bytes(interface, release, 0, "4e800020")
check_bytes(playback, "?DumpRegistries@Module@1Playback@CgsSound@@QAAXXZ", 0xd0, "807f0060")
check_bytes(playback, "?AddRegistry@Module@1Playback@CgsSound@@QAAXAAURegistry@23@I@Z", 0xe8, "83de0060")
compile_results = json.loads((objects / "compile-results.json").read_text())
assert len(compile_results["results"]) == 6 and all(x["passed"] and x["machine"] == 0x1f2 for x in compile_results["results"])
report = {
    "baseline": "ef3f9b543b9b2482e11d5c77914da04595d2b884",
    "validation": "Xbox 360 SDK PPC COFF bytes and relocations, six TUs",
    "artist": {
        "prepare_assign": "0x826E9354, 0x826E937C, then temp IAems slot +4 at 0x826E9398",
        "release_assign": "0x826C10D4–0x826C10DC",
        "assign_body": "0x826A7A08. Acquire non-null incoming at object +8, invoke old object's primary vtable +4, copy incoming pointer",
        "dump_registry": "0x82694284–0x82694288. Load complete AEMS pointer from module +0x2260 and registry from object +0x60"
    },
    "r137": "Linked 0x8229250C–0x8229251C called Object::Release directly for module +0x2260",
    "candidate": {
        "prepare": "module +0x2260, REL24 to typed assignment at +0x284, then temp IAems +4",
        "release": "module +0x2260, REL24 to typed assignment at +0x198 between other factory releases",
        "assign": "Acquire +8 at +0x28–0x30, old primary vtable +4 at +0x34–0x4C, pointer store at +0x58",
        "vtable": "AemsFactory IAems primary vtable +4 relocates to AemsRWSampleFactory::Release, body 4e800020",
        "registry_consumers": "DumpRegistries and AddRegistry load complete-object +0x60"
    },
    "objects": {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(objects.glob("*.obj"))},
    "scope": "Static and compile-only"
}
(root / "R139_AEMS_PROOF.json").write_text(json.dumps(report, indent=2) + "\n")
print(json.dumps(report, indent=2))
