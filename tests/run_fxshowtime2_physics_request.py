"""FX-SHOWTIME2 (crash parity 2026-09-24): the physics hop of the showtime traffic-type request.

  PhysicsModule::HandleGameActions @0x825A72F0, jump-table case 109 == action 116
  (0x825A7844..0x825A7854): `bl 0x8259FFD8` OutputBuffer::GetVehicleManagerOutputInterface (write),
  `addi r3, r3, 0x750` (mTrafficTypeRequestQueue), `bl 0x825A3148` EventQueue<u16,32>::AddEvent
  with the action record -- the request GameStateModule::UpdateShowtimeMode posts reaches the queue
  TrafficEntityModule::ProcessTrafficTypeRequests @0x8272B880 answers. The pre-fix arm was a one-shot
  "id 116 DEFERRED" print and the queue stayed empty.

Numeric: tests/FxShowtime2PhysicsRequest.cpp compiles the extracted production arm (and its two
constants) into a one-arm switch against the real VehicleManagerOutputInterface.
Wiring: the deferral print and the `(void)lpOutputBuffer` are gone; the write accessor exists.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxshowtime2_physics_request.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, code_only, compile_and_run, report

ACTIONS_CPP = "src/GameSource/Physics/BrnPhysicsModuleGameActions.cpp"
INTERFACE_H = "src/GameSource/Physics/VehicleManager/SharedIO/BrnVehicleOutputInterface.h"
NUMERIC_CHECKS = 8
ARM_LABEL = "case KI_ACTION_FORWARD_TO_OUTPUT:"


def arm_text(source):
    """`case KI_ACTION_FORWARD_TO_OUTPUT:` plus the brace-balanced block that follows it."""
    start = source.find(ARM_LABEL)
    if start < 0:
        return ""
    brace = source.find("{", start)
    depth = 0
    for token in re.finditer(r'//[^\n]*|/\*[\s\S]*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|[{}]',
                             source[brace:]):
        if token[0] == "{":
            depth += 1
        elif token[0] == "}":
            depth -= 1
            if depth == 0:
                return source[start:brace + token.end()]
    return ""


def constants(source):
    found = []
    for name in ("KI_ACTION_FORWARD_TO_OUTPUT", "KU_EV_LEADING_WORD"):
        match = re.search(r"const\s+(?:s32|u32)\s+" + name + r"\s*=\s*[^;]+;", source)
        if match:
            found.append(match.group(0))
    return found


def wiring(tree):
    source = tree.read(ACTIONS_CPP)
    arm = code_only(arm_text(source))
    yield ("case 116 no longer prints the 'id 116 DEFERRED' one-shot",
           arm != "" and "DEFERRED" not in arm)
    yield ("case 116 reaches the request queue through the write accessor "
           "(bl 0x8259FFD8 ; addi 0x750 ; bl 0x825A3148)",
           re.search(r"GetVehicleManagerOutputInterface\(\)\s*->\s*GetTrafficTypeRequestQueue\(\)\s*->\s*AddEvent\(",
                     arm) is not None)
    yield ("the trailing `(void)lpOutputBuffer` (only 116 read it) is gone",
           "(void)lpOutputBuffer" not in code_only(source))
    header = code_only(tree.read(INTERFACE_H))
    yield ("VehicleManagerOutputInterface has the non-const GetTrafficTypeRequestQueue()",
           re.search(r"TrafficTypeRequestQueue\*\s*GetTrafficTypeRequestQueue\(\)\s*\{", header) is not None)


def numeric(tree):
    source = tree.read(ACTIONS_CPP)
    arm = arm_text(source)
    found = constants(source)
    if not arm or len(found) != 2:
        print("NUMERIC: cannot build -- the case-116 arm or its constants are absent")
        return None
    return compile_and_run(Path(__file__).with_name("FxShowtime2PhysicsRequest.cpp"),
                           "physics_request_arm.inc", arm + "\n", "FxShowtime2PhysicsRequest",
                           extra_files={"physics_request_constants.inc": "\n".join(found) + "\n"})


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxshowtime2_physics_request", list(wiring(tree)), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
