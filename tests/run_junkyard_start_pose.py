"""L4 boot order (conductor 2026-09-28, L2's crash-sweep cell h225_s80): the start-of-game one-shot seeds the
profile's car pose, and the pose follows the junkyard the player was last in, as on the console.

GameStateModule::SendSetupPlayerCarEvent @0x8239A918 opens by storing the TriggerData's player-start pose into
this+0xBCD0 / this+0xBCE0 (`stvx128 v127, r31, r10` @0x8239A97C, `stvx128 v0, r31, r9` @0x8239A980). this+0xBCA0 is
mProgressionManager's embedded Profile, so those are Profile+0x30 mCarPosition / +0x40 mCarDirection -- the pose
OnProfileLoaded @0x82397310 hands FindNearestJunkyardID. The PC dropped the two stores; once the MemoryCard exit ran
OnProfileLoaded (b5 f935feb8), a fresh profile and every save a PC build created (pose never seeded: Profile::Construct's
(0,0,0)) entered the junkyard nearest the ORIGIN, 312262, instead of the start junkyard 250700 -- and L2's sweep cell
fired its shot from 4.8 km away.

THE WRITER (follow-up (2)). GameStateModule::PreWorldUpdate @0x823A5328 hands ProgressionManager::PreWorldUpdate
lbIsInJunkyard = (mCarSelectManager.mJunkyardId != 0 || mOnlineCarSelectManager.mbIsInOnlineCarSelect)
(0x823A5B48..0x823A5B80: `ld r11, 0(this+0x2CDC0)`, else `lbzx r11, r31, 0x2CE34`); while it holds, the callee saves the
car's pose into the profile, so the saved pose follows the junkyard the player was last in. The PC passed false.

  1. WIRING -- the one-shot's step 1 (both stores, before FindNearestJunkyardID); OnProfileLoaded reads that pose;
     the PreWorldUpdate call passes the console's lbIsInJunkyard.
  2. NUMERIC -- tests/JunkyardStartPose.cpp compiles the PRODUCTION SendSetupPlayerCarEvent onto a fixture and
     replays the fresh and the returning boot; tests/JunkyardPoseWriter.cpp evaluates the PRODUCTION lbIsInJunkyard
     argument over the console's truth table and across a junkyard -> exit -> drive sequence.

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
DISPATCH_CPP = "src/GameSource/GameState/GameStateModule_gUI_00.cpp"
SEND_SETUP = "void GameStateModule::SendSetupPlayerCarEvent("
ON_PROFILE_LOADED = "void GameStateModule::OnProfileLoaded("
PM_PREWORLD_CALL = "mProgressionManager.PreWorldUpdate("

NUMERIC_CHECKS = 9 + 6   # see JunkyardStartPose.cpp + JunkyardPoseWriter.cpp


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


def is_in_junkyard_argument(tree):
    """The expression the PreWorldUpdate pump passes as ProgressionManager::PreWorldUpdate's lbIsInJunkyard: the
    call's last argument, or -- when that is a local -- the initializer of the local."""
    source = code_only(tree.read(DISPATCH_CPP).replace("\r\n", "\n"))
    call = source.find(PM_PREWORLD_CALL)
    if call < 0:
        return None
    start = source.index("(", call)
    depth = 0
    end = start
    for end in range(start, len(source)):
        depth += {"(": 1, ")": -1}.get(source[end], 0)
        if depth == 0:
            break
    last = source[start + 1:end].split(",")[-1].strip()
    if re.fullmatch(r"[A-Za-z_]\w*", last) and last not in ("true", "false"):
        found = list(re.finditer(r"\bbool\s+" + re.escape(last) + r"\s*=\s*([^;]+);", source[:call]))
        return " ".join(found[-1].group(1).split()) if found else None
    return last


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
    argument = re.sub(r"\s+", "", is_in_junkyard_argument(tree) or "")
    yield ("PreWorldUpdate @0x823A5328 passes ProgressionManager::PreWorldUpdate the console's lbIsInJunkyard "
           "(0x823A5B48..0x823A5B80: mJunkyardId at this+0x2CDC0 != 0 || the online car select byte at this+0x2CE34)",
           argument == "(mCarSelectManager.GetJunkyardId()!=0)||mOnlineCarSelectManager.IsInOnlineCarSelect()")


def numeric(tree):
    send = body(tree, GSM_CPP, SEND_SETUP)
    argument = is_in_junkyard_argument(tree)
    if not send or argument is None:
        print("NUMERIC: cannot build -- " + ("no SendSetupPlayerCarEvent" if not send else "no lbIsInJunkyard argument"))
        return None
    start_pose = compile_and_run(Path(__file__).with_name("JunkyardStartPose.cpp"), "junkyard_sendsetup.inc",
                                 send + "\n", "JunkyardStartPose")
    writer = compile_and_run(Path(__file__).with_name("JunkyardPoseWriter.cpp"), "junkyard_isinjunkyard.inc",
                             "return (" + argument + ");\n", "JunkyardPoseWriter")
    if start_pose is None or writer is None:
        return None
    return start_pose[0] + writer[0], start_pose[1] + writer[1]


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
