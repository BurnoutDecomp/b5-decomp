"""Issue 70: replay the production route guidance with current/max speed separated."""
import argparse
import os
from pathlib import Path
from fxgs_common import Tree, definition, compile_and_run, report

os.environ.pop("NoDefaultCurrentDirectoryInExePath", None)
parser = argparse.ArgumentParser()
parser.add_argument("--rev")
args = parser.parse_args()
tree = Tree(args.rev)
source = tree.read("src/GameSource/GameState/StreetData/BrnGameStateStreetManager.cpp")
body = definition(source, "void StreetManager::FindUpcomingStreetsFromRoute(")
body = body.replace("StreetManager::", "Guidance::")
result = compile_and_run(Path(__file__).with_name("RouteGuidanceSpeed.cpp"),
                         "route_guidance_speed.inc", body, "RouteGuidanceSpeed")
raise SystemExit(report("run_route_guidance_speed", [], result, 1))
