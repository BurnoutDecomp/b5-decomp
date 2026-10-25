"""ARTIST 8239F144..8239F24C: discover nearby services during freeburn.

Execute the complete production proximity-discovery branch with recording
profile/GUI collaborators. --rev supplies the pre-fix negative control.
"""
import argparse
from pathlib import Path
import re

from fxgs_common import Tree, compile_and_run, definition, report

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--rev")
args = parser.parse_args()
source = Tree(args.rev).read(
    "src/GameSource/GameState/Offences/BrnDriveThruManager.cpp")
update = definition(source, "void DriveThruManager::Update(")
start = update.index("// Proximity discovery")
branch = definition(update[start:], "if (")
constants = "\n".join(re.findall(
    r"^static const f32 KF_DRIVE_THRU_DISCOVERY_\w+\s*=[^;]*;", source, re.M))
code = constants + "\nvoid Fixture::Step() {\n" + branch + "\n}\n"
result = compile_and_run(Path(__file__).with_name("DriveThruDiscovery.cpp"),
    "drive_thru_discovery.inc", code, "DriveThruDiscovery")
raise SystemExit(report("run_drive_thru_discovery", [], result, 23))
