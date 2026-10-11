"""Exercise production route copy/append through real car and response owners."""
import os
import re
from pathlib import Path
from fxgs_common import Tree, definition, compile_and_run, report

os.environ.pop("NoDefaultCurrentDirectoryInExePath", None)
source = Tree().read("src/GameSource/World/AI/Route/BrnRoute.cpp")
body = "\n".join(definition(source, signature) for signature in (
    "bool Route::AddNode(const RouteNode& lrNode)",
    "void Route::Construct(const Route* lpSource)",
))
fixture = Path(__file__).with_name("AiRouteStorage.cpp")
numeric = compile_and_run(fixture, "ai_route_storage_source.inc", body, "AiRouteStorage")
total = len(re.findall(r"\bCheck\s*\(", fixture.read_text(encoding="utf-8"))) - 1
raise SystemExit(report("run_ai_route_storage", [], numeric, total))
