"""Factory-produced traffic dispatch queue lifecycle, including poisoned reuse.

Executes the actual factory template, IO stack, traffic Construct/accessors and
World's crashing Clear/AddEvent fan-out. No graphics device or game is launched.
--rev tests a published baseline; --omit-queue-construction is a negative control.
"""
from pathlib import Path
import argparse
import os
from fxgs_common import Tree, REPO, WORKFLOW, STRSTREAM_CPP, definition, code_only, compile_and_run, report

os.environ.pop("NoDefaultCurrentDirectoryInExePath", None)
os.environ.pop("BRN_IOBUF_ZERO", None)
parser = argparse.ArgumentParser()
parser.add_argument("--rev")
parser.add_argument("--omit-queue-construction", action="store_true")
args = parser.parse_args()
tree = Tree(args.rev)
header_path = "src/GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModuleIO.h"
source_path = "src/GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModuleIO_InputBuffer_Dispatch.cpp"
source = tree.read(source_path)
stubs = tree.read("src/GameSource/World/WorldLinkStubs.cpp")
if "InputBuffer_Dispatch::GetSceneResultQueue()" not in source:
    # The exact published baseline resolves this symbol to the inert body.
    source += "\n" + definition(stubs,
        "class CgsModule::VariableEventQueue<32768,16> * BrnTraffic::BrnTrafficIO::InputBuffer_Dispatch::GetSceneResultQueue()")
if args.omit_queue_construction:
    source = source.replace("mSceneResultQueue.Construct();", "")
world = code_only(definition(Tree().read("src/GameSource/World/BrnWorldModule.cpp"),
                             "WorldModule::GenerateDispatchLists("))
start = world.index("    lpTrafficDispatchInput->LockForWrite();")
end = world.index("    lpTrafficDispatchInput->UnlockForWrite();", start) + len("    lpTrafficDispatchInput->UnlockForWrite();")
fanout = world[start:end]
construct = code_only(definition(source, "void InputBuffer_Dispatch::Construct()"))
home = code_only(source)
factory = code_only(tree.read("src/GameShared/GameClasses/Module/CgsIOBufferStack.h"))
mount = (WORKFLOW / "tools/build/build_game_exe.bat").read_text(encoding="utf-8")
wiring = [
    ("native factory still default-initializes then calls the actual traffic Construct", "new (lpMem) T;" in factory
     and "(*lpOutBuffer)->Construct();" in factory),
    ("original constructor constructs its real scene queue before the four pointer resets",
     "mSceneResultQueue.Construct();" in construct and construct.index("mSceneResultQueue.Construct();") < construct.index("mpDispatchFrame")),
    ("canonical getter is homed beside the constructor and the null WorldLinkStubs body is retired",
     "InputBuffer_Dispatch::GetSceneResultQueue()" in home
     and "return &mSceneResultQueue;" in home
     and "InputBuffer_Dispatch::GetSceneResultQueue()" not in code_only(stubs)),
    ("canonical getter/constructor TU and remaining WorldLinkStubs TU are already shipping-mounted",
     "TrafficEntityModule\\BrnTrafficEntityModuleIO_InputBuffer_Dispatch.cpp" in mount
     and "World\\WorldLinkStubs.cpp" in mount),
]
result = compile_and_run(Path(__file__).with_name("PCTrafficDispatchIo.cpp"),
    "traffic_dispatch_io.inc", source, "PCTrafficDispatchIo", extra_flags="/Gy /Gw",
    shadow={header_path: tree.read(header_path)}, extra_sources=[STRSTREAM_CPP] + [REPO / p for p in (
        "src/GameShared/GameClasses/Module/CgsIOBuffer.cpp", "src/GameShared/GameClasses/Module/CgsIOBufferStack.cpp")],
    extra_files={"traffic_dispatch_fanout.inc": fanout})
raise SystemExit(report("run_pc_traffic_dispatch_io", wiring, result, 1))
