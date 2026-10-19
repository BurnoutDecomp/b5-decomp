"""FX-GS (crash parity 2026-09-23): BrnGameState::CrashModeScoring regressions.

  G10-D10  CrashModeScoring::ClearData @0x82320D10 -- the CgsID stunt ring (+0x280) is cleared
           (0x82320D90..0x82320D98) and +0x304 is never stored.

Numeric: tests/FxGsCrashScoring.cpp compiled against the extracted production ClearData and the
revision's own BrnCrashModeScoringRecentCrash.h.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxgs_crash_scoring.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, extract, compile_and_run, report, body_or_empty

SCORING_CPP = "src/GameSource/GameState/ModeManager/Scoring/BrnCrashModeScoring.cpp"
SCORING_H = "src/GameSource/GameState/ModeManager/Scoring/BrnCrashModeScoringRecentCrash.h"
BODIES = ["    void CrashModeScoring::ClearData()"]
BASE_CPP = "src/GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugComponent.cpp"
BASE_BODIES = [
    "    DebugComponent::DebugComponent()",
    "    void        DebugComponent::Update()",
    "    void        DebugComponent::RenderWorld(",
    "    void        DebugComponent::RenderHUD(",
    "    const char* DebugComponent::GetName() const",
    "    const char* DebugComponent::GetPath() const",
    "    bool        DebugComponent::IsSimple() const",
    "    void        DebugComponent::OnActivate()",
    "    void        DebugComponent::OnRegister()",
]
NUMERIC_CHECKS = 27


def wiring(tree):
    body = body_or_empty(tree.read(SCORING_CPP), BODIES[0])
    stunt = body.find("mRecentStuntSet.Clear()")
    props = body.find("mRecentlyHitPropSet.Clear()")
    crashes = body.find("maRecentCrashes.Clear()")
    yield ("D10 ClearData clears mRecentStuntSet before the prop ring and the crash set "
           "(console order 0x82320D90 < 0x82320D9C < 0x82320DAC)",
           0 <= stunt < props < crashes)
    yield ("D10 ClearData has no mfResetComboGracePeriod store (no +0x304 in 0x82320D10..0x82320DC4)",
           body != "" and "mfResetComboGracePeriod" not in body)


def numeric(tree):
    texts, missing = extract(tree, SCORING_CPP, BODIES)
    if missing:
        print("NUMERIC: cannot build -- production bodies absent: " + ", ".join(missing))
        return None
    base, base_missing = extract(tree, BASE_CPP, BASE_BODIES)
    if base_missing:
        print("NUMERIC: cannot build -- debug base bodies absent: " + ", ".join(base_missing))
        return None
    inc = ("namespace CgsDev {\n" + "\n".join(base) + "\n}\n"
           + "namespace BrnGameState {\n" + "\n".join(texts) + "\n}\n")
    return compile_and_run(Path(__file__).with_name("FxGsCrashScoring.cpp"), "crash_scoring_methods.inc",
                           inc, "FxGsCrashScoring", shadow={SCORING_H: tree.read(SCORING_H)})


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxgs_crash_scoring", list(wiring(tree)), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
