"""Compile the actual GuiCache car-select case and getter; supports old-source controls."""
import argparse
from pathlib import Path
import re
import sys
from fxgs_common import Tree, compile_and_run, definition

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--rev")
parser.add_argument("--output-dir", type=Path)
args = parser.parse_args()
tree = Tree(args.rev)
source = definition(tree.read("src/GameSource/Gui/BrnGuiCache.cpp"), "void GuiCache::RecEvent(")
match = re.search(r"\n        case (?:77|81):.*?meCarSelectType\s*=.*?;\s*break;", source, re.S)
if not match:
    raise ValueError("No production car-select type handler found")
getter = definition(tree.read("src/GameSource/Gui/BrnGuiCache.h"), "s32 GetCurrentCarSelectType() const")
result = compile_and_run(Path(__file__).with_name("JunkyardCarSelectType.cpp"),
    "junkyard_car_select_type.inc", match[0], "JunkyardCarSelectType",
    extra_files={"junkyard_car_select_getter.inc": getter}, output_dir=args.output_dir)
sys.exit(0 if result and result[1] == 0 else 1)
