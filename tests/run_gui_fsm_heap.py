"""Issue 68: real Lua/HeapMalloc, authored FSMs and repeated HUD reloads.

Requires converted FSM bundles and vendor/lua/lua515.lib. Example:
python b5-decomp/tests/run_gui_fsm_heap.py --game-data build/game [--rev REV]
"""
import argparse
import json
import os
from pathlib import Path
import re
import struct

from fxgs_common import Tree, REPO, WORKFLOW, compile_and_run, definition, report

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--rev", help="negative control: use an earlier backing budget")
parser.add_argument("--game-data", type=Path, default=WORKFLOW / "build/game")
args = parser.parse_args()
os.environ.pop("NoDefaultCurrentDirectoryInExePath", None)
tree = Tree(args.rev)
module = tree.read("src/GameSource/Gui/BrnGuiModule.cpp")
capacity = re.search(r"^\s*const u32 KU_FSM_LUA_HEAP_BYTES\s*=[^;]+;", module, re.M)
backing = re.search(r"^\s*u8 s_fsmLuaHeapBuffer\[[^;]+;", module, re.M)
code = (capacity[0] if capacity else "") + "\n" + backing[0] + "\n"
lua = tree.read("src/GameShared/GameClasses/Fsm/CgsLuaState.cpp")
code += "namespace CgsFsm {\n" + "\n".join(definition(lua, signature) for signature in [
    "void LuaState::Construct(", "void* LuaState::LuaAllocator(",
    "bool LuaState::Prepare(", "void LuaState::Release("]) + "\n}\n"
for name, bundle in [("screen", "BRNSCREENFSM"), ("overlay", "BRNOVERLAY"),
                     ("freeburn", "BRNFBFSM"), ("event", "BRNEVENTFSM")]:
    path = args.game_data / "FSM" / (bundle + ".BUNDLE")
    data = path.read_bytes()
    magic, version, platform, _, count, entries, main, _, _, flags = struct.unpack_from("<4s9I", data)
    if magic != b"bnd2" or version != 2 or platform != 4 or count != 1 or flags & 1:
        raise ValueError(f"Expected an uncompressed single-entry PC FSM bundle: {path}")
    resource_type = struct.unpack_from("<I", data, entries + 56)[0]
    offset = struct.unpack_from("<I", data, entries + 40)[0]
    if resource_type != 34:
        raise ValueError(f"Expected LuaCode resource type 34: {path}")
    base = main + offset
    size, start = struct.unpack_from("<II", data, base)
    source = data[base + start:base + start + size].decode("utf-8")
    code += f"const char* {name}_source = " + "\n".join(
        json.dumps(source[i:i + 8000]) for i in range(0, len(source), 8000)) + ";\n"
result = compile_and_run(Path(__file__).with_name("GuiFsmHeap.cpp"),
    "gui_fsm_heap.inc", code, "GuiFsmHeap", extra_sources=[
        REPO / "src/GameShared/GameClasses/Memory/CgsHeapMalloc.cpp",
        REPO / "vendor/PPMalloc/src/EAGeneralAllocator.cpp",
        REPO / "vendor/coreallocator/source/icoreallocator_interface.cpp",
        REPO / "vendor/lua/lua515.lib"])
raise SystemExit(report("run_gui_fsm_heap", [], result, 21))
