"""CPU checks for original event-interpreter IO lifecycle and ARTIST queue capacity.

Controls change only temporary tested code or header copies. --old-capacity
restores 16384 bytes and suppresses only the fixture's compile-time layout pins,
so actual queue behavior is measured. --clear-input-on-destruct adds the queue
destruction that the original input teardown deliberately omits.
"""
import argparse
import os
from pathlib import Path

from fxgs_common import Tree, definition, compile_and_run, STRSTREAM_CPP

os.environ.pop("NoDefaultCurrentDirectoryInExePath", None)
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--old-capacity", action="store_true")
parser.add_argument("--clear-input-on-destruct", action="store_true")
args = parser.parse_args()
tree = Tree()
base = tree.read("src/GameShared/GameClasses/Module/CgsIOBuffer.cpp")
source_input = tree.read("src/GameShared/GameClasses/Gui/Model/CgsEventInterpreterModuleIO.cpp")
source_output = tree.read("src/GameShared/GameClasses/Gui/Model/CgsEventInterpreterModuleIO_OutputBuffer.cpp")
shadow = {}
if args.old_capacity:
    header = "src/GameShared/GameClasses/Gui/Model/CgsEventInterpreterModuleIO.h"
    original = tree.read(header)
    mutated = original.replace("VariableEventQueue<18432, 16> GuiEventQueue;",
                               "VariableEventQueue<16384, 16> GuiEventQueue;", 1)
    assert mutated != original
    shadow[header] = mutated
    source_output = source_output.replace(definition(source_output, "void OutputBuffer::_AssertLayout()"),
                                          "void OutputBuffer::_AssertLayout() {}", 1)
if args.clear_input_on_destruct:
    original = definition(source_input, "void InputBuffer::Destruct()")
    mutated = original.replace("CgsModule::IOBuffer::Destruct();",
                               "mGuiEvents.Destruct();\n        CgsModule::IOBuffer::Destruct();", 1)
    assert mutated != original
    source_input = source_input.replace(original, mutated, 1)
result = compile_and_run(Path(__file__).with_name("GuiInterpreterIO.cpp"),
    "gui_interpreter_io.inc", base + "\n" + source_input + "\n" + source_output,
    "GuiInterpreterIO", shadow=shadow, extra_sources=[STRSTREAM_CPP])
if result is None:
    raise SystemExit(1)
checks, failures = result
print(f"run_gui_interpreter_io: {checks - failures}/{checks} pass ({failures} fail)")
raise SystemExit(bool(failures))
