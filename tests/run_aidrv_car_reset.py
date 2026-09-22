"""FX-AIDRV G01-D2: replay the production AICar::Reset (ARTIST @0x82792800) reset-portal stores.

Inside the !lbKeepTransform block the console stores StartPortal = 0 (stb r30 @0x827928AC) and
EndPortal = 1 (stb r9 @0x827928B0). See AIDrvCarReset.cpp.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_aidrv_car_reset.py [--rev <rev>]
"""
import re
import sys
sys.dont_write_bytecode = True
from aidrv_common import REPO, Tree, compile_and_run, constant, definition, parse_args  # noqa: E402

AICAR_UPDATE = "src/GameSource/World/AI/BrnAICar_Update.cpp"


def main():
    args = parse_args()
    source = Tree(args.rev).read(AICAR_UPDATE)
    rng = re.search(r"^[ \t]*static CgsNumeric::Random gAICarRandom;", source, re.M)
    if rng is None:
        raise ValueError("no gAICarRandom declaration")
    chunks = ["namespace BrnAI {", "namespace vpu = rw::math::vpu;",
              constant(source, "KF_AICAR_FLT_MAX"),
              constant(source, "KAF_PERSONALITY_BASE_AGGRESSION"),
              rng[0],
              definition(source, "    void AICar::Reset(EPersonalityType lePersonalityType, bool lbKeepTransform)"),
              "}"]
    extra = [REPO / "src/GameShared/GameClasses/Numeric/CgsRandom.cpp"]
    sys.exit(compile_and_run("AIDrvCarReset.cpp", chunks, extra, prefix="brn_aidrv_reset_"))


if __name__ == "__main__":
    main()
