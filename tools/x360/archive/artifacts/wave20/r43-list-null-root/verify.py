import hashlib
import json
import pathlib
import re

here = pathlib.Path(__file__).resolve().parent
wave = here.parent
exports = pathlib.Path("D:/Reverse/BP-Decomp_Workflow/.ida-exports/BURNOUT_X360_ARTIST.XEX")

def pin(path):
    path = pathlib.Path(path)
    with path.open("rb") as stream:
        return {"path": str(path.resolve()), "sha256": hashlib.file_digest(stream, "sha256").hexdigest()}

report = here / "native/compile-results.json"
compiled = json.loads(report.read_text())
assert compiled["passed"] == compiled["requested"] == 3 and compiled["failed"] == 0
names = {pathlib.PurePosixPath(row["source"]).stem for row in compiled["results"]}
assert names == {"VehicleList", "WheelList", "ChallengeList"}
assembly = {}
for row in compiled["results"]:
    assert row["machine"] == 0x1f2
    name = pathlib.PurePosixPath(row["source"]).stem
    listing = pathlib.Path(row["assembly"]).read_text(errors="replace")
    assert "skInvalidHandle" not in listing
    for method in ("Construct", "Destruct"):
        block = listing.split(f"; Begin code for function: ?{method}@{name}@", 1)[1]
        block = block.split("; End code for function:", 1)[0]
        assert "NULLResourcePtr" in block
        assert re.search(r"addi\s+r4,r\d+,20\b", block)
        assert re.search(r"bl\s+\?CreateFromHandle@BaseResourcePtr@", block)
    assembly[name] = {k: pin(row[k]) for k in ("object", "assembly", "log")}

original = [exports / (a + ".json") for a in ("0x82C60168","0x82677850","0x82677DB8","0x82677D00","0x828D93A0")]
init, *callers, target = (json.loads(path.read_text()) for path in original)
assert "dword_82FFB25C = &dword_82FFB248" in init["pseudocode"]
assert "dword_82FFB260 = 0" in init["pseudocode"]
assert all("dword_82FFB25C - 0x82FFB248" in item["assembly"] for item in callers)
assert "lwz       r10, 0(r9)" in target["assembly"]
trace = wave / "native-xenia-smoke/run-r42/startup-trace-functions.json"
rows = json.loads(trace.read_text())
assert any(row["start"] == "0x822bf3f8" and row["last_nonzero"] == "0x822bf410" for row in rows)
assert any(row["start"] == "0x828f11f0" and row["hits"] == 12 for row in rows)
receipt = {
    "base": "56cc0c4459f8f2b294a5d269eb3c0b032abb2f80",
    "head": "39f415402ccb541dfde551bd96ffb1ab9a6e8b31",
    "tree": "3d66b8916019844b4ed0e652e064d40959d117b4",
    "compile_report": pin(report), "objects": assembly, "launch_trace": pin(trace),
    "artist_exports": [pin(path) for path in original],
    "native_pointer": "Construct and Destruct pass the initialized per-TU NULLResourcePtr handle at +0x14",
}
out = here / "DELIVERY.json"
with out.open("x", encoding="utf-8") as stream:
    json.dump(receipt, stream, indent=2)
    stream.write("\n")
print(pin(out))
