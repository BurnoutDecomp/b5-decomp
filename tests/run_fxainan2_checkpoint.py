"""FX-AINAN2 / crash parity CC-10: AIModule::OnRaceCarReachedCheckpoint @0x8278A658.

The handler was a named park (an ARTIST export hole nobody had disassembled). Extracts the
production body from BrnAIModule_Events.cpp and the three AICar members it inlines
(InvalidateRoute / OnReachedCheckpoint / IsOpponent) from BrnAICar_Update.cpp, feeds it
checkpoint actions for an opponent, the player and a car without an opponent slot, and checks
the seven car stores and the one rubber-band call the console makes. See FxAinan2Checkpoint.cpp
for the ARTIST addresses.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxainan2_checkpoint.py [--rev <rev>]

`--rev <b5 rev>` reads the production files from that revision (the RED side of the fix); an
AICar member that revision lacks is replaced by an empty stub.
"""
import sys
sys.dont_write_bytecode = True
from aidrv_common import (REPO, Tree, body_or_stub, compile_and_run, constant,  # noqa: E402
                          definition, parse_args)

EVENTS = "src/GameSource/World/AI/BrnAIModule_Events.cpp"
CAR = "src/GameSource/World/AI/BrnAICar_Update.cpp"
STRSTREAM = REPO / "src/GameShared/GameClasses/Development/CgsStrStream.cpp"


def main():
    args = parse_args()
    tree = Tree(args.rev)
    events, car = tree.read(EVENTS), tree.read(CAR)
    chunks = ["namespace BrnAI {",
              "namespace {", constant(car, "KF_AICAR_FLT_MAX"), "}",
              body_or_stub(car, "void AICar::InvalidateRoute()",
                           "void AICar::InvalidateRoute() {}"),
              body_or_stub(car, "void AICar::OnReachedCheckpoint(",
                           "void AICar::OnReachedCheckpoint(s32, u16) {}"),
              body_or_stub(car, "bool AICar::IsOpponent() const",
                           "bool AICar::IsOpponent() const { return false; }"),
              definition(events, "void AIModule::OnRaceCarReachedCheckpoint("),
              "}"]
    sys.exit(compile_and_run("FxAinan2Checkpoint.cpp", chunks, extra_sources=[STRSTREAM],
                             prefix="brn_fxainan2_cp_"))


if __name__ == "__main__":
    main()
