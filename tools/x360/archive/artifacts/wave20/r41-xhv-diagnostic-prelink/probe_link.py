import hashlib
import json
import os
import pathlib
import runpy
import subprocess

here = pathlib.Path(__file__).resolve().parent
wave = here.parent
base_dir = wave / "r40-link-repaired"
candidate = wave / "r41-xhv-preflight"
helpers = runpy.run_path(str(base_dir / "run_once.py"))
sha = helpers["sha"]
read = lambda path: json.loads(path.read_text(encoding="utf-8-sig"))

base_seal = base_dir / "seal.json"
assert sha(base_seal) == "fed9eb3508f52daea6d1d046a692ccd66cde8aaa4764d2c1fb6ecddceae85193"
seal = read(base_seal)
assert seal["head"] == "8e9b44e0058424905069834f4953fe3358ffe492"
coff = candidate / "COFF_BOUNDARY.json"
assert sha(coff) == "bff53a37acb4f2989df074b210408a4d404f8c558d5033ecb70c7a81c274b686"
boundary = read(coff)
assert len(boundary["expected_cleared"]) == 15 and not boundary["unproved_added_references"]
fresh_report = candidate / "native/compile-results.json"
assert sha(fresh_report) == boundary["compile_report"]["sha256"]
compiled = read(fresh_report)
assert compiled["passed"] == compiled["requested"] == 26 and compiled["failed"] == 0
base_selected = {x["source"]: x for x in seal["selected"]}
fresh = {x["source"]: x for x in compiled["results"]}
assert len(fresh) == 26 and all(x["passed"] and x["machine"] == 0x1f2 for x in fresh.values())
assert len(fresh.keys() - base_selected.keys()) == 4
assert not {x["source"] for x in seal["excluded"]} & fresh.keys()

selected = []
for source, row in (base_selected | fresh).items():
    path = pathlib.Path(row["object"])
    digest = sha(path)
    if source not in fresh:
        assert digest == row["sha256"], path
    selected.append({"source": source, "object": str(path), "sha256": digest})
assert len(selected) == 3604
sdk = helpers["SDK"]
xhv = sdk / "lib/xbox/xhv.lib"
sdk_libraries = [*seal["libraries"], {"path": str(xhv),
                  "sha256": "d4cff97c603e15bce167559bca2e7d02f6676c8a958f0e66b37e5c873497485e"}]
assert len(sdk_libraries) == 17
for row in sdk_libraries + seal["tool_files"]:
    assert sha(row["path"]) == row["sha256"], row["path"]
explicit = [row["path"] for row in sdk_libraries
            if pathlib.Path(row["path"]).name.lower() not in ("oldnames.lib", "xapilib.lib")]
rsp = here / "diagnostic.rsp"
options = ["/NOLOGO", "/MACHINE:PPCBE", "/SUBSYSTEM:XBOX", "/ENTRY:mainCRTStartup",
           "/VERBOSE:LIB", "/OUT:" + str(here / "diagnostic-game.exe"),
           "/PDB:" + str(here / "diagnostic-game.pdb"),
           "/MAP:" + str(here / "diagnostic-game.map"),
           "/LIBPATH:" + str(sdk / "lib/xbox")]
with rsp.open("x", encoding="ascii") as stream:
    stream.write("\n".join(f'"{item}"' for item in options + [row["object"] for row in selected] + explicit) + "\n")
temp = here / "tmp"
temp.mkdir(exist_ok=False)
env = os.environ.copy()
env["TEMP"] = env["TMP"] = str(temp)
env["PATH"] = str(sdk / "bin/win32") + os.pathsep + env["PATH"]
env["LIB"] = str(sdk / "lib/xbox")
command = [str(helpers["HOST"] / "link.exe"), "@" + str(rsp)]
inputs = {"scope": "Private diagnostic from the original R41 targeted preflight. No runtime.",
          "preflight_head": boundary["private_head"], "command": command, "rsp_sha256": sha(rsp),
          "selected": selected, "libraries": sdk_libraries, "base_seal_sha256": sha(base_seal),
          "coff_proof_sha256": sha(coff)}
with (here / "invocation.json").open("x", encoding="utf-8") as stream:
    json.dump(inputs, stream, indent=2)
with (here / "link.log").open("xb") as stream:
    proc = subprocess.run(command, cwd=here, env=env, stdout=stream, stderr=subprocess.STDOUT)
assert sha(rsp) == inputs["rsp_sha256"]
assert all(sha(x["object"]) == x["sha256"] for x in selected)
assert all(sha(x["path"]) == x["sha256"] for x in sdk_libraries + seal["tool_files"])
log = here / "link.log"
rows, duplicates = helpers["parse"](log.read_text(errors="replace"), selected)
outputs = {}
for path in here.glob("diagnostic-game.*"):
    with path.open("rb") as stream:
        magic = stream.read(4).hex()
    outputs[str(path)] = {"sha256": sha(path), "bytes": path.stat().st_size, "magic": magic}
result = {"scope": inputs["scope"], "returncode": proc.returncode,
          "distinct_unresolved": sorted({row["symbol"] for row in rows}),
          "duplicates": duplicates, "log_sha256": sha(log), "outputs": outputs}
with (here / "RESULT.json").open("x", encoding="utf-8") as stream:
    json.dump(result, stream, indent=2)
    stream.write("\n")
print("PRIVATE DIAGNOSTIC", proc.returncode, len(result["distinct_unresolved"]), len(duplicates), outputs)
