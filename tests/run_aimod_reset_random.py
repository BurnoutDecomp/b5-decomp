"""FX-AIMOD G07-D6: ResetAwayFromPlayer draws the 0x8300D5D0 stream that AICar::Reset re-seeds.

Run from the workflow checkout:
    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_aimod_reset_random.py [--rev <b5 rev>]
"""
import re
import sys
sys.dont_write_bytecode = True
from aimod_common import Tree, parse_args, compile_and_run, definition, REPO

STRATEGIES = "src/GameSource/World/AI/ResetOnTrack/BrnResetOnTrackManager_Strategies.cpp"
MANAGER = "src/GameSource/World/AI/ResetOnTrack/BrnResetOnTrackManager.cpp"
AICAR_UPDATE = "src/GameSource/World/AI/BrnAICar_Update.cpp"


def constant(source, name):
    match = re.search(r"^[ \t]*(?:static )?const [^;=]*\b" + name + r"\b[^;]*;", source, re.M)
    if match is None:
        raise ValueError("no constant " + name)
    return match[0]


def main():
    tree = Tree(parse_args().rev)
    strategies, manager, aicar = tree.read(STRATEGIES), tree.read(MANAGER), tree.read(AICAR_UPDATE)
    stream = re.search(r"^[ \t]*static CgsNumeric::Random gAICarRandom;", aicar, re.M)[0]
    chunks = ["namespace BrnAI {", "namespace vpu = rw::math::vpu;",
              constant(aicar, "KF_AICAR_FLT_MAX"),
              constant(aicar, "KAF_PERSONALITY_BASE_AGGRESSION"),
              stream,
              definition(aicar, "    f32 AICar::GetRandomNumber() const"),
              definition(aicar, "    void AICar::Reset(EPersonalityType lePersonalityType, bool lbKeepTransform)"),
              definition(manager, "    AICar* ResetOnTrackManager::GetAICar("),
              definition(strategies, "namespace\n{"),
              definition(strategies, "bool ResetOnTrackManager::ResetAwayFromPlayer("),
              "}"]
    sys.exit(compile_and_run("AIModResetRandom.cpp", chunks,
                             [REPO / "src/GameShared/GameClasses/Numeric/CgsRandom.cpp"],
                             prefix="brn_aimod_resetrandom_"))


if __name__ == "__main__":
    main()
