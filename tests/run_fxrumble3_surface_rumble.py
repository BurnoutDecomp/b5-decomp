"""FX-RUMBLE3 (crash parity 2026-09-24, G10-D6): the road-surface rumble.

BrnGameState::RumbleManager::Update @0x82386A98 opens with UpdateSurfaceRumble @0x82378AE0 (0x82386AB8): each
grounded wheel of the player's car registers its road surface when that surface's rumblesurface carries a
priority >= 0; live surface rumbles are refreshed with a speed-scaled volume or stopped; newly touched surfaces
start one. On PC the call was a [FLAG G10-D6] hole: the body, Attrib::Gen::rumblesurface's 0x3C layout and
accessors, surface::RumbleSurface() (layout +0x28) and the `surfacelist mSurfaceList` member were all missing.

  1. WIRING -- Update's first statement, the DWARF member type, the generated-class accessors.
  2. NUMERIC -- tests/FxRumble3SurfaceRumble.cpp compiles the PRODUCTION UpdateSurfaceRumble + PlayRumble /
     ChangeRumbleVolume / StopRumble (+ Construct / Prepare and the file-scope constants) against the revision's
     own BrnRumbleManager.h / surface.h / rumblesurface.h, with the production GetRaceCarState, the real
     Attrib::StringToKey and a fake Attrib database, and checks the re-bind key, the sanity assert, the wheel
     filter, the slot bookkeeping, the three gates, every event field, the envelope lane map and the volume.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxrumble3_surface_rumble.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import REPO, STRSTREAM_CPP, Tree, body_or_empty, code_only, compile_and_run, definition, report

RUMBLE_H = "src/GameSource/GameState/RumbleManager/BrnRumbleManager.h"
RUMBLE_CPP = "src/GameSource/GameState/RumbleManager/BrnRumbleManager.cpp"
SURFACE_H = "src/GameSource/AttribSys/Generated/classes/surface.h"
RUMBLESURFACE_H = "src/GameSource/AttribSys/Generated/classes/rumblesurface.h"
RCIF_CPP = "src/GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRCEntityActiveRaceCarOutputInterface.cpp"
HASH_CPP = REPO / "src/SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribhash64.cpp"

NUMERIC_CHECKS = 23

BODIES = [
    "    void RumbleManager::Construct()",
    "    bool RumbleManager::Prepare()",
    "    void RumbleManager::UpdateSurfaceRumble(",
    "    void RumbleManager::PlayRumble(",
    "    void RumbleManager::ChangeRumbleVolume(",
    "    void RumbleManager::StopRumble(",
]


def normalised(tree, relative):
    return tree.read(relative).replace("\r\n", "\n")


def wiring(tree):
    cpp = normalised(tree, RUMBLE_CPP)
    update = body_or_empty(cpp, "    void RumbleManager::Update(")
    surface_call = re.search(r"UpdateSurfaceRumble\(\s*lpActiveRaceCarInterface\s*,\s*lePlayerCarIndex\s*\)\s*;", update)
    impacts_call = update.find("UpdateImpacts(")
    yield ("Update's first statement is UpdateSurfaceRumble(itf, playerIndex) (0x82386AB8), before UpdateImpacts (0x82386ACC)",
           surface_call is not None and 0 <= surface_call.start() < impacts_call)

    header = code_only(normalised(tree, RUMBLE_H))
    yield ("RumbleManager holds `Attrib::Gen::surfacelist mSurfaceList` (DWARF BrnRumbleManager.h:108), not opaque bytes",
           re.search(r"Attrib::Gen::surfacelist\s+mSurfaceList\s*;", header) is not None
           and "maSurfaceListStorage" not in header)

    surface = code_only(normalised(tree, SURFACE_H))
    yield ("surface.h names the RumbleSurface ref at layout +0x28 and exposes RumbleSurface() (DWARF surface.h:103)",
           re.search(r"RefSpec\s+mVisualFXSurface\s*;\s*RefSpec\s+mRumbleSurface\s*;\s*RefSpec\s+mPhysicsSurface\s*;", surface)
           is not None and "const RefSpec& RumbleSurface() const" in surface)

    rumble = code_only(normalised(tree, RUMBLESURFACE_H))
    yield ("rumblesurface.h has the RefSpec ctor (0x82364828 -> Instance(const RefSpec&)) and the 0x3C layout accessors",
           "explicit rumblesurface(const RefSpec& lrRefSpec" in rumble and "const s32& RumblePriority() const" in rumble
           and "const f32& MaxSpeedForRumble() const" in rumble)


def numeric(tree):
    cpp = normalised(tree, RUMBLE_CPP)
    missing = [s.strip() for s in BODIES if s not in cpp]
    if missing:
        print("NUMERIC: cannot build -- production bodies absent: " + "; ".join(missing))
        return None
    includes = re.findall(r"^#include [^\n]+", cpp, re.M)
    constants = re.findall(r"^    (?:static )?const (?:f32|s32|u32|u64) K[FNU]_[A-Z0-9_]+\s*=[^;]+;", cpp, re.M)
    rcif = tree.read(RCIF_CPP).replace("\r\n", "\n")
    text = "\n".join(
        includes
        + ["namespace BrnGameState {"] + constants + [definition(cpp, s) for s in BODIES] + ["}"]
        + ["namespace BrnWorld { namespace RaceCarEntityModuleIO {",
           definition(rcif, "const RCEntityActiveRaceCarOutputInterface::RaceCarState*\n"
                            "RCEntityActiveRaceCarOutputInterface::GetRaceCarState(EActiveRaceCarIndex leActiveRaceCarIndex) const"),
           "} }"])
    return compile_and_run(Path(__file__).with_name("FxRumble3SurfaceRumble.cpp"), "fxrumble3_surface_rumble.inc", text,
                           "FxRumble3SurfaceRumble",
                           shadow={RUMBLE_H: tree.read(RUMBLE_H), SURFACE_H: tree.read(SURFACE_H),
                                   RUMBLESURFACE_H: tree.read(RUMBLESURFACE_H)},
                           extra_sources=(HASH_CPP, STRSTREAM_CPP))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxrumble3_surface_rumble", list(wiring(tree)), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
