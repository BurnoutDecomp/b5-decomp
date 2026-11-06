
"""Compile the two R162 RaceCar lifecycle owners with the selected XDK commands."""
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
import json
import sys

OUT = Path(__file__).resolve().parent
sys.path.insert(0, str(OUT.parent / "r161-native-inputpads"))
import build_focus as b

b.OUT = OUT
BASE = "dfbb33b7725438f842ecec86f8968dcb94683380"
RACE = "src/GameSource/World/EntityModules/RaceCarEntityModule/"
SOURCES = (RACE + "BrnRaceCarEntityModule.cpp", RACE + "BrnRaceCarEntityModule_wS34_00.cpp")
PROVIDERS = (
    "src/GameSource/World/BrnWorldModule.cpp",
    "src/GameSource/World/BrnPlaceOnTrackManager.cpp",
    "src/GameShared/GameClasses/Module/CgsModuleSingleBuffered.cpp",
)
assert b.git("rev-parse", "HEAD^") == BASE and not b.git("status", "--porcelain=v1")
head = b.git("rev-parse", "HEAD")
tree = b.git("rev-parse", "HEAD^{tree}")
manifest = json.loads(b.SEAL.read_bytes())
rows = {row["source"]: row for row in manifest["results"]}
snapshots = {source: b.sha(b.REPO / source) for source in (*SOURCES, *PROVIDERS)}
inherited = {}
for source in PROVIDERS:
    before = b.git("rev-parse", f"9a78c87e3:{source}")
    assert before == b.git("rev-parse", f"{BASE}:{source}") == b.git("rev-parse", f"HEAD:{source}")
    assert rows[source]["passed"]
    inherited[source] = {"git_blob": before, "selected_object": rows[source]["object"],
                         "object_sha256": b.sha(Path(rows[source]["object"]))}
with ThreadPoolExecutor(max_workers=2) as pool:
    results = list(pool.map(b.compile_one, [rows[source] for source in SOURCES]))
assert head == b.git("rev-parse", "HEAD") and not b.git("status", "--porcelain=v1")
assert all(b.sha(b.REPO / source) == digest for source, digest in snapshots.items())
record = {"base": BASE, "commit": head, "tree": tree, "sdk": manifest["sdk"],
          "sealed_commands_sha256": b.sha(b.SEAL), "source_sha256": snapshots,
          "inherited_provider_objects": inherited, "results": results}
(OUT / "compile-receipt.json").write_text(json.dumps(record, indent=2) + "\n")
assert all(row["exit_code"] == 0 and not row["warnings"] for row in results)
print("2/2 Xbox 360 compiles passed, zero warnings")

