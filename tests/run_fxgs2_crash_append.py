"""FX-GS2 (crash parity 2026-09-23, G11-D5): GameStateToGuiInterface's race-car crash queue --
Construct's ninth leg, AppendRaceCarCrashes and its online-gated caller.

  Construct @0x82379908 builds the crash queue at +0x1E0 (0x82379964). AppendRaceCarCrashes
  @0x82379980 asserts "lpRaceCarCrashEventQueue" (line 247) and Appends the source queue onto it.
  GameStateModule::PreWorldUpdate @0x823A5544..0x823A55A0: when the current game mode exists and is
  online, store GetPlayerActiveRaceCarIndex() into the interface (the inlined SetPlayerRaceCarIndex)
  and append the cached post-world crash queue (gsm+0x3D1A0) -- before the drive-thru tick.

Numeric: tests/FxGs2CrashAppend.cpp compiles the extracted production Construct /
AppendRaceCarCrashes / SetPlayerRaceCarIndex against the real header and RaceCarCrashEvent.
Wiring: the online-gated block at the head of PreWorldUpdateStuntBringUp (GameStateModule_gUI_00.cpp).

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxgs2_crash_append.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, definition, code_only, compile_and_run, report, STRSTREAM_CPP

GUI_CPP = "src/GameSource/GameState/SharedIO/BrnGameStateToGuiIOInterfaces.cpp"
PUMP_CPP = "src/GameSource/GameState/GameStateModule_gUI_00.cpp"
NUMERIC_CHECKS = 12

REQUIRED = ["void GameStateToGuiInterface::Construct()"]
OPTIONAL = [
    ("void GameStateToGuiInterface::AppendRaceCarCrashes(",
     "void GameStateToGuiInterface::AppendRaceCarCrashes("
     "const CgsModule::EventQueue<BrnPhysics::Vehicle::RaceCarCrashEvent, 8>*) {}"),
    ("void GameStateToGuiInterface::SetPlayerRaceCarIndex(",
     "void GameStateToGuiInterface::SetPlayerRaceCarIndex(s32) {}"),
]


def wiring(tree):
    source = tree.read(PUMP_CPP)
    try:
        pump = re.sub(r"\s+", "", code_only(definition(source, "void GameStateModule::PreWorldUpdateStuntBringUp(")))
    except ValueError:
        pump = ""
    gate = pump.find("lpCurrentGameMode!=0&&lpCurrentGameMode->IsOnline()")
    store = pump.find("GetGameStateToGuiInterface()->SetPlayerRaceCarIndex(liPlayerRaceCarIndex);")
    append = pump.find("GetGameStateToGuiInterface()->AppendRaceCarCrashes(&mpTakedownCache->mRaceCarCrashEventQueue);")
    drive = pump.find("mDriveThruManager.Update(")
    yield ("PreWorldUpdate: an online current mode stores the player's slot, then appends the cached crash "
           "queue (0x823A5548..0x823A55A0), before the drive-thru tick (0x823A56C0)",
           0 <= gate < store < append < drive)
    index = pump.find("liPlayerRaceCarIndex=static_cast<s32>(GetPlayerActiveRaceCarIndex());")
    yield ("the stored index is GetPlayerActiveRaceCarIndex() (0x823A5578 -> `stw r28, 0(r11)`)",
           0 <= gate < index < store)


def numeric(tree):
    source = tree.read(GUI_CPP)
    parts, stood_in = [], []
    for signature in REQUIRED:
        try:
            parts.append(definition(source, signature))
        except ValueError:
            print("NUMERIC: cannot build -- production body absent: " + signature)
            return None
    for signature, stand_in in OPTIONAL:
        try:
            parts.append(definition(source, signature))
        except ValueError:
            parts.append("// [stand-in: body absent in this revision]\n" + stand_in)
            stood_in.append(signature.split("::", 1)[1].strip())
    if stood_in:
        print("NUMERIC: bodies absent in this revision (empty stand-ins): " + ", ".join(stood_in))
    inc = "namespace BrnGameState { namespace GameStateModuleIO {\n" + "\n".join(parts) + "\n} }\n"
    return compile_and_run(Path(__file__).with_name("FxGs2CrashAppend.cpp"), "crash_append_methods.inc", inc,
                           "FxGs2CrashAppend", extra_sources=[STRSTREAM_CPP])


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxgs2_crash_append", list(wiring(tree)), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
