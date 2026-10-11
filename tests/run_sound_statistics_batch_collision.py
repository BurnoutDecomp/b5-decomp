"""Grouped Collision menu check using complete production functions and real descriptors."""
import argparse
import os
import re
from pathlib import Path
from fxgs_common import Tree, definition, compile_and_run, report, REPO, WORKFLOW

os.environ.pop("NoDefaultCurrentDirectoryInExePath", None)
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--rev")
parser.add_argument("--compile-only", action="store_true")
args = parser.parse_args()
tree = Tree(args.rev)
source = tree.read("src/GameSource/Sound/Collision/BrnCollisionStateManager.cpp")
header = tree.read("src/GameSource/Sound/Collision/BrnCollisionStateManager.h")
effect = tree.read("src/GameSource/Sound/Collision/BrnCollisionEffect.cpp")
missing = []
parts = []
for text, signature, prefix in (
    (header, "struct CrashBinUtils", "template<typename CrashBin>\n"),
    (source, "f32 Dot3(", ""), (source, "VecFloat Splat(", ""),
    (source, "f32 NormalisedImpulse(", ""),
    (source, "unsigned int CrashBinUtils< CrashBin >::GetSampleIds(", "template<typename CrashBin>\n"),
    (source, "void CollisionStateManager::AddInputCollision(", ""),
    (source, "void CollisionStateManager::UpdateResolver(", ""),
    (source, "void CollisionStateManager::GetRandomSampleID(", "template<typename BinType>\n")):
    try:
        body = definition(text, signature)
        if signature == "struct CrashBinUtils":
            body += ";"
        parts.append(prefix + body)
    except ValueError:
        missing.append(signature)
if missing:
    print("Missing production bodies: " + ", ".join(missing))
    raise SystemExit(1)

# Capture actual owner initializers, including the CRT-only product. These are
# production data definitions; expected outcomes below come from ARTIST stores.
data = []
for line in (source + "\n" + effect).splitlines():
    if re.match(r"(?:volatile )?s32 KI_|f32 KF_(?:FASTEST_COLLISION|BIGGEST_THING_MASS|BIGGEST_COLLISION_IN_SECOND)|const f32 KF_BIGGEST_COLLISION_IN_SECOND|const char\* const KAPC_IMPACT_TIME_NAMES", line):
        data.append(line)
code = "namespace BrnSound { namespace Logic { namespace Collision {\n" + "\n".join(data + parts) + "\n} } }\n"
fixture = Path(__file__).with_name("SoundStatisticsBatchCollision.cpp")
shadow = {path: tree.read(path) for path in (
    "src/GameSource/Sound/Collision/BrnCollisionDataStructures.h",
    "src/GameSource/Sound/Collision/BrnCollisionFrameInformation.h")}
if args.compile_only:
    import sys
    sys.path.insert(0, str(WORKFLOW / "tools/work"))
    from verify import compile_gate
    output = WORKFLOW / "scratch/DEBUG_MENU_1011/sound_batch/statistics/collision/fixture_compile.cpp"
    output.write_text(fixture.read_text(encoding="utf-8").replace(
        '#include "sound_statistics_batch_collision.inc"', code), encoding="utf-8")
    status, diagnostics = compile_gate([str(output)])
    print(status)
    print(diagnostics)
    raise SystemExit(0 if status == "pass" else 1)
numeric = compile_and_run(fixture, "sound_statistics_batch_collision.inc", code,
                         "SoundStatisticsBatchCollision", shadow=shadow,
                         extra_sources=(REPO / "src/GameShared/GameClasses/Development/CgsStrStream.cpp",
                                        REPO / "src/GameShared/GameClasses/Numeric/CgsRandom.cpp",
                                        REPO / "src/GameShared/GameClasses/Sound/CgsSoundUtils.cpp"))
raise SystemExit(report("run_sound_statistics_batch_collision", [], numeric, 1))
