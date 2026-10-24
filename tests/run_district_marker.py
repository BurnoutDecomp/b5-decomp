"""Exercise successive district/county changes through the production marker state machine."""
import argparse
import os
from pathlib import Path

from fxgs_common import Tree, definition, compile_and_run, report


parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--rev", help="negative control: compile marker methods from an older revision")
parser.add_argument("--output-dir", type=Path, help="retain the exact compiled input and result receipts")
args = parser.parse_args()
os.environ.pop("NoDefaultCurrentDirectoryInExePath", None)
source = Tree(args.rev).read("src/GameSource/Gui/Flow/HUD/Components/BrnDistrictMarker.cpp")
tables = [definition(source, "const char* const " + name + "[") + ";"
          for name in ("KAPC_COUNTY_ICON_NAMES", "KAPC_DISTRICT_ICON_NAMES")]
methods = [definition(source, "void DistrictMarkerComponent::" + name + "(")
           for name in ("Construct", "SetCounty", "SetDistrict",
                        "ProcessCountyTransitionComplete", "ProcessDistrictTransitionComplete",
                        "SetHideCountyIcon")]
code = "namespace BrnGui {\n" + "\n".join(tables + methods) + "\n}\n"
result = compile_and_run(Path(__file__).with_name("DistrictMarker.cpp"),
                         "district_marker.inc", code, "DistrictMarker", output_dir=args.output_dir)
raise SystemExit(report("run_district_marker", [], result, 24))
