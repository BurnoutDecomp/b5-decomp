"""FX-BRIDGES (crash parity 2026-09-24) CC-9: BridgeControllerToWorld @0x823CD890, the Showtime-intro control override.

Console 0x823CDB80..0x823CDBCC: while GameStateModule::IsInShowtimeIntro() (the 0.5 s window opened when both
bumpers are held) the bridge publishes full throttle and full handbrake (flt_82001C98 == 1.0f), no brake, the
steering GetShowtimeIntroSteering() latched from the car's yaw-rate sign, and zeroed stick / sensor lanes -- the
spin into Showtime. The PC body carried the block only as a comment, so the player kept driving through the intro.
The record is also retyped from the bridge-local WorldVehicleControlsImage to the real BrnWorld::PlayerVehicleControls.

Numeric: tests/FxBridgesShowtimeIntro.cpp compiles the PRODUCTION body (by its definition signature) against the real
pad record and controls type. The pre-fix body compiles too (the fixture carries the retired image) and fails the
override checks.
Wiring: the override calls IsInShowtimeIntro / GetShowtimeIntroSteering on mGameStateModule before the publish; the
retired image is gone from the header.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxbridges_showtime_intro.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, code_only, definition, compile_and_run, report, STRSTREAM_CPP

BRIDGE_CPP = "src/GameSource/Game/GameBridgeControllerToX.cpp"
BRIDGE_H = "src/GameSource/Game/GameBridgeControllerToX.h"
SIGNATURE = "void BrnGameModule::BridgeControllerToWorld("
NUMERIC_CHECKS = 13


def squash(text):
    return re.sub(r"\s+", "", code_only(text))


def body_text(tree):
    try:
        text = definition(tree.read(BRIDGE_CPP), SIGNATURE)
    except ValueError:
        return None
    return text[text.index("{"):] + "\n"


def wiring(tree):
    body = squash(body_text(tree) or "")
    gate = body.find("if(mGameStateModule.IsInShowtimeIntro())")
    steer = body.find("mGameStateModule.GetShowtimeIntroSteering()")
    publish = body.find("lpWorldInput->SetPlayerVehicleControls(")
    yield ("the override is live code: IsInShowtimeIntro gate + GetShowtimeIntroSteering, before the publish "
           "(0x823CDB80 / 0x823CDBA8 < 0x823CDBD8)", 0 <= gate < steer < publish)
    yield ("the published record is the real BrnWorld::PlayerVehicleControls (no reinterpret_cast of a local image)",
           "BrnWorld::PlayerVehicleControlslControls;" in body and "reinterpret_cast" not in body[max(publish, 0):publish + 80])
    header = code_only(tree.read(BRIDGE_H))
    yield ("the bridge-local WorldVehicleControlsImage fork is retired from the header",
           re.search(r"\bstruct\s+WorldVehicleControlsImage\b", header) is None)


def numeric(tree):
    text = body_text(tree)
    if text is None:
        print("NUMERIC: cannot build -- BridgeControllerToWorld has no definition in this revision")
        return None
    return compile_and_run(Path(__file__).with_name("FxBridgesShowtimeIntro.cpp"),
                           "fxbridges_showtime_intro_body.inc", text, "FxBridgesShowtimeIntro",
                           extra_sources=[STRSTREAM_CPP])


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxbridges_showtime_intro", list(wiring(tree)), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
