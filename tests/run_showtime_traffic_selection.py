"""Issue 72: preserve authored vehicle choices during Showtime.

The complete production selector runs against real traffic records and a
deterministic random tape. --rev selects an earlier implementation.
"""
import argparse
from pathlib import Path
import re

from fxgs_common import Tree, compile_and_run, definition, report

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--rev")
args = parser.parse_args()
source = Tree(args.rev).read(
    "src/GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.cpp")
constants = re.findall(
    r"^\s*const u(?:32|64) (?:KU_SHOWTIME_\w+VEHICLE_ASSET_ID|KU_PERCENTAGE_ROLL_MODULUS)\s*=[^;]+;",
    source, re.M)
body = definition(source, "u8 TrafficEntityModule::PickVehicleToSpawn(")
body = body.replace("TrafficEntityModule::PickVehicleToSpawn", "Picker::PickVehicleToSpawn", 1)
result = compile_and_run(Path(__file__).with_name("ShowtimeTrafficSelection.cpp"),
    "showtime_traffic_selection.inc", "\n".join(constants) + "\n" + body,
    "ShowtimeTrafficSelection")
raise SystemExit(report("run_showtime_traffic_selection", [], result, 1030))
