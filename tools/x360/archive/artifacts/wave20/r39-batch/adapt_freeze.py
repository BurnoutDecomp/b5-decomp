import pathlib

here = pathlib.Path(__file__).resolve().parent
old = (here.parent / "r38-batch/freeze.py").read_text()
body = old.replace('BASE = "5ea596e049a8eaff6253c1513e54d9208ef2751e"',
                   'BASE = "73dfafe5d9693359d462b07ceb216e9f38aae538"')
start = body.index('verbose = WAVE')
end = body.index('base_tree =', start)
body = body[:start] + '''
audio = WAVE / "r38-ealayer3-root/DELIVERY.json"
ai = WAVE.parent / "wave36/ai-debug-render-terra/RECEIPT-complete.json"
addr = WAVE.parent / "wave37/sockaddr-parse-terra/DELIVERY.json"
tcp = WAVE.parent / "wave37/commtcp-native-terra/receipt.json"
srp = WAVE.parent / "wave37/commsrp-native-terra/receipt.txt"
srp_provenance = dict(line.split("=", 1) for line in srp.read_text().splitlines())

groups = [
    make_candidate("ai-debug-render-world", ai),
    make_candidate("ea-layer3-core-allocation", audio),
    make_candidate("dirtysdk-sockaddr-parse", addr),
    ("dirtysdk-commtcp", read(tcp)["base"], read(tcp)["candidate"], (tcp,)),
    ("dirtysdk-commsrp", srp_provenance["parent"], srp_provenance["candidate"], (srp,)),
]
assert not git("status", "--porcelain")
assert git("rev-parse", "HEAD").decode().strip() == read(audio)["head"]
''' + body[end:]
start = body.index('assert len(final_paths)')
end = body.index('with patch.open("xb")', start)
body = body[:start] + '''
assert len(final_paths) == 14 and all(row["new"] and row["old"] != row["new"] for row in final_paths)
assert {row["path"] for row in final_paths if row["old"] is None} == {
    "vendor/dirtysdk/src/x360/commtcp_native_x360.cpp",
    "vendor/dirtysdk/src/x360/commsrp_native_x360.cpp",
    "vendor/renderware/src/rw/audio/core/EaLayer3Dec.cpp",
    "vendor/renderware/src/rw/audio/core/EaLayer3Tables.inl",
}
patch = HERE / "r39.patch"
''' + body[end:]
body = (body.replace('r38-native-intake-v1', 'r39-native-intake-v1')
        .replace('"expected_total_native": 3695', '"expected_total_native": 3698')
        .replace('r37-intake/', 'r38-intake/')
        .replace('r37-link/', 'r38-link/'))
with (here / "freeze.py").open("x", encoding="utf-8") as stream:
    stream.write(body)
