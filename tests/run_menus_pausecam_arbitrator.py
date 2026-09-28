"""OWNERLIST 2026-09-27, lane L5 MENUS: the pause camera -- Arbitrator::Update's crash-nav arms.

  The owner: "The camera behind the pause menu isn't the correct pause camera with black and white filter".
  Arbitrator::Update @0x8226ADA0, NORMAL state, 0x8226B0A0..0x8226B100: when GameState::mbCrashNavShown (+0x101, raised
  by the pause screen's 191{0}) or mbDoing100PercentSequence (+0x1B0) is up and mbGameIntroFlybyActive (+0xD9) is not,
  the console Prepares and Updates mArbStateCrashNav and enters E_STATE_CRASH_NAV_ICE_CAMERAS (4); case 4
  (0x8226B358..0x8226B3BC) asserts IsActive(), ticks the state, takes its camera and returns to NORMAL once the state
  has released itself; case 3 (0x8226B3C0..0x8226B3EC) returns to NORMAL when the crash nav is hidden.
  The PC carried all three as comments behind blockers that had closed, so every pause kept the frozen gameplay camera
  in full colour (no pause playlist, no "Black_In_BW" / "Black_Out_BW" hooks).

Numeric: tests/MenusPauseCamArbitrator.cpp compiles the extracted production trigger and case 3 / case 4 bodies
against recording stand-ins; a revision without the trigger gets a labelled empty stand-in (and fails).
Wiring: the trigger sits in the NORMAL state after the attract-mode trigger (0x8226B08C) and before case 3.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_menus_pausecam_arbitrator.py [--rev <b5 rev>]
        [--src-root <dir holding src/...>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, definition, code_only, compile_and_run, report

ARBITRATOR_CPP = "src/GameSource/Director/Arbitrator/BrnDirectorArbitrator.cpp"
UPDATE = "void Arbitrator::Update(bool lbPaused, Camera::Camera& lrCameraInOut,"
TRIGGER = "if ((lrSharedInfo.mpGameState->mbCrashNavShown"
CASE3 = "case E_STATE_CRASH_NAV:"
CASE4 = "case E_STATE_CRASH_NAV_ICE_CAMERAS:"
CASE5 = "case E_STATE_CHANGING_TO_ATTRACT_MODE:"
NUMERIC_CHECKS = 15


class RootTree(Tree):
    """A b5 source tree rooted somewhere else (e.g. a lane's shadow mirror): <root>/src/..."""

    def __init__(self, root):
        super().__init__(None)
        self.root = Path(root)

    def read(self, relative):
        path = self.root / relative
        if not path.exists():
            return super().read(relative)
        return path.read_text(encoding="utf-8-sig")


def block(text, start):
    """(end index, text) of the brace block that opens at or after `start` in comment-free text."""
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


def update_text(tree):
    try:
        return code_only(definition(tree.read(ARBITRATOR_CPP), UPDATE))
    except ValueError:
        return ""


def pieces(tree):
    update = update_text(tree)
    start = update.find(TRIGGER)
    if start >= 0:
        end, _ = block(update, start)
        trigger = update[start:end]
    else:
        print("NUMERIC: Arbitrator::Update has no crash-nav trigger in this revision (empty stand-in)")
        trigger = "{ /* [stand-in: no crash-nav trigger in this revision] */ }"
    c3, c4, c5 = update.find(CASE3), update.find(CASE4), update.find(CASE5)
    if min(c3, c4, c5) < 0:
        return None
    return trigger, update[c3:c4], update[c4:c5]


def wiring(tree):
    update = re.sub(r"\s+", "", update_text(tree))
    attract = update.find("if(mbDoAttractMode)")
    trigger = update.find(re.sub(r"\s+", "", TRIGGER))
    case3 = update.find(re.sub(r"\s+", "", CASE3))
    yield ("the crash-nav trigger runs in the NORMAL state after the attract-mode trigger (0x8226B08C) and "
           "before the CRASH_NAV case", 0 <= attract < trigger < case3)


def numeric(tree):
    parts = pieces(tree)
    if parts is None:
        print("NUMERIC: Arbitrator::Update's CRASH_NAV cases not found")
        return None
    trigger, case3, case4 = parts
    inc = ("namespace BrnDirector\n{\n"
           "void ArbitratorStandIn::Trigger(bool lbPaused, Camera::Camera& lrCameraInOut, SharedInfoStandIn& lrSharedInfo)\n{\n"
           "(void)lbPaused; (void)lrCameraInOut;\n" + trigger + "\n}\n"
           "void ArbitratorStandIn::Step(bool lbPaused, Camera::Camera& lrCameraInOut, SharedInfoStandIn& lrSharedInfo)\n{\n"
           "(void)lbPaused;\nswitch (meState)\n{\n" + case3 + "\n" + case4 + "\ndefault: break;\n}\n}\n}\n")
    return compile_and_run(Path(__file__).with_name("MenusPauseCamArbitrator.cpp"), "pausecam_arbitrator.inc", inc,
                           "MenusPauseCamArbitrator")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    parser.add_argument("--src-root", help="read the b5 sources from <dir>/src/... (a shadow mirror)")
    args = parser.parse_args()
    tree = RootTree(args.src_root) if args.src_root else Tree(args.rev)
    return report("run_menus_pausecam_arbitrator", list(wiring(tree)), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
