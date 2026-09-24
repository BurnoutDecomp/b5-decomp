"""FX-DIRECTOR (crash parity 2026-09-24, conductor item C1): the drowning fade.

  Arbitrator::Update @0x8226ADA0 (NORMAL state) @0x8226AF90..0x8226B008 keys the "BlackFade_Water"
  camera effect on the published Camera::PlayerCrashInfo::mbHitWater (+0x27): set -> clear the
  camera's bit 1, request the start hook at blend 1.0 with the stop hook and post-FX id cleared, and
  re-arm the external gameplay behaviour (ForcePrimaryGameplayBehaviourToFinish); clear ->
  EnsureEffectIsStopped(camera, effects, "BlackFade_Water"). The PC gated BOTH arms off ("DELETE-WHEN
  PlayerCrashInfo is homed"); the record and its producer landed with FX-BRIDGES CC-6 (9aea778c).

Numeric: tests/FxDirectorWaterFade.cpp compiles the extracted production if/else against the real
CameraEffects and PlayerCrashInfo with recording stand-ins for the two calls; a revision without the
branch gets a labelled empty stand-in (and fails).
Wiring: the branch sits where the console has it -- after the frame camera is taken from the
current state and before the attract-mode trigger.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxdirector_water_fade.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, definition, code_only, compile_and_run, report

ARBITRATOR_CPP = "src/GameSource/Director/Arbitrator/BrnDirectorArbitrator.cpp"
UPDATE = "void Arbitrator::Update(bool lbPaused, Camera::Camera& lrCameraInOut,"
BRANCH = "if (lrSharedInfo.mpPlayerCrashInfo->mbHitWater)"
NUMERIC_CHECKS = 12


def block(text, start):
    """The brace block that opens at or after `start` in comment-free text: (end index, block text)."""
    brace = text.find("{", start)
    depth = 0
    for index in range(brace, len(text)):
        if text[index] == "{":
            depth += 1
        elif text[index] == "}":
            depth -= 1
            if depth == 0:
                return index + 1, text[brace:index + 1]
    raise ValueError("unterminated block")


def branch(tree):
    """The production if/else text, or None when this revision has no such branch."""
    try:
        update = code_only(definition(tree.read(ARBITRATOR_CPP), UPDATE))
    except ValueError:
        return None
    start = update.find(BRANCH)
    if start < 0:
        return None
    end, if_block = block(update, start)
    rest = update[end:]
    match = re.match(r"\s*else\s*", rest)
    if match is None:
        return None
    _, else_block = block(rest, match.end())
    return BRANCH + "\n" + if_block + "\nelse\n" + else_block


def wiring(tree):
    try:
        update = re.sub(r"\s+", "", code_only(definition(tree.read(ARBITRATOR_CPP), UPDATE)))
    except ValueError:
        update = ""
    camera = update.find("lrCameraInOut=GetNormalCamera();")
    water = update.find("if(lrSharedInfo.mpPlayerCrashInfo->mbHitWater)")
    attract = update.find("if(mbDoAttractMode)")
    yield ("the water branch runs in the NORMAL state after the frame camera is taken (0x8226AF8C) and "
           "before the attract-mode trigger (0x8226B08C)",
           0 <= camera < water < attract)


def numeric(tree):
    text = branch(tree)
    if text is None:
        print("NUMERIC: Arbitrator::Update has no BlackFade_Water branch in this revision (empty stand-in)")
        text = "{ /* [stand-in: no BlackFade_Water branch in this revision] */ }"
    inc = ("namespace BrnDirector\n{\n"
           "void ArbitratorStandIn::Branch(Camera::Camera& lrCameraInOut, SharedInfoStandIn& lrSharedInfo)\n{\n"
           + text + "\n}\n}\n")
    return compile_and_run(Path(__file__).with_name("FxDirectorWaterFade.cpp"), "water_fade.inc", inc,
                           "FxDirectorWaterFade")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxdirector_water_fade", list(wiring(tree)), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
