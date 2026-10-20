"""CPU checks for ARTIST GUI model IO lifecycle with poisoned storage and reuse.

--base-only restores the former inherited base-only Construct/Destruct behavior.
--retain-view-events omits only the original view queue Clear operation.
"""
import argparse
import os
from pathlib import Path

from fxgs_common import Tree, definition, compile_and_run, STRSTREAM_CPP

os.environ.pop("NoDefaultCurrentDirectoryInExePath", None)
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--base-only", action="store_true")
parser.add_argument("--retain-view-events", action="store_true")
parser.add_argument("--output-dir", type=Path)
args = parser.parse_args()
tree = Tree()
base = tree.read("src/GameShared/GameClasses/Module/CgsIOBuffer.cpp")
source_input = tree.read("src/GameShared/GameClasses/Gui/Model/CgsModelModuleIO_InputBuffer.cpp")
source_output = tree.read("src/GameShared/GameClasses/Gui/Model/CgsModelModuleIO_OutputBuffer.cpp")
# This fixture covers lifecycle/accessors. The two separate input publishers
# require their own lock/payload fixture; keep their real bodies out of this TU.
for signature in ("void InputBuffer::AddGuiEvents(const GuiEventQueue*",
                  "void InputBuffer::AddGuiEvents(const GuiEventInputQueue*"):
    source_input = source_input.replace(definition(source_input, signature), "", 1)
if args.base_only:
    for name, text in (("InputBuffer", source_input), ("OutputBuffer", source_output)):
        for method in ("Construct", "Destruct"):
            signature = f"void {name}::{method}()"
            text = text.replace(definition(text, signature),
                f"{signature} {{ CgsModule::IOBuffer::{method}(); }}", 1)
        if name == "InputBuffer":
            source_input = text
        else:
            source_output = text
if args.retain_view_events:
    original = definition(source_output, "void OutputBuffer::Clear()")
    clear = "mViewOutEvents.CgsModule::VariableEventQueue<65536, 16>::Clear();"
    assert original.count(clear) == 1
    mutated = original.replace(clear, "", 1)
    assert mutated != original
    source_output = source_output.replace(original, mutated, 1)
bodies = base + "\n" + source_input + "\n" + source_output
result = compile_and_run(Path(__file__).with_name("GuiModelIO.cpp"),
    "gui_model_io.inc", bodies, "GuiModelIO", extra_sources=[STRSTREAM_CPP],
    output_dir=args.output_dir)
if result is None:
    raise SystemExit(1)
checks, failures = result
print(f"run_gui_model_io: {checks - failures}/{checks} pass ({failures} fail)")
raise SystemExit(bool(failures))
