"""FX-RUMBLE3 (crash parity 2026-09-24, G10-D4): the game state's rumble requests reach the input module.

Before this fix nothing drained BrnGameState::RumbleManager's four event queues on PC: the console's first update
leg, BrnGame::BrnGameModule::DoUpdate_InputPreWorld @0x823C5650, did not exist, GameStateModule::BridgeRumbleToInput
@0x8236B570 and RumbleManager::BridgeRumbleToInput @0x82364978 had no bodies, CgsInput::InputIO::PreWorldInputBuffer
held its ChangeVolume / Stop queues and three tail flags as a padding gap with no Construct and no Post*ByPlayer, and
BrnGameModule.hpp embedded an EMPTY ODR stub of CgsInput::InputModule (no PreWorldUpdate to call).

  1. WIRING -- the game module runs the console's leg, in the console's place, through the real module.
  2. NUMERIC -- tests/FxRumble3BridgeRumble.cpp compiles the PRODUCTION RumbleManager::Construct /
     BridgeRumbleToInput and the PreWorldInputBuffer bodies (Construct / Destruct / the four posts / the queue and
     timer accessors) against the revision's own BrnRumbleManager.h and CgsInputModuleIO.h, with the real
     CgsModule::IOBuffer, and checks every store against the ARTIST asm (addresses in the fixture banner).

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxrumble3_bridge_rumble.py [--rev <b5 rev>]

A revision that lacks any of the bodies cannot build the numeric test: every numeric check is then counted failed,
with the absent bodies named.
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import REPO, STRSTREAM_CPP, Tree, body_or_empty, code_only, compile_and_run, definition, report

RUMBLE_H = "src/GameSource/GameState/RumbleManager/BrnRumbleManager.h"
RUMBLE_CPP = "src/GameSource/GameState/RumbleManager/BrnRumbleManager.cpp"
IO_H = "src/GameShared/GameClasses/System/Input/CgsInputModuleIO.h"
IO_CPP = "src/GameShared/GameClasses/System/Input/CgsInputModuleIO.cpp"
QUEUES_CPP = "src/GameShared/GameClasses/System/Input/CgsInputProcessRumbleQueues.cpp"
GSM_CPP = "src/GameSource/GameState/BrnGameStateModule.cpp"
GAME_HPP = "src/GameSource/Game/BrnGameModule.hpp"
GAME_CPP = "src/GameSource/Game/BrnGameModule.cpp"
IOBUFFER_CPP = REPO / "src/GameShared/GameClasses/Module/CgsIOBuffer.cpp"

NUMERIC_CHECKS = 35

IO_BODIES = [
    "void PreWorldInputBuffer::Construct()",
    "void PreWorldInputBuffer::Destruct()",
    "void PreWorldInputBuffer::PostPlayJoltEffectByPlayer(",
    "void PreWorldInputBuffer::PostPlayRumbleEffectByPlayer(",
    "void PreWorldInputBuffer::PostChangeVolumeRumbleEffectByPlayer(",
    "void PreWorldInputBuffer::PostStopRumbleEffectByPlayer(",
    "const PreWorldInputBuffer::PlayJoltEffectEventQueue* PreWorldInputBuffer::GetPlayJoltEffectEventQueue() const",
    "const PreWorldInputBuffer::ChangeVolumeRumbleEffectEventQueue*\nPreWorldInputBuffer::GetChangeVolumeRumbleEffectEventQueue() const",
    "const PreWorldInputBuffer::StopRumbleEffectEventQueue*\nPreWorldInputBuffer::GetStopRumbleEffectEventQueue() const",
    "const CgsSystem::TimerStatusInterface* PreWorldInputBuffer::GetTimerStatusInt() const",
    "void PreWorldInputBuffer::SetTimerStatusInterface(",
]
QUEUE_BODIES = [
    "const PreWorldInputBuffer::PlayRumbleEffectEventQueue*\nPreWorldInputBuffer::GetPlayRumbleEffectEventQueue() const",
]
RUMBLE_BODIES = [
    "    void RumbleManager::Construct()",
    "    void RumbleManager::BridgeRumbleToInput(",
]


def normalised(tree, relative):
    return tree.read(relative).replace("\r\n", "\n")


def wiring(tree):
    gsm = body_or_empty(normalised(tree, GSM_CPP), "void GameStateModule::BridgeRumbleToInput(")
    yield ("GameStateModule::BridgeRumbleToInput (0x8236B570) forwards the buffer and the timer to mRumbleManager",
           re.search(r"mRumbleManager\.BridgeRumbleToInput\(\s*lpInputInputBuffer\s*,\s*lpTimerStatusInterface\s*\)", gsm)
           is not None)

    header = code_only(normalised(tree, GAME_HPP))
    yield ("BrnGameModule.hpp no longer defines an empty CgsInput::InputModule ODR stub",
           re.search(r"class\s+InputModule\s*:\s*public\s+CgsModule::ModuleSingleBuffered\s*\{\s*\}", header) is None)
    yield ("...it includes the real CgsInput::InputModule (GameShared/GameClasses/Input/CgsInputModule.h)",
           '#include "GameShared/GameClasses/Input/CgsInputModule.h"' in header)

    game = normalised(tree, GAME_CPP)
    leg = body_or_empty(game, "u32 BrnGameModule::DoUpdate_InputPreWorld(")
    helper = re.search(r"CgsModule::IOHelper<\s*CgsInput::InputIO::PreWorldInputBuffer\s*>\s*(\w+)\(\s*lpInputBufferStack\s*,"
                       r"\s*\"InputPreWorld\"\s*\)", leg)
    yield ("DoUpdate_InputPreWorld (0x823C5650) carves the \"InputPreWorld\" PreWorldInputBuffer off the input stack (0x823C5690)",
           helper is not None)
    name = helper.group(1) if helper else "\x00"
    lock = leg.find(name + "->LockForWrite()")
    bridge = re.search(r"mGameStateModule\.BridgeRumbleToInput\(\s*" + re.escape(name) + r"\s*,\s*&mTimerStatusInterface\s*\)", leg)
    unlock = leg.find(name + "->UnlockForWrite()")
    module = re.search(r"mInputModule\.PreWorldUpdate\(\s*lpInputBufferStack\s*,\s*lpOutputBufferStack\s*,\s*" + re.escape(name)
                       + r"\s*,\s*lpInputModuleOutput\s*,\s*!\s*mbDiskError\s*\)", leg)
    yield ("...LockForWrite, GameStateModule::BridgeRumbleToInput(buffer, &mTimerStatusInterface), UnlockForWrite"
           " (0x823C56DC / 0x823C56F4 / 0x823C56FC)",
           bridge is not None and 0 <= lock < bridge.start() < unlock)
    yield ("...then mInputModule.PreWorldUpdate(inStack, outStack, buffer, output, !mbDiskError) (vtable+0x44, 0x823C573C)",
           module is not None and bridge is not None and unlock < module.start())
    start = leg.find("StartMonitor(mCpuMonitors.miUT_GameState)")
    bridge_start = leg.find("StartMonitor(mCpuMonitors.miUT_GameState_Bridge)")
    bridge_stop = leg.find("StopMonitor(mCpuMonitors.miUT_GameState_Bridge)")
    yield ("...inside miUT_GameState (0x823C5678), the bridge inside miUT_GameState_Bridge (0x823C56D0 / 0x823C5704)",
           0 <= start < bridge_start < lock and unlock < bridge_stop)

    code = code_only(game)
    call = re.search(r"DoUpdate_InputPreWorld\(\s*mpUpdateInputBufferStack\s*,\s*mpUpdateOutputBufferStack\s*,"
                     r"\s*&mPcInputOutputBuffer\s*\)\s*;", code)
    network = re.search(r"DoUpdate_NetworkPreSim\(\s*mpUpdateInputBufferStack", code)
    yield ("the update step calls DoUpdate_InputPreWorld(inStack, outStack, &input output) BEFORE DoUpdate_NetworkPreSim"
           " (DoUpdate 0x823F0F58 < 0x823F1010)",
           call is not None and network is not None and call.start() < network.start())
    gate = code[max(0, call.start() - 400):call.start()] if call else ""
    yield ("...in the in-game state and every loading-scripted state (the two console callers: DoUpdate, and"
           " LoadingScriptedState::Update 0x823F26F4)",
           "E_MGS_IN_GAME" in gate and "E_MGS_CHECK_DISK_SPACE" in gate and "E_MGS_COMPLETE_LOADING" in gate)


def numeric(tree):
    io_cpp = normalised(tree, IO_CPP)
    queues_cpp = normalised(tree, QUEUES_CPP)
    rumble_cpp = normalised(tree, RUMBLE_CPP)
    missing = [s.strip() for s in IO_BODIES if s not in io_cpp]
    missing += [s.strip() for s in QUEUE_BODIES if s not in queues_cpp]
    missing += [s.strip() for s in RUMBLE_BODIES if s not in rumble_cpp]
    if missing:
        print("NUMERIC: cannot build -- production bodies absent: " + "; ".join(m.replace("\n", " ") for m in missing))
        return None

    includes = re.findall(r"^#include [^\n]+", rumble_cpp, re.M)
    text = "\n".join(
        ["namespace CgsInput { namespace InputIO {"]
        + [definition(io_cpp, s) for s in IO_BODIES]
        + [definition(queues_cpp, s) for s in QUEUE_BODIES]
        + ["} }"]
        + includes
        + ["namespace BrnGameState {"]
        + [definition(rumble_cpp, s) for s in RUMBLE_BODIES]
        + ["}"])
    return compile_and_run(Path(__file__).with_name("FxRumble3BridgeRumble.cpp"), "fxrumble3_bridge_rumble.inc", text,
                           "FxRumble3BridgeRumble",
                           shadow={RUMBLE_H: tree.read(RUMBLE_H), IO_H: tree.read(IO_H)},
                           extra_sources=(IOBUFFER_CPP, STRSTREAM_CPP))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxrumble3_bridge_rumble", list(wiring(tree)), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
