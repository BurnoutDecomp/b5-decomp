import pathlib

here = pathlib.Path(__file__).resolve().parent
text = (here.parent / "r38-root-combined/verify_boundary.py").read_text()
text = text.replace('r37-link', 'r38-link').replace(
    '327612daa19620f4462a8377fc63090c11f78644a017cb46d23a78c405085d38',
    '07f13d2677a81f1274f58f5d0fb1e6bef5b5dd98cf0c07cdc8615ba95e6b7350')
start = text.index('tunnel = ')
end = text.index('def object_symbols', start)
text = text[:start] + '''
srp = "vendor/dirtysdk/src/x360/commsrp_native_x360.cpp"
tcp = "vendor/dirtysdk/src/x360/commtcp_native_x360.cpp"
codec = "vendor/renderware/src/rw/audio/core/EaLayer3Dec.cpp"
address = "vendor/dirtysdk/src/x360/dirtynet_addr_x360.cpp"
ai = "src/GameSource/World/AI/BrnAIDebugComponent.cpp"
assert len(fresh) == 19 and fresh.keys() - baseline.keys() == {srp, tcp, codec}
assert all(row["passed"] and row["machine"] == 0x1F2 for row in compiled)
old_codec = "?CreateInstance@EaLayer3DecBase@core@audio@rw@@SAHHE@Z"
new_codec = "?CreateInstance@EaLayer3DecBase@core@audio@rw@@QAAHE@Z"
render = "?RenderWorld@AIDebugComponent@BrnAI@@UAAXPAUDebug3DImmediateRender@CgsDev@@@Z"
targets = {"CommSRPConstruct", "CommTCPConstruct", old_codec, render}
assert targets <= {row["symbol"] for row in read(old / "diagnostic-parsed.json")["unresolved"]}
defined = {"strong", "comdat", "common"}
expected = {"CommSRPConstruct": srp, "CommTCPConstruct": tcp, new_codec: codec,
            render: ai, "SockaddrInParse": address}
proof = set(expected)

''' + text[end:]
start = text.index('for name in targets:')
end = text.index('missing = ', start)
text = text[:start] + '''
for name, owner in expected.items():
    assert providers[name] == {owner}, (name, providers[name])
for row in selected.values():
    assert all(name != old_codec or kind != "undefined" for name, kind, *_ in object_symbols(row))
''' + text[end:]
text = text.replace('9f53ea57697ea2afba7350b8b3772baeeb768d36',
                    'eebba8dfa62cf24239c49aefdd950f975758087d').replace(
                    'f7b15aee23e41e8f1086f225e9b5c09dea5f8703',
                    '8dddce7292099871fefafb5d4c11194c4e1115f2')
with (here / "verify_boundary.py").open("x", encoding="utf-8") as stream:
    stream.write(text)
