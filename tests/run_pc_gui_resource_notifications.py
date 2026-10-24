"""Replay the issue 52/53 notification burst through production code and native queues."""
import argparse
import os
from pathlib import Path

from fxgs_common import Tree, definition, compile_and_run, report, REPO, STRSTREAM_CPP

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--rev", help="negative control: use the producer and bridge from an older revision")
args = parser.parse_args()
os.environ.pop("NoDefaultCurrentDirectoryInExePath", None)
tree = Tree(args.rev)
gui = "src/GameShared/GameClasses/Gui/"
pc = tree.read(gui + "Model/Resources/CgsGuiResourceModulePC.cpp")
emit = definition(pc, "u32 EmitAptDataNotifications(")
call = "EmitAptDataNotifications(lpPool, lpacPath, liType, lpOutput)"
if "lpacBundlePath" not in emit:
    call = "EmitAptDataNotifications(lpPool, liType, lpOutput)"
sources = [tree.read(path) for path in [
    "src/GameShared/GameClasses/Module/CgsIOBuffer.cpp",
    gui + "Model/CgsModelModuleIO_InputBuffer.cpp",
    gui + "Model/CgsModelModuleIO_OutputBuffer.cpp",
    gui + "Model/Resources/CgsGuiResourceModuleIO_InputBuffer.cpp",
    gui + "Model/Resources/CgsGuiResourceModuleIO_OutputBuffer.cpp",
]]
# The fixture has no general model-event publisher; retain the real lifecycle,
# resource queues and locks without linking unused generic event publishers.
for signature in ("void InputBuffer::AddGuiEvents(const GuiEventQueue*",
                  "void InputBuffer::AddGuiEvents(const GuiEventInputQueue*"):
    sources[1] = sources[1].replace(definition(sources[1], signature), "", 1)
code = "\n".join(sources)
code += "\nnamespace CgsGui { const u32 KU_APTDATA_RESOURCE_TYPE_ID = 30;\n" + emit
code += "\nu32 EmitForFixture(CgsResource::Pool* lpPool, const char* lpacPath, s32 liType, " \
        "GuiResourceModuleIO::OutputBuffer* lpOutput) { return " + call + "; }\n}\n"
code += "namespace BrnGui {\n" + definition(
    tree.read("src/GameSource/Gui/BrnGuiModule.cpp"), "void GuiModule::DispatchGuiResourceModule()") + "\n}\n"
result = compile_and_run(Path(__file__).with_name("PCGuiResourceNotifications.cpp"),
    "pc_gui_resource_notifications.inc", code, "PCGuiResourceNotifications",
    extra_sources=[STRSTREAM_CPP,
        REPO / "src/GameShared/GameClasses/Gui/CgsGuiEventQueue.cpp",
        REPO / "src/GameShared/GameClasses/System/Resource/CgsResourceTypeBase.cpp",
        REPO / "vendor/renderware/src/rw/BaseResourceDescriptor.cpp"])
raise SystemExit(report("run_pc_gui_resource_notifications", [], result, 8))
