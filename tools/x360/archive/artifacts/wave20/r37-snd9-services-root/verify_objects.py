import json
import pathlib
import re
import runpy
import struct
import subprocess

HERE = pathlib.Path(__file__).resolve().parent
WORK = pathlib.Path("D:/Reverse/BP-X360-Experimental-Worktrees/r1_vecfloat_repair_terra")
TOOLS = runpy.run_path(str(HERE.parent / "r36-link/prepare_seal.py"))
read, parse, sha = (TOOLS[key] for key in ("read", "parse", "sha"))
seal = read(HERE.parent / "r36-link/seal.json")
report = HERE / "native/compile-results.json"
rows = read(report)["results"]
assert len(rows) == 5 and all(row["passed"] and row["machine"] == 0x1F2 for row in rows)
by_name = {pathlib.Path(row["source"]).stem: row for row in rows}


def object_symbol(row, prefix):
    obj = pathlib.Path(row["object"]).read_bytes()
    matches = [symbol for symbol in parse(obj) if symbol[0].startswith(prefix)]
    name, kind, section, _, offset = matches[0]
    assert len(matches) == 1 and kind == "strong"
    header = struct.unpack_from("<8sIIIIIIHHI", obj, 20 + (section - 1) * 40)
    raw = obj[header[4] + offset:header[4] + offset + 4] if header[4] else b""
    return name, header, raw


guard, guard_section, guard_data = object_symbol(by_name["CMpegBase"], "?guSynthBypass@Snd@@")
output_count, _, output_data = object_symbol(by_name["sndhal"], "?giNumOutputs@Hal@Snd9@@")
assert guard_section[0].rstrip(b"\x00") == b".bss" and guard_section[9] & 0x80 and not guard_data
assert output_data == b"\x00\x00\x00\x01"
guard_listing = pathlib.Path(by_name["CMpegBase"]["assembly"]).read_text(errors="replace")
assert re.search(r"\blwz\s+r\d+," + re.escape(guard) + r"\(", guard_listing)
hal_listing = pathlib.Path(by_name["sndhal"]["assembly"]).read_text(errors="replace")
assert re.search(r"\bmulli\s+r\d+,r\d+,132\b", hal_listing)
assert re.search(r"\bmulli\s+r\d+,r\d+,92\b", hal_listing)
assert "Data 82F883D0 00000001 1" in (HERE / "init.txt").read_text()
assert "82C73B84 39600000 li" in (HERE / "init.txt").read_text()
missing = {r["symbol"] for r in read(HERE.parent / "r36-link/diagnostic-parsed.json")["unresolved"]}
assert "dword_83277C9C" in missing


def git(*args):
    return subprocess.check_output(["git", *args], cwd=WORK, text=True).strip()


assert git("rev-parse", "HEAD") == "59dcf86dbaa5379e09b01ec7640655bf42822ced"
assert git("rev-parse", "HEAD^") == seal["head"] and not git("status", "--porcelain")
result = {"base": seal["head"], "head": git("rev-parse", "HEAD"), "tree": git("rev-parse", "HEAD^{tree}"),
          "compiled": len(rows), "compile_sha256": sha(report), "original_init_sha256": sha(HERE / "init.txt"),
          "original_data_xrefs_sha256": sha(HERE / "refs.txt"), "retired_reference": "dword_83277C9C",
          "guard": guard, "output_count": output_count, "channel_stride": 0x84, "mixer_stride": 0x5C,
          "products": {row["source"]: {key: sha(row[key]) for key in ("object", "assembly")} for row in rows},
          "scope": "Targeted native compile and actual COFF verification. Full mixer lifecycle, link and runtime pending."}
output = HERE / "DELIVERY.json"
with output.open("x", encoding="utf-8") as stream:
    json.dump(result, stream, indent=2)
    stream.write("\n")
print(sha(output))
