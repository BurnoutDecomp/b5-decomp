
"""Compile the changed RaceCar owners using sealed native XDK invocations."""
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
import json
import sys

ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT.parent / "r161-native-inputpads"))
import build_focus as b

scope = sys.argv[1]
RACE = "src/GameSource/World/EntityModules/RaceCarEntityModule/"
owner = RACE + "BrnRaceCarEntityModule.cpp"
if scope == "prepare":
    sources = (owner, RACE + "BrnRaceCarEntityModule_Range.cpp", RACE + "BrnRaceCarComponentStreamers.cpp")
elif scope == "handle-abi":
    sources = (owner, "src/GameSource/World/BrnWorldModule.cpp")
else:
    raise AssertionError(scope)

b.OUT = ROOT / scope
b.OUT.mkdir(exist_ok=True)
head, tree, parent = (b.git("rev-parse", ref) for ref in ("HEAD", "HEAD^{tree}", "HEAD^"))
assert not b.git("status", "--porcelain=v1")
lifecycle = "de860910247c327292982d4362a7a04d5e0a09e3"
assert b.git("merge-base", lifecycle, head) == lifecycle
assert b.git("rev-parse", "HEAD^" if scope == "prepare" else "HEAD^^") == lifecycle
headers = (RACE + "BrnRaceCarEntityModule.h", "src/GameShared/GameClasses/System/Resource/CgsResourceHandle.h")
snapshots = {source: b.sha(b.REPO / source) for source in (*sources, *headers)}
manifest = json.loads(b.SEAL.read_bytes())
rows = {row["source"]: row for row in manifest["results"]}
with ThreadPoolExecutor(max_workers=len(sources)) as pool:
    results = list(pool.map(b.compile_one, [rows[source] for source in sources]))
assert head == b.git("rev-parse", "HEAD") and not b.git("status", "--porcelain=v1")
assert all(b.sha(b.REPO / source) == digest for source, digest in snapshots.items())
record = {"commit": head, "parent": parent, "tree": tree, "scope": scope, "sdk": manifest["sdk"],
          "sealed_commands_sha256": b.sha(b.SEAL), "source_sha256": snapshots, "results": results}
(b.OUT / "compile-receipt.json").write_text(json.dumps(record, indent=2) + "\n")
assert all(row["exit_code"] == 0 and not row["warnings"] for row in results)
print(f"{len(results)}/{len(results)} Xbox 360 compiles passed, zero warnings")

