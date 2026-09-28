"""OWNERLIST 2026-09-27, lane L5 MENUS: the director's sim-paused flag (the pause-time asserts).

  L1's assert sweep of the owner's pause (menus_pause_camera, exe d65db9997047): "!rw::math::fpu::IsZero(
  lTimeStep.GetFloat())" at BrnLooker.cpp:282 once a frame for the whole pause, from BehaviourBystanderCam through
  BehaviourManager::UpdateAllBehaviours -- which holds every behaviour without the update-during-pause bit when the
  director's paused argument (DirectorIO::InputBuffer::IsSimPaused) is set. The console's only writer of that byte is
  BrnGameModule::DoUpdate_Director @0x823E8DE0 (0x823E8EF8..0x823E8F2C, right after BridgeGameStateToDirector):
      mbSimPaused(input +0x7AC8) = (updateSet & 0x100) ? 0 : (gm.mbOnline ? 0 : gm.mbSimPaused)
  The PC never staged it, so the director always saw an unpaused sim.

Numeric: tests/MenusSimPausedBridge.cpp compiles the extracted production staging block (the `if (!lbPostGui)`
block holding SetSimPaused) against recording stand-ins; a revision without it gets a labelled empty stand-in (and fails).
Wiring: the block is in DoUpdate_Director after BridgeGameStateToDirector; InputBuffer::SetSimPaused stores mbSimPaused.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_menus_simpaused_bridge.py [--rev <b5 rev>]
        [--src-root <dir holding src/...>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, definition, code_only, compile_and_run, report

GAME_MODULE_CPP = "src/GameSource/Game/BrnGameModule.cpp"
DIRECTOR_IO_H = "src/GameSource/Director/DirectorModule/BrnDirectorModuleIO.h"
DO_UPDATE_DIRECTOR = "void BrnGameModule::DoUpdate_Director(bool lbPostGui)"
NUMERIC_CHECKS = 9


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
    """(start of the enclosing `if`, end index) of the brace block that contains `start`'s statement."""
    head = text.rfind("if (!lbPostGui)", 0, start)
    if head < 0:
        return None
    brace = text.find("{", head)
    depth = 0
    for index in range(brace, len(text)):
        if text[index] == "{":
            depth += 1
        elif text[index] == "}":
            depth -= 1
            if depth == 0:
                return head, index + 1
    return None


def director_text(tree):
    try:
        return code_only(definition(tree.read(GAME_MODULE_CPP), DO_UPDATE_DIRECTOR))
    except ValueError:
        return ""


def staging(tree):
    text = director_text(tree)
    store = text.find("SetSimPaused(")
    if store < 0:
        return None
    span = block(text, store)
    if span is None:
        return None
    return text[span[0]:span[1]]


def wiring(tree):
    text = re.sub(r"\s+", "", director_text(tree))
    bridge = text.find("BridgeGameStateToDirector(lpDirectorInput,lpGameStateOutput);")
    store = text.find("SetSimPaused(")
    yield ("DoUpdate_Director stages SetSimPaused after BridgeGameStateToDirector (0x823E8EF4 -> 0x823E8F2C)",
           0 <= bridge < store)
    yield ("DoUpdate_Director stages it exactly once", text.count("SetSimPaused(") == 1)
    header = re.sub(r"\s+", "", code_only(tree.read(DIRECTOR_IO_H)))
    yield ("DirectorIO::InputBuffer::SetSimPaused (DWARF BrnDirectorModuleIO.h:284) stores mbSimPaused",
           "voidSetSimPaused(boollbSimPaused){mbSimPaused=lbSimPaused;}" in header)


def numeric(tree):
    body = staging(tree)
    if body is None:
        print("NUMERIC: DoUpdate_Director stages no SetSimPaused in this revision (empty stand-in)")
        body = "{ /* [stand-in: no sim-paused staging in this revision] */ }"
    inc = ("void GameModuleStandIn::Stage(bool lbPostGui, DirectorInputStandIn* lpDirectorInput)\n{\n"
           "(void)lbPostGui; (void)lpDirectorInput;\n" + body + "\n}\n")
    return compile_and_run(Path(__file__).with_name("MenusSimPausedBridge.cpp"), "simpaused_bridge.inc", inc,
                           "MenusSimPausedBridge")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    parser.add_argument("--src-root", help="read the b5 sources from <dir>/src/... (a shadow mirror)")
    args = parser.parse_args()
    tree = RootTree(args.src_root) if args.src_root else Tree(args.rev)
    return report("run_menus_simpaused_bridge", list(wiring(tree)), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
