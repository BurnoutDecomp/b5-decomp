import hashlib
import json
import os
import pathlib
import struct
import subprocess

here = pathlib.Path(__file__).resolve().parent
wave = here.parent.parent
sdk = pathlib.Path("C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/b5-decomp/build/x360/sdk-6534.4/portable/XDK")
original = json.loads((wave / "r39-link-repaired/invocation.json").read_text())
source = here / "packaging_probe.cpp"
obj = here / "packaging-probe.obj"
exe = here / "packaging-probe.exe"
xex = here / "packaging-probe.xex"
temp = here / "tmp"
temp.mkdir(exist_ok=True)
env = os.environ.copy()
env.update(TEMP=str(temp), TMP=str(temp), PATH=str(sdk / "bin/win32") + os.pathsep + env["PATH"],
           LIB=str(sdk / "lib/xbox"))
cl = sdk / "bin/win32/cl.exe"
image = sdk / "bin/win32/imagexex.exe"
link = pathlib.Path(original["command"][0])
libraries = original["libraries"]
explicit = [row["path"] for row in libraries if pathlib.Path(row["path"]).name.lower() not in ("oldnames.lib", "xapilib.lib")]
explicit.append(str(sdk / "lib/xbox/xhv.lib"))
commands = [
    ("compile", [str(cl), "/nologo", "/c", "/X", "/O2", "/Oi", "/Ob2", "/GS-", "/GR-",
                 "/EHs-c-", "/Gy", "/D_XBOX", "/I" + str(sdk / "include/xbox"), "/Fo" + str(obj), str(source)]),
    ("link", [str(link), "/NOLOGO", "/MACHINE:PPCBE", "/SUBSYSTEM:XBOX", "/ENTRY:mainCRTStartup",
              "/VERBOSE:LIB", "/OUT:" + str(exe), "/LIBPATH:" + str(sdk / "lib/xbox"), str(obj), *explicit]),
    ("image", [str(image), "/IN:" + str(exe), "/OUT:" + str(xex)]),
    ("dump", [str(image), "/DUMP", "/IN:" + str(xex)]),
]
def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()
results = []
for name, cmd in commands:
    log = here / (name + ".log")
    with log.open("xb") as output:
        proc = subprocess.run(cmd, cwd=here, env=env, stdout=output, stderr=subprocess.STDOUT)
    results.append({"name": name, "command": cmd, "returncode": proc.returncode,
                    "log": str(log), "sha256": sha(log)})
    print(name, proc.returncode, flush=True)
    if proc.returncode:
        print(log.read_text(errors="replace")[-5000:])
        break
receipt = {"scope": "Portable XDK XHV dependency probe only, no game payload and no execution",
           "commands": results, "products": {}}
for path in (source, obj, exe, xex):
    if path.is_file():
        receipt["products"][path.name] = {"sha256": sha(path), "size": path.stat().st_size}
if obj.is_file():
    receipt["obj_machine"] = hex(struct.unpack_from("<H", obj.read_bytes())[0])
if xex.is_file():
    receipt["xex_magic"] = xex.read_bytes()[:4].decode("ascii")
(here / "RECEIPT.json").write_text(json.dumps(receipt, indent=2) + "\n")
assert len(results) == 4 and all(r["returncode"] == 0 for r in results)
assert receipt["obj_machine"] == "0x1f2" and receipt["xex_magic"] == "XEX2"


