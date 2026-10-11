"""Run one grouped numeric fixture for MusicStream and speed-stream debug tuning."""
import argparse
import os
import re
from pathlib import Path

from fxgs_common import Tree, definition, compile_and_run, report

os.environ.pop("NoDefaultCurrentDirectoryInExePath", None)
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--rev")
args = parser.parse_args()
tree = Tree(args.rev)

music_stream = tree.read("src/GameSource/Sound/Global/BrnMusicStream.cpp")
music_effect = tree.read("src/GameSource/Sound/Global/BrnMusicEffect.cpp")
music_header = tree.read("src/GameSource/Sound/Global/BrnMusicEffect.h")
speed_control = tree.read("src/GameSource/Sound/Vehicles/Environment/BrnSpeedStreamControl.cpp")
speed_effect = tree.read("src/GameSource/Sound/Vehicles/Environment/BrnSpeedStreamEffect.cpp")
speed_header = tree.read("src/GameSource/Sound/Vehicles/Environment/BrnSpeedStreamControl.h")

music_filters = {
    "KF_MUSIC_HIGH_PASS_ORDER": "4.0f",
    "KF_MUSIC_HIGH_PASS_Q": "0.95f",
    "KF_MUSIC_LOW_PASS_ORDER": "4.0f",
    "KF_MUSIC_LOW_PASS_Q": "0.99f",
}
speed_tuning = {
    "KF_SPEED_STREAM_THRESHOLD_MPH": "105.0f",
    "KF_SPEED_STREAM_CROSSFADE_WIDTH": "40.0f",
}

globals_by_namespace = []
missing_globals = []
bad_defaults = []
for source, values, namespace in (
        (music_effect, music_filters, "BrnSound::Logic"),
        (speed_effect, speed_tuning, "BrnSound::Vehicles::Environment")):
    definitions = []
    for name, expected in values.items():
        match = re.search(rf"^\s*f32\s+{name}\s*=\s*([^;]+);", source, re.MULTILINE)
        if not match:
            missing_globals.append(name)
            continue
        initializer = match.group(1).strip()
        if initializer != expected:
            bad_defaults.append(f"{name} expected {expected}, found {initializer}")
        definitions.append(f"f32 {name} = {initializer};")
    globals_by_namespace.append((namespace, definitions))

missing_declarations = []
for header, values in ((music_header, music_filters), (speed_header, speed_tuning)):
    for name in values:
        if not re.search(rf"\bextern\s+f32\s+{name}\s*;", header):
            missing_declarations.append(name)

missing_bodies = []
try:
    music_body = definition(music_stream, "void MusicStream::UpdateVoiceParams(")
except ValueError:
    missing_bodies.append("MusicStream::UpdateVoiceParams")
    music_body = ""

speed_bodies = []
for signature in ("void SpeedStreamControl::UpdateParams(",
                  "void SpeedStreamControl::UpdateDuckers()"):
    try:
        speed_bodies.append(definition(speed_control, signature))
    except ValueError:
        missing_bodies.append(signature)

code = ""
for namespace, definitions in globals_by_namespace:
    parts = namespace.split("::")
    code += "namespace " + " { namespace ".join(parts) + " {\n"
    code += "\n".join(definitions) + "\n"
    code += "} }\n" if len(parts) == 2 else "} } }\n"
code += "namespace BrnSound { namespace Logic {\n" + music_body + "\n} }\n"
code += "namespace BrnSound { namespace Vehicles { namespace Environment {\n"
code += "\n".join(speed_bodies) + "\n} } }\n"

wiring = [
    ("current production MusicStream::UpdateVoiceParams body found", not missing_bodies or
     "MusicStream::UpdateVoiceParams" not in missing_bodies),
    ("both current production speed-stream consumer bodies found",
     not any(name.startswith("void SpeedStreamControl::") for name in missing_bodies)),
    ("all six tuning cells have production definitions", not missing_globals),
    ("all six tuning cells have production header declarations", not missing_declarations),
    ("production tuning-cell defaults match ARTIST values", not bad_defaults),
]

problems = []
if missing_bodies:
    problems.append("Missing production bodies: " + ", ".join(missing_bodies))
if missing_globals:
    problems.append("Missing production tuning cells: " + ", ".join(missing_globals))
if missing_declarations:
    problems.append("Missing header declarations: " + ", ".join(missing_declarations))
if bad_defaults:
    problems.append("Unexpected production defaults: " + "; ".join(bad_defaults))

if problems:
    for problem in problems:
        print(problem)
    numeric = None
else:
    numeric = compile_and_run(Path(__file__).with_name("SoundStreamDebugMenu.cpp"),
                              "sound_stream_debug_menu.inc", code,
                              "SoundStreamDebugMenu")

raise SystemExit(report("run_sound_stream_debug_menu", wiring, numeric, 22))
