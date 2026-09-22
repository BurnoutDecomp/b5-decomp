"""FX-AIDRV G01-D3: AICar::GetRandomNumber draws the 0x8300D5D0 stream AICar::Reset re-seeds.

Extracts the production AICar::Reset, the file-scope stream and AICar::GetRandomNumber from
BrnAICar_Update.cpp. A revision without GetRandomNumber (the pre-fix tree) gets a NaN stub, so
its draw checks fail instead of the build. See AIDrvCarRandom.cpp.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_aidrv_car_random.py [--rev <rev>]
"""
import re
import sys
sys.dont_write_bytecode = True
from aidrv_common import (REPO, Tree, body_or_stub, compile_and_run, constant,  # noqa: E402
                          definition, parse_args)

AICAR_UPDATE = "src/GameSource/World/AI/BrnAICar_Update.cpp"
MISSING = ("    f32 AICar::GetRandomNumber() const\n"
           "    {\n"
           "        return std::nanf(\"\");   // [test stub] no GetRandomNumber in this revision\n"
           "    }")


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
              body_or_stub(source, "    f32 AICar::GetRandomNumber() const", MISSING),
              definition(source, "    void AICar::Reset(EPersonalityType lePersonalityType, bool lbKeepTransform)"),
              "}"]
    extra = [REPO / "src/GameShared/GameClasses/Numeric/CgsRandom.cpp"]
    sys.exit(compile_and_run("AIDrvCarRandom.cpp", chunks, extra, prefix="brn_aidrv_random_"))


if __name__ == "__main__":
    main()
