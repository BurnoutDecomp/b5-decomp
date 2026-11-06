import pathlib

here = pathlib.Path(__file__).resolve().parent
text = (here.parent / "r38-root-combined/verify_boundary.py").read_text()
text = text.replace('r37-link', 'r39-link-repaired').replace(
    '327612daa19620f4462a8377fc63090c11f78644a017cb46d23a78c405085d38',
    '0b8240e026765c33a5a6b84227a9de342ee87a43b82af304fbc0d951b37a3639')
start = text.index('tunnel = ')
end = text.index('def object_symbols', start)
text = text[:start] + '''
snd = "src/SDKs/EATech/include/snd/"
new_sources = {snd + name + ".cpp" for name in ("sndfilter", "sndmix", "sndpacket", "sndsys")}
assert len(fresh) == 15 and fresh.keys() - baseline.keys() == new_sources
assert all(row["passed"] and row["machine"] == 0x1F2 for row in compiled)
defined = {"strong", "comdat", "common"}
raw_retired = {"byte_83277838", "dword_83277830", "dword_83277834"}
old_unresolved = {row["symbol"] for row in read(old / "diagnostic-parsed.json")["unresolved"]}
lobby = "vendor/dirtysdk/src/x360/lobbylogin.cpp"
expected = {
    "MIX_create": snd + "sndmix.cpp",
    "MIX_destroy": snd + "sndmix.cpp",
    "SNDPKTPLAYI_flushcallbackdata": snd + "sndpacket.cpp",
    "?gpfnSndAlloc@Snd@@3P6APAXI@ZA": snd + "sndmix.cpp",
    "?gpfnSndFree@Snd@@3P6AXPAX@ZA": snd + "sndmix.cpp",
    "?computeFrameTextureMap@CVideoFrameAnalyst@@QAAXPAE@Z": "src/SDKs/EATech/rwmovie/CVideoFrameAnalyst.cpp",
    "sub_82B1FD80": lobby,
    "sub_82B20070": lobby,
}
proof = set(expected)
targets = proof | raw_retired
assert len(targets) == 11 and targets <= old_unresolved

''' + text[end:]
start = text.index('for name in targets:')
end = text.index('missing = ', start)
text = text[:start] + '''
for name, owner in expected.items():
    assert providers[name] == {owner}, (name, providers[name])
for row in selected.values():
    assert not {name for name, kind, *_ in object_symbols(row) if kind == "undefined"} & raw_retired
''' + text[end:]
text = text.replace('9f53ea57697ea2afba7350b8b3772baeeb768d36',
                    'e1fb537b11395d1e21c1f0f6e0f142b7a453d8a9').replace(
                    'f7b15aee23e41e8f1086f225e9b5c09dea5f8703',
                    'c784b3eb58656b205fa3c09745cb10311faa618b')
text = text.replace('"unproved_added_references": sorted(missing),',
                    '"unproved_added_references": sorted(missing - old_unresolved),\n    "remaining_old_references": sorted(missing & old_unresolved),')
with (here / "verify_boundary.py").open("x", encoding="utf-8") as stream:
    stream.write(text)
