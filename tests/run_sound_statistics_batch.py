"""One coherent CPU check of recovered Sound Statistics production bodies."""
import argparse
import os
from pathlib import Path
from fxgs_common import Tree, definition, compile_and_run, report

os.environ.pop("NoDefaultCurrentDirectoryInExePath", None)
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--rev")
args = parser.parse_args()
tree = Tree(args.rev)
source = tree.read("src/GameSource/Sound/Debug/BrnSoundDebugStatistics.cpp")
signatures = ("Statistics::Statistics()", "void Statistics::Construct(",
              "void Statistics::Update()", "void Statistics::RenderLine(",
              "void Statistics::DrawGraph(", "void Statistics::RenderHUD(")
bodies, missing = [], []
for signature in signatures:
    try:
        bodies.append(definition(source, signature))
    except ValueError:
        missing.append(signature)
if missing:
    print("Missing production bodies: " + ", ".join(missing))
    raise SystemExit(1)
code = "namespace BrnSound { namespace Debug {\n" + "\n".join(bodies) + "\n} }\n"
shadow = {path: tree.read(path) for path in (
    "src/GameSource/Sound/Debug/BrnSoundDebugStatistics.h",
    "src/GameShared/GameClasses/Sound/Playback/CgsVoiceProfilingData.h")}
numeric = compile_and_run(Path(__file__).with_name("SoundStatisticsBatch.cpp"),
                          "sound_statistics_batch.inc", code, "SoundStatisticsBatch", shadow=shadow)
raise SystemExit(report("run_sound_statistics_batch", [], numeric, 1))
