
"""Focused original Xbox SDK proof for the R164 GameData resource routes."""
from concurrent.futures import ThreadPoolExecutor
import json
from pathlib import Path
import sys

OUT = Path(__file__).resolve().parent
sys.path.insert(0, str(OUT.parent / "r161-native-inputpads"))
import build_focus as b

b.OUT = OUT
b.REPO = Path("C:/Users/adri1/.codex/worktrees/bp360/native_lion")
b.SEAL = OUT.parent / "r163-intake/composite-active-native.json"
BASE = "ac550e0556e38f534e1d15cbb5a468b132155286"
GAME = "src/GameSource/Resource/"
WORLD = "src/GameSource/World/EntityModules/WorldEntityModule/"
SOURCES = (GAME + "BrnGameDataModule.cpp", WORLD + "BrnWorldEntityModule.cpp",
           WORLD + "PVSModule/BrnPVSModule.cpp")
HEADERS = (GAME + "BrnGameDataModule.h", GAME + "SharedIO/BrnGameDataEvents.h",
           "src/GameShared/GameClasses/System/Resource/CgsResourceIOEvents.h")
head, parent, tree = (b.git("rev-parse", ref) for ref in ("HEAD", "HEAD^", "HEAD^{tree}"))
assert parent == BASE and not b.git("status", "--porcelain=v1")
assert b.git("rev-parse", "refs/heads/codex/experimental-x360-r158-native-batch") == (
    "b492f1163ba4988f7244cd7dd5f63824d4d7c964")
manifest = json.loads(b.SEAL.read_bytes())
rows = {row["source"]: row for row in manifest["results"]}
snapshots = {source: b.sha(b.REPO / source) for source in (*SOURCES, *HEADERS)}
with ThreadPoolExecutor(max_workers=3) as pool:
    results = list(pool.map(b.compile_one, [rows[source] for source in SOURCES]))
assert head == b.git("rev-parse", "HEAD") and not b.git("status", "--porcelain=v1")
assert all(b.sha(b.REPO / source) == digest for source, digest in snapshots.items())
receipt = {"head": head, "parent": parent, "tree": tree, "sdk": manifest["sdk"],
           "sealed_commands_sha256": b.sha(b.SEAL), "input_sha256": snapshots, "results": results}
(OUT / "compile-receipt.json").write_text(json.dumps(receipt, indent=2) + "\n")
assert all(row["exit_code"] == 0 and not row["warnings"] for row in results)
print("3/3 Xbox 360 compiles passed, zero warnings")

