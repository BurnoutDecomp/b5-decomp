"""crash parity FX-NETCRASH (2026-09-25), online hull set piece 2: the traffic un-pause and the
network traffic restart.

The online PREPARE_FOR_MODE reset leaves the traffic PAUSED. HandlePrepareForModeAction stores 1
(E_RUNNINGSTATE_PAUSED) to meRunningStateToUseAfterStartup at 0x827484E4. The console un-pauses it
through TrafficEntityModule::HandleExternalRequests @0x8274B660, and PC wired none of those arms, so
online traffic never left PAUSED:
  47  E_ACTION_SET_COUNTDOWN (0x8274BD98): online, RUNNING && IsPaused() -> meRunningState = NORMAL,
      else STARTING_UP -> meRunningStateToUseAfterStartup = NORMAL. Its
      TrafficLightManager::SetCountdownValue leg stays a named gate.
  143 E_ACTION_SHOWTIME_MODE_SWITCH (0x8274BFE0): !mbEnteringShowtime -> muCurrentlyPredictedHull = 0xFFFF.
  225/226 E_ACTION_LOCAL_PLAYER_DISCONNECTED / _LEFT_GAME (0x8274BE4C): online -> RestartTraffic().
  236 E_ACTION_RESTART_TRAFFIC (0x8274BEE0): online -> RestartTraffic(),
      mbActivateOnlineHullsAfterReset = true, mau16HullsToActivateAfterReset = the record's 8 hulls.
Also Reset's replay block (0x8272D470..0x8272D6D8). While the flag is up, it seeds each slot's
maaRaceCarHulls with its parked hull plus that hull's PVS, and the local player's hull becomes
muCurrentlyPredictedHull. Then it consumes the flag and the parked hulls. Construct
(0x827408B4..0x827408E4) starts the replay disarmed, with the flag down and the eight parked hulls at
0xFFFF. Without that, the first online PREPARE_FOR_MODE replayed hull 0 for seven empty slots.

The runner extracts the PRODUCTION bodies (HandleExternalRequests, RestartTraffic, IsPaused,
EnterTearingDownState, and the replay block out of Reset) and compiles them against
FxNetcrashExternalRequests.cpp. Actions go through a real VariableEventQueue<13312,16>, and the
replay walks a real Pvs. On a revision without the arms the actions fall to `default`, and without the
replay block the replay is an empty function, so every behavioural check fails.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxnetcrash_external_requests.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import REPO, Tree, definition, code_only, compile_and_run, report, STRSTREAM_CPP

MODULE_CPP = "src/GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.cpp"
PVS_CPP = "src/SharedClasses/Traffic/BrnTrafficPvs.cpp"
FIXTURE = "ExtFixture"

BODIES = [
    "bool TrafficEntityModule::IsPaused()",
    "void TrafficEntityModule::RestartTraffic()",
    "void TrafficEntityModule::EnterTearingDownState()",
    "void TrafficEntityModule::HandleExternalRequests(",
]
RESET = "void TrafficEntityModule::Reset()"
REPLAY_IF = "    if (mbActivateOnlineHullsAfterReset)\n"

NUMERIC_CHECKS = 25


def replay_block(module):
    """The `if (mbActivateOnlineHullsAfterReset) { ... }` statement inside Reset, or '' if absent."""
    try:
        reset = definition(module, RESET)
    except ValueError:
        return ""
    at = reset.find(REPLAY_IF)
    if at < 0:
        return ""
    return definition(reset[at:], REPLAY_IF.strip("\n").lstrip())


def numeric(tree):
    module = tree.read(MODULE_CPP).replace("\r\n", "\n")
    parts = []
    try:
        for signature in BODIES:
            body = definition(module, signature).replace("TrafficEntityModule::", FIXTURE + "::", 1)
            parts.append(body)
    except ValueError as error:
        print("NUMERIC: cannot build -- production body absent: " + str(error))
        return None
    text = "\n".join(parts)
    text = text.replace("BrnTrafficIO::InputBuffer_PostPhysics::GameActionQueueStorage",
                        "FakeInput::GameActionQueueStorage")
    text = re.sub(r"const\s+BrnTrafficIO::InputBuffer_PostPhysics\*\s*lpInput", "const FakeInput* lpInput", text)
    text = re.sub(r"BrnTrafficIO::OutputBuffer_PostPhysics\*\s*lpOutput", "FakeOutput* lpOutput", text)
    block = replay_block(module)
    if not block:
        print("NUMERIC: Reset has no mbActivateOnlineHullsAfterReset replay block in this revision")
    text += "\nvoid " + FIXTURE + "::ReplayOnlineHullSet()\n{\n" + block + "\n}\n"
    return compile_and_run(Path(__file__).with_name("FxNetcrashExternalRequests.cpp"), "external_requests_bodies.inc",
                           text, "FxNetcrashExternalRequests", extra_sources=[STRSTREAM_CPP, REPO / PVS_CPP])


def wiring(tree):
    module = tree.read(MODULE_CPP).replace("\r\n", "\n")
    try:
        handler = code_only(definition(module, "void TrafficEntityModule::HandleExternalRequests("))
    except ValueError:
        handler = ""
    block = code_only(replay_block(module))
    reset = ""
    try:
        reset = code_only(definition(module, RESET))
    except ValueError:
        pass
    try:
        construct = code_only(definition(module, "void TrafficEntityModule::Construct()"))
    except ValueError:
        construct = ""
    maths = reset.find("mbNetworkHasDetectedDivergence = false;")
    replay = reset.find("if (mbActivateOnlineHullsAfterReset)")
    return [
        ("HandleExternalRequests has the SET_COUNTDOWN arm (47, 0x8274BD98)", "E_ACTION_SET_COUNTDOWN:" in handler),
        ("HandleExternalRequests has the SHOWTIME_MODE_SWITCH arm (143, 0x8274BFE0)", "E_ACTION_SHOWTIME_MODE_SWITCH:" in handler),
        ("HandleExternalRequests has the LOCAL_PLAYER_DISCONNECTED / LEFT_GAME arms (225/226, 0x8274BE4C)",
         "E_ACTION_LOCAL_PLAYER_DISCONNECTED:" in handler and "E_ACTION_LOCAL_PLAYER_LEFT_GAME:" in handler),
        ("HandleExternalRequests has the RESTART_TRAFFIC arm (236, 0x8274BEE0) and it calls RestartTraffic()",
         "E_ACTION_RESTART_TRAFFIC:" in handler and "RestartTraffic();" in handler),
        ("Reset replays the parked online hull set after the divergence resets (0x8272D470)",
         0 <= maths < replay and "GetHullPvs(" in block),
        ("Construct disarms the replay: mbActivateOnlineHullsAfterReset = false and the eight parked hulls "
         "= 0xFFFF (0x827408B4..0x827408E4)",
         re.search(r"mbActivateOnlineHullsAfterReset\s*=\s*false;", construct) is not None
         and re.search(r"mau16HullsToActivateAfterReset\[luSlot\]\s*=\s*KU_INVALID_HULL;", construct) is not None),
    ]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxnetcrash_external_requests", wiring(tree), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
