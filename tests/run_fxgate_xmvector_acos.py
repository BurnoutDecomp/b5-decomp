"""FX-GATE item 6 (crash parity 2026-09-25): XboxMath::XMVectorACos is the console's XMVectorACos (X360 0x821F0980).

The console's inverse cosine is the XDK math library's polynomial + one-Newton-step rsqrt pipeline, not a correctly
rounded acos: XMVectorACos(1) = 0x35000000, XMVectorACos(-1) = 0x40490FD9, and every |x| > 1 is NaN. The shared
header src/SDKs/XboxMath/XMVectorACos.h writes it operation for operation, with the coefficients read from the image
(0x82000C30 / 0x82000C40 / 0x82000C50 / 0x82000C60, 0x82001C40).

  1. WIRING -- every game call site the console routes through XMVectorACos calls XboxMath::XMVectorACos and no
     longer std::acos / acosf (the SITES table below: file, the call's console function and `bl` address).
  2. NUMERIC -- tests/FxGateXMVectorACos.cpp checks the header against the console's own words run on emu64
     (FxGateXMVectorACosData.h, 3324 rows), the landmarks, NaN for |x| > 1, and that std::acos is not the console.
     It also runs PreRaceFlyByState::FindEventDirection's clamp, extracted from the production source: vmaxfp
     0x824B501C / vminfp 0x824B5020 keep a NaN (the ternaries 60c6c301 wrote turned it into -1.0).

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxgate_xmvector_acos.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, code_only, compile_and_run, definition, report

HEADER = "src/SDKs/XboxMath/XMVectorACos.h"
NUMERIC_CHECKS = 3357
FLYBY = "src/GameSource/Gui/Flow/PreEvent/States/BrnPreRaceFlyBy.cpp"
FLYBY_START = "f32 lfCosAngle = Dot3(lv3ReferenceDirection, lv3LandmarkDirection);"
FLYBY_END = "f32 lfAngle = XboxMath::XMVectorACos(lfCosAngle);"
FLYBY_INDEX = "inline u32 ConsoleWordIndex(s32 liIndex)"
FLYBY_TABLE = "const char* const KAPC_COMPASS_POINT_STRINGIDS[E_COMPASS_POINTS_COUNT] ="

# (file, the console function and its `bl XMVectorACos`, the PC expression that must be the call)
SITES = [
    ("src/GameSource/World/AI/BrnAIUtils_Angles.cpp",
     "FindUnsignedAngleBetween2DVectors 0x82766B84", "XboxMath::XMVectorACos(lfDot)"),
    ("src/GameSource/GameState/ModeManager/GameModes/BrnOnlineStuntRunMode.cpp",
     "OnlineStuntRunMode::GetBestStartGridID 0x823319AC", "XboxMath::XMVectorACos(lfCosAngle)"),
    ("src/GameShared/GameClasses/Sound/Logic/Cgs3dEffectControl.cpp",
     "Cgs3dEffectControl::GetPanningAngle 0x826DC068", "XboxMath::XMVectorACos(lfDot)"),
    ("src/GameSource/Sound/Vehicles/Engines/BrnPhysicsControl.cpp",
     "PhysicsControl::UpdateParams 0x826CBC44", "XboxMath::XMVectorACos(lfDot)"),
    ("src/GameSource/Gui/Flow/HUD/Components/BrnCompassComponent.cpp",
     "CompassComponent::ShowPositionOnCompass 0x8241FCE4", "XboxMath::XMVectorACos(lfDot)"),
    ("src/GameSource/Gui/Flow/PreEvent/States/BrnPreRaceFlyBy.cpp",
     "PreRaceFlyByState::FindEventDirection 0x824B5024", "XboxMath::XMVectorACos(lfCosAngle)"),
    ("src/GameSource/Gui/SatNav/BrnMapIconManager.cpp",
     "MapIconManager::GetSatNavIconStateForRival 0x824FA79C", "XboxMath::XMVectorACos(lfDot)"),
    ("src/GameSource/Game/GameBridgeWorldToGui.cpp",
     "BrnGameModule::BridgeWorldVehicleDataToGui 0x823E60E0", "XboxMath::XMVectorACos(lfDot)"),
    ("vendor/renderware/include/rw/math/vpu/matrix44affine_operation.h",
     "rw::math::vpu::SLerp 0x82216910", "XboxMath::XMVectorACos(lfCos)"),
]


def wiring(tree):
    for relative, console, call in SITES:
        text = tree.read(relative)
        code = code_only(text)
        squashed = re.sub(r"\s+", "", code)
        ok = (re.sub(r"\s+", "", call) in squashed and "std::acos(" not in code
              and not re.search(r"(?<![\w:])acosf?\s*\(", code))
        yield (f"{Path(relative).name}: {console} -> {call}, no std::acos / acosf left", ok)


def numeric(tree):
    header = tree.read(HEADER)
    if not header:
        print("NUMERIC: cannot build -- " + HEADER + " is missing at this revision")
        return None
    return compile_and_run(Path(__file__).with_name("FxGateXMVectorACos.cpp"), "fxgate_acos_flyby.inc",
                           flyby_clamp(tree), "FxGateXMVectorACos", shadow={HEADER: header})


def flyby_clamp(tree):
    """The statements between FindEventDirection's Dot3 and its XMVectorACos call, as a function of lfCosAngle (a
    revision without them gets a clamp that returns 0, which fails the checks), then the description setters'
    ConsoleWordIndex (a revision without it indexes with the raw direction) and the KAPC_COMPASS_POINT_STRINGIDS
    table."""
    source = tree.read(FLYBY)
    start, end = source.find(FLYBY_START), source.find(FLYBY_END)
    body = source[start + len(FLYBY_START):end] if 0 <= start < end else "lfCosAngle = 0.0f;"
    try:
        index = definition(source, FLYBY_INDEX)
    except ValueError:
        index = FLYBY_INDEX + "\n{\n    return static_cast<u32>(liIndex);\n}"
    table_start = source.find(FLYBY_TABLE)
    table = source[table_start:source.index("};", table_start) + 2] if table_start >= 0 else ""
    return ("inline float FxGateFlyByClamp(float lfCosAngle)\n{\n" + body + "\n    return lfCosAngle;\n}\n"
            + index + "\n" + table + "\n")


def wiring_flyby(tree):
    code = re.sub(r"\s+", "", code_only(tree.read(FLYBY)))
    yield ("BrnPreRaceFlyBy.cpp: the three description setters index KAPC_COMPASS_POINT_STRINGIDS through "
           "ConsoleWordIndex (slwi r10, rDir, 2 @0x824C6EE8 / 0x824C74C8 / 0x824C7738)",
           code.count("KAPC_COMPASS_POINT_STRINGIDS[ConsoleWordIndex(leEventDirection)]") == 3)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxgate_xmvector_acos", list(wiring(tree)) + list(wiring_flyby(tree)), numeric(tree),
                  NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
