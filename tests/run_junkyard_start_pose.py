"""L4 boot order (conductor 2026-09-28, L2's crash-sweep cell h225_s80): the start-of-game one-shot seeds the
profile's car pose, as on the console.

GameStateModule::SendSetupPlayerCarEvent @0x8239A918 opens by storing the TriggerData's player-start pose into
this+0xBCD0 / this+0xBCE0 (`stvx128 v127, r31, r10` @0x8239A97C, `stvx128 v0, r31, r9` @0x8239A980). this+0xBCA0 is
mProgressionManager's embedded Profile, so those are Profile+0x30 mCarPosition / +0x40 mCarDirection -- the pose
OnProfileLoaded @0x82397310 hands FindNearestJunkyardID. The PC dropped the two stores; once the MemoryCard exit ran
OnProfileLoaded (b5 f935feb8), a fresh profile and every save a PC build wrote (pose never seeded: Profile::Construct's
(0,0,0)) entered the junkyard nearest the ORIGIN, 312262, instead of the start junkyard 250700 -- and L2's sweep cell
fired its shot from 4.8 km away.

  1. WIRING -- the one-shot's step 1 (both stores, before FindNearestJunkyardID); OnProfileLoaded reads that pose.
  2. NUMERIC -- tests/JunkyardStartPose.cpp compiles the PRODUCTION SendSetupPlayerCarEvent onto a fixture and
     replays the fresh and the returning boot.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_junkyard_start_pose.py [--rev <b5 rev>]
                                                                                                [--root <shadow root>]
(--root reads any file present under <root>/... in place of the working tree's.)
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, code_only, compile_and_run, definition, report

GSM_CPP = "src/GameSource/GameState/BrnGameStateModule.cpp"
SEND_SETUP = "void GameStateModule::SendSetupPlayerCarEvent("
ON_PROFILE_LOADED = "void GameStateModule::OnProfileLoaded("

NUMERIC_CHECKS = 9    # see JunkyardStartPose.cpp


class RootTree(Tree):
    """The working tree (or --rev) with an optional shadow root whose files take precedence."""

    def __init__(self, rev=None, root=None):
        super().__init__(rev)
        self.root = Path(root) if root else None

    def read(self, relative):
        if self.root is not None and (self.root / relative).exists():
            return (self.root / relative).read_text(encoding="utf-8-sig")
        try:
            return super().read(relative)
        except FileNotFoundError:
            return ""


def body(tree, relative, signature):
    try:
        return definition(tree.read(relative).replace("\r\n", "\n"), signature)
    except ValueError:
        return ""


def squash(text):
    return re.sub(r"\s+", "", code_only(text))


def in_order(text, needles):
    position = -1
    for needle in needles:
        found = text.find(needle, position + 1)
        if found < 0:
            return False
        position = found
    return True


def wiring(tree):
    send = squash(body(tree, GSM_CPP, SEND_SETUP))
    yield ("SendSetupPlayerCarEvent @0x8239A918 step 1: the TriggerData start pose goes into the PROFILE "
           "(Profile+0x30 / +0x40 == this+0xBCD0 / +0xBCE0, @0x8239A97C / @0x8239A980) before the junkyard is chosen",
           in_order(send, ["lpTriggerData->GetPlayerStartPosition()",
                           "mProgressionManager.GetProfile()",
                           "->SetCarPosition(lPlayerStart)",
                           "->SetCarDirection(lpTriggerData->GetPlayerStartDirection())",
                           "mpVehicleList->GetVehicleData(0)",
                           "FindNearestJunkyardID(lPlayerStart)",
                           "mCarSelectManager.EnterJunkyardAtStartOfGame("]))
    opl = squash(body(tree, GSM_CPP, ON_PROFILE_LOADED))
    yield ("OnProfileLoaded @0x82397310 enters the junkyard nearest THAT pose (`lvx128 v1, r30, 0x30` @0x823973D0)",
           "FindNearestJunkyardID(lpProfile->GetCarPosition())" in opl)


def numeric(tree):
    send = body(tree, GSM_CPP, SEND_SETUP)
    if not send:
        print("NUMERIC: cannot build -- no SendSetupPlayerCarEvent")
        return None
    return compile_and_run(Path(__file__).with_name("JunkyardStartPose.cpp"), "junkyard_sendsetup.inc",
                           send + "\n", "JunkyardStartPose")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", default=None, help="b5 revision to test (default: the working tree)")
    parser.add_argument("--root", default=None, help="a shadow tree root whose files take precedence")
    args = parser.parse_args()
    tree = RootTree(args.rev, args.root)
    checks = list(wiring(tree))
    return report("run_junkyard_start_pose", checks, numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
