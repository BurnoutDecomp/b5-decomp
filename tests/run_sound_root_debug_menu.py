"""Check original Sound memory-action ordering through complete production bodies."""
import argparse
import os
import re
from pathlib import Path
from fxgs_common import Tree, definition, compile_and_run, report

os.environ.pop("NoDefaultCurrentDirectoryInExePath", None)
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--rev")
args = parser.parse_args()
source = Tree(args.rev).read("src/GameSource/Sound/Module/BrnRootSoundModule.cpp")
missing = []
bodies = {}
for signature in ("void DebugAudioMemoryDump()", "void RootSoundModule::Update("):
    try:
        bodies[signature] = definition(source, signature)
    except ValueError:
        missing.append(signature)
globals_ = []
for line in source.splitlines():
    if re.match(r"s32 KI_DEBUG_PRINT_AUDIO_ALLOCATIONS\s*=", line):
        globals_.append(line)
    elif re.match(r"CgsSound::TestBed::Allocator g(?:Rwac|Csis|Playback|Logic)TestBedAlloc\(", line):
        globals_.append(line)
if len(globals_) != 5:
    missing.append("five live shared allocator/action definitions")
if missing:
    print("Missing production definitions: " + ", ".join(missing))
    raise SystemExit(1)
code = "\n".join(globals_) + "\n" + bodies["void DebugAudioMemoryDump()"]
code += "\nnamespace BrnSound { namespace Module {\n" + bodies["void RootSoundModule::Update("] + "\n} }\n"
numeric = compile_and_run(Path(__file__).with_name("SoundRootDebugMenu.cpp"),
                          "sound_root_debug_menu.inc", code, "SoundRootDebugMenu")
raise SystemExit(report("run_sound_root_debug_menu", [], numeric, 10))
