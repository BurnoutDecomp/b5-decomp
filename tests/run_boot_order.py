"""L4 boot order (2026-09-28): the start-of-game one-shot runs in the loading spine's first frame.

Below load stage 8 the console's LoadingScriptedState::Update @0x823F22D8 -- which every loading-scripted flow
state's Update calls first -- runs its own partial spine, and that spine calls GameStateModule::PreWorldUpdate
@0x823A5328 on EVERY frame, unconditionally (bl @0x823F27BC), after BridgeNetworkToGui (0x823F2758) and before
the world leg (BridgeSoundToWorld 0x823F2818). PreWorldUpdate's first call consumes the one-shot latch that
GameStateModule::Construct @0x82380388 arms (gsm+208324): SendSetupPlayerCarEvent @0x8239A918 +
SendSetUpAllEventStartsMessage. So the default car's OnSpecialEventPlayerCarChange reaches the profile in the
FIRST loading-scripted frame, long before the MemoryCard state deserialises the saved profile. The PC ran that
one-shot only in E_MGS_IN_GAME, after the Deserialise, so it overwrote the saved spawn car.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_boot_order.py [--rev <b5 rev>]
                                                                                        [--root <shadow root>]
(--root reads any file present under <root>/... in place of the working tree's.)
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, code_only, definition, report

FLOW_STATES_CPP = "src/GameSource/GameFlowController/TopLevel/BrnGameMainFlowStates.cpp"
GAME_MODULE_CPP = "src/GameSource/Game/BrnGameModule.cpp"
GAME_STATE_CPP = "src/GameSource/GameState/BrnGameStateModule.cpp"
SPINE_SIGNATURE = "void LoadingScriptedState::Update()"
GAME_MAIN_SIGNATURE = "bool BrnGameModule::GameMain()"
SETUP_LEG_SIGNATURE = "void GameStateModule::PreWorldUpdateSetupPlayerCarBringUp()"
SEAT_CALL = "GetGameStateModule().PreWorldUpdateSetupPlayerCarBringUp()"


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
        return code_only(definition(tree.read(relative).replace("\r\n", "\n"), signature))
    except ValueError:
        return ""


def partial_spine_calls(spine):
    """The seat calls that sit inside an `if (lbPartialSpine)` block of the spine."""
    found = []
    for match in re.finditer(r"if\s*\(\s*lbPartialSpine\s*\)\s*\{", spine):
        block = definition(spine[match.start():], spine[match.start():match.end()])
        if SEAT_CALL in re.sub(r"\s+", "", block).replace("lpGameModule->", ""):
            found.append(match.start())
    return found


def wiring(tree):
    spine = body(tree, FLOW_STATES_CPP, SPINE_SIGNATURE)
    seats = partial_spine_calls(spine) if spine else []
    yield ("the partial spine runs the game-state pre-world pass: LoadingScriptedState::Update calls "
           "PreWorldUpdateSetupPlayerCarBringUp inside `if (lbPartialSpine)` (console bl @0x823F27BC, unconditional "
           "below stage 8)", len(seats) == 1)
    order_ok = False
    if len(seats) == 1:
        network = spine.find("BridgeNetworkToGui(")
        world = spine.find("UpdateWorldModule(")
        order_ok = 0 <= network < seats[0] < world
    yield ("...in the console's order: after BridgeNetworkToGui (0x823F2758) and before the world leg "
           "(BridgeSoundToWorld 0x823F2818)", order_ok)
    yield ("the partial spine is the console's `cmpwi r11,8` test on the scripted load stage (0x823F22F4), read "
           "once at the top of the spine",
           re.search(r"const\s+bool\s+lbPartialSpine\s*=\s*\(\s*gBrnScriptedLoadStage\s*!=\s*8\s*\)", spine)
           is not None)
    game_main = body(tree, GAME_MODULE_CPP, GAME_MAIN_SIGNATURE)
    yield ("the in-game pre-world leg still runs it (a flow that reaches E_MGS_IN_GAME without a loading frame "
           "still fires the one-shot)", "mGameStateModule.PreWorldUpdateSetupPlayerCarBringUp()" in
           re.sub(r"\s+", "", game_main))
    setup = body(tree, GAME_STATE_CPP, SETUP_LEG_SIGNATURE)
    gsm = code_only(tree.read(GAME_STATE_CPP).replace("\r\n", "\n"))
    one_shot = re.search(r"if\s*\(\s*mbSendSetupPlayerCarPending\s*\)\s*\{\s*mbSendSetupPlayerCarPending\s*=\s*false\s*;"
                         r"\s*SendSetupPlayerCarEvent\s*\(", setup) is not None
    armed = re.search(r"mbSendSetupPlayerCarPending\s*=\s*true\s*;", gsm) is not None
    yield ("the latch is the console's one-shot: armed in GameStateModule::Construct (gsm+208324 = 1), cleared "
           "by its first consumer before SendSetupPlayerCarEvent (0x823A5510..0x823A5540)", one_shot and armed)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", default=None, help="b5 revision to test (default: the working tree)")
    parser.add_argument("--root", default=None, help="a shadow tree root whose files take precedence")
    args = parser.parse_args()
    tree = RootTree(args.rev, args.root)
    return report("run_boot_order", list(wiring(tree)), None, 0)


if __name__ == "__main__":
    sys.exit(main())
