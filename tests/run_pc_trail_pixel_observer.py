"""Actual trail observer helpers and unchanged original pass/cadence boundary.

This CPU gate does not claim native images or GPU coverage; the live case does.
"""
import argparse
import os
from pathlib import Path
from fxgs_common import Tree, compile_and_run, report

os.environ.pop("NoDefaultCurrentDirectoryInExePath", None)
parser = argparse.ArgumentParser()
parser.add_argument("--slot-identity-control", action="store_true",
    help="negative control: reused slots incorrectly count as the original strip")
args = parser.parse_args()
tree = Tree(None)
path = "src/pc/gcm/renderengine/TrailPixelDiagPC.h"
header = tree.read(path)
if args.slot_identity_control:
    begin = header.index("if (kind == type && pa.x")
    end = header.index("{ ++matches", begin)
    header = header[:begin] + "if (kind == type)\n                        " + header[end:]
result = compile_and_run(Path(__file__).with_name("PCTrailPixelObserver.cpp"),
    "trail_pixels.inc", "", "PCTrailPixelObserver", shadow={path: header})
raise SystemExit(report("run_pc_trail_pixel_observer", [], result, 1))
