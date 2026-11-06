import hashlib
import json
import pathlib
import re
import runpy
import subprocess
import sys

here = pathlib.Path(__file__).resolve().parent
work = pathlib.Path("D:/Reverse/BP-X360-Experimental-Worktrees/r1_vecfloat_repair_terra")
wave = here.parent
helpers = runpy.run_path(str(wave / "r38-link/prepare_seal.py"))
read, parse, sha = (helpers[key] for key in ("read", "parse", "sha"))
sys.path.insert(0, "D:/Reverse/BP-Decomp_Workflow/tools/re")
import x360rd

def git(*args):
    return subprocess.check_output(["git", "--no-optional-locks", *args], cwd=work)

base, head = (git("rev-parse", ref).decode().strip() for ref in ("HEAD^", "HEAD"))
assert base == "73dfafe5d9693359d462b07ceb216e9f38aae538"
sources = git("diff", "--name-only", base, head).decode().splitlines()
assert sources == ["src/SDKs/EATech/rwmovie/CVideoFrameAnalyst.cpp"]
patch = here / "candidate.patch"
with patch.open("xb") as stream:
    stream.write(git("diff", "--full-index", "--binary", base, head))

compile_report = here / "native/compile-results.json"
report = read(compile_report)
assert report["passed"] == report["requested"] == 3 and report["failed"] == 0
objects = {pathlib.Path(row["source"]).stem: row for row in report["results"]}
assert all(row["machine"] == 0x1f2 and row["passed"] for row in objects.values())
symbol = "?computeFrameTextureMap@CVideoFrameAnalyst@@QAAXPAE@Z"
def records(stem):
    return parse(pathlib.Path(objects[stem]["object"]).read_bytes())
assert any(name == symbol and kind in {"strong", "comdat"} for name, kind, *_ in records("CVideoFrameAnalyst"))
assert any(name == symbol and kind == "undefined" for name, kind, *_ in records("CVideoAnalyst"))
assert any(name.startswith("?calcGradient8x1B@") and kind in {"strong", "comdat"}
           for name, kind, *_ in records("CWMVFARoutines"))

listing = pathlib.Path(objects["CVideoFrameAnalyst"]["assembly"]).read_text(errors="replace")
assert re.search(r"38810070\s+addi\s+r4,r1,lumaScratch\$", listing)
assert re.search(r"39210070\s+addi\s+r9,r1,lumaScratch\$", listing)
assert re.search(r"39400008\s+li\s+r10,8", listing)
assert re.search(r"81090000\s+lwz\s+r8,0\(r9\)", listing)
assert re.search(r"39290004\s+addi\s+r9,r9,4", listing)
assert x360rd.rd(0x82A4722C, 4).hex() == "91440000"

diff = read(here / "differential.json")
assert len(diff) == 16 and {row["malloc_failure"] for row in diff} == {0, 1, 2}
assert all(len(row["matched_outputs"]) == 6 for row in diff)

def pin(p):
    p = pathlib.Path(p)
    return {"path": str(p.resolve()), "sha256": sha(p)}

receipt = {
    "base": base,
    "head": head,
    "paths": sources,
    "patch": pin(patch),
    "compile": pin(compile_report),
    "command": pin(here / "command.json"),
    "symbol": symbol,
    "native_products": [{name: pin(row[name]) for name in ("object", "assembly", "log")}
                        for row in objects.values()],
    "raw_ppc_sha256": {hex(start): hashlib.sha256(x360rd.rd(start, end-start)).hexdigest()
                      for start, end in ((0x82A47640, 0x82A486E0), (0x82A47050, 0x82A47238),
                                         (0x82A46610, 0x82A46710), (0x82A46710, 0x82A46AE8))},
    "vtable_raw": x360rd.rd(0x8210F890, 16).hex(),
    "source_evidence": [pin(here / name) for name in ("texture.asm", "gradient.asm", "caller.asm")],
    "differential": pin(here / "differential.json"),
    "differential_script": pin(here / "verify_ppc.py"),
    "matched_cases": 16,
    "limitations": [
        "PowerPC instruction interpreter with mock CRT allocation/memory and synthetic vtable object selecting the actual original or compiled kernels.",
        "Both frames were seeded at their respective scratch offsets with the same words. The inherited seven unwritten stack words affect gradientDifference. Stack history in an actual process can differ.",
        "Preserves odd-width progression, original floor-sized accumulator clear, and bottom-left accumulator index. No target execution or whole-program link.",
    ],
}
with (here / "DELIVERY.json").open("x", encoding="utf-8") as stream:
    json.dump(receipt, stream, indent=2)
    stream.write("\n")
print(pin(here / "DELIVERY.json"))
