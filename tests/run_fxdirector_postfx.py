"""FX-DIRECTOR (crash parity 2026-09-24): MainDirector::Update's post-FX id bookkeeping (lane item 3).

  An ICE camera publishes its take's authored post-FX hook id (KeyAnimController -> mEffects.muRequestedPostFxId)
  every frame it plays, and BridgeDirectorToGui turns any non-zero id into GUI event 495 (StartHook by GUID), which
  re-initialises the effect blender. The console's MainDirector::Update tail (0x82275084, 0x82275094..0x822750E4)
  makes that an edge: the id is published only when it changed (or on a camera's first frame) and is then registered
  on the director's EffectInterface (the inlined RegisterStartingEffectWithId) -- the state Camera::StopCurrentEffect
  reads to request the null effect later. The PC tail had neither half, so a take's post-FX restarted every frame.

Numeric: tests/FxDirectorPostFxBookkeeping.cpp runs the production block (from the mLastCamera id read through the
registration) on the real CameraState and EffectInterface. A revision without it runs the pre-fix tail (the flag roll
and the copy), which fails the edge / registration checks. Wiring: the old id is read before the copy, the drop and
the registration follow it; RegisterStartingEffectWithId has its three stores.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxdirector_postfx.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, code_only, definition, compile_and_run, report, STRSTREAM_CPP

DIRECTOR_CPP = "src/GameSource/Director/BrnMainDirector.cpp"
TRIGGER_H = "src/GameSource/Director/Utils/BrnDirectorEffectTrigger.h"
START = "const u32 luLastRequestedPostFxId"
ELSE_IF = "else if (luRequestedPostFxId != 0)"
PRE_FIX_TAIL = ("lCamera.GetState().CopyFlagsToPrevious(mLastCamera.GetState());\n"
                "mLastCamera = lCamera;\n")
NUMERIC_CHECKS = 10


def squash(text):
    return re.sub(r"\s+", "", code_only(text))


def block(tree):
    """From the old-id read through the end of the registration arm (brace-balanced: the arm carries a
    nested diag block)."""
    source = tree.read(DIRECTOR_CPP)
    start = source.find(START)
    if start < 0:
        return None
    arm = source.find(ELSE_IF, start)
    if arm < 0:
        return None
    try:
        return source[start:arm] + definition(source[arm:], ELSE_IF)
    except ValueError:
        return None


def wiring(tree):
    text = squash(block(tree) or "")
    old = text.find("constu32luLastRequestedPostFxId=mLastCamera.GetEffects().muRequestedPostFxId;")
    copy = text.find("mLastCamera=lCamera;")
    new = text.find("constu32luRequestedPostFxId=lCamera.GetEffects().muRequestedPostFxId;")
    drop = text.find("lCamera.GetEffects().muRequestedPostFxId=0;")
    register = text.find("->RegisterStartingEffectWithId(luRequestedPostFxId);")
    yield ("the tail reads the old id BEFORE mLastCamera = lCamera, then drops an unchanged id and registers a changed "
           "one (0x82275084 < 0x82275090 < 0x822750C4 / 0x822750D4)",
           0 <= old < copy < new < drop < register
           and "!lCamera.GetState().IsFlagSet(Camera::CameraState::E_FLAG_NEW_THIS_FRAME)" in text)
    header = squash(tree.read(TRIGGER_H))
    yield ("EffectInterface::RegisterStartingEffectWithId (DWARF :119) is the three inlined stores "
           "(+0xD37 = 0, +0xD39 = 1, +0xCE8 = id)",
           "voidRegisterStartingEffectWithId(u32luEffectId){mbHasCurrentEffectName=false;mbHasCurrentEffectId=true;"
           "muRequestedPostFxId=luEffectId;}" in header)


def numeric(tree):
    text = block(tree)
    if text is None:
        print("NUMERIC: this revision's MainDirector::Update has no post-FX bookkeeping -- running the pre-fix tail")
        text = PRE_FIX_TAIL
    shadow = {TRIGGER_H: tree.read(TRIGGER_H)} if tree.rev else None
    return compile_and_run(Path(__file__).with_name("FxDirectorPostFxBookkeeping.cpp"), "fxdirector_postfx_block.inc",
                           text + "\n", "FxDirectorPostFxBookkeeping", shadow=shadow,
                           extra_sources=[STRSTREAM_CPP])


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxdirector_postfx", list(wiring(tree)), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
