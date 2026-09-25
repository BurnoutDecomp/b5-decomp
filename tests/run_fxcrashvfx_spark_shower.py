"""FX-CRASHVFX (crash parity 2026-09-24): ParticleModule::HandleSpawnSparkShowerFromPointEvent @0x82299CC8 -- the
consumer that turns every spark-shower record (world grinding, vehicle grinding, the crash shower) into sparks.
Before this fix it announced itself NOT RECONSTRUCTED and dropped the record: the drain posted showers nobody drew.

NUMERIC -- tests/FxCrashVfxSparkShower.cpp compiles the PRODUCTION body (with the file-local constants and VMX
idioms it names, and the production randomiser bodies) onto a fixture and compares every SpawnSpark call, bit for
bit, with the handler's own instruction words run on emu64 (tests/FxCrashVfxSparkShowerData.h, written by
scratch/CRASHPARITY_0922/fxcrashvfx_vmxemu/gen_shower_data.py).

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxcrashvfx_spark_shower.py [--rev <b5 rev>]
                                                                             [--root <shadow tree root>]
"""
from pathlib import Path
import argparse
import sys

sys.dont_write_bytecode = True
from fxgs_common import REPO, Tree, code_only, compile_and_run, definition, report

EVENTS_CPP = "src/GameSource/Effects/Particles/ParticleModule_SparkEvents.cpp"
UTILS_CPP = "src/GameSource/Effects/BrnEffectsUtils.cpp"
SHADOW_HEADERS = ("src/GameSource/Effects/BrnEffectsUtils.h",
                  "src/GameSource/Effects/Particles/BrnParticleModuleIO_EventTypes.h")
RANDOM_CPP = REPO / "src/GameShared/GameClasses/Numeric/CgsRandom.cpp"
SIGNATURE = "void ParticleModule::HandleSpawnSparkShowerFromPointEvent("
CONSTS_START = "    static const f32 KF_SHOWER_REFLECTION_SCALE"
CONSTS_END = "    // The once-only announcement helper"
RANDOMISERS = ("void Vector4Randomiser::Prepare(", "Vector4 Vector4Randomiser::RandomiseXYZW(")

# 7 cases x 3 checks + 1 global check (see FxCrashVfxSparkShower.cpp)
NUMERIC_CHECKS = 7 * 3 + 1


class RootTree(Tree):
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


def wiring(tree):
    source = tree.read(EVENTS_CPP).replace("\r\n", "\n")
    try:
        body = code_only(definition(source, SIGNATURE))
    except ValueError:
        body = ""
    yield ("HandleSpawnSparkShowerFromPointEvent runs its body (no NOT RECONSTRUCTED announcement) and spawns into "
           "maSparks[type] through SparkArray::SpawnSpark",
           body != "" and "LogSparkEventNotReconstructed" not in body and ".SpawnSpark(" in body)
    yield ("its constants are the image's, cited by address (flt_82001D9C 2.0, flt_820139F8 1/60, the seven "
           "TrigBaseFunctions5 splats 0x8307A3B0..0x8307A680 with their CRT thunks)",
           all(token in source for token in ("flt_82001D9C", "flt_820139F8", "0x8307A680", "0x8307A590", "0x8307A3C0",
                                             "0x8307A560", "0x8307A670", "0x8307A3B0", "0x8307A5F0", "0x82C6EB00")))


def numeric(tree):
    source = tree.read(EVENTS_CPP).replace("\r\n", "\n")
    try:
        body = definition(source, SIGNATURE)
    except ValueError:
        print("NUMERIC: cannot build -- HandleSpawnSparkShowerFromPointEvent is absent")
        return None
    consts = ""
    if CONSTS_START in source and CONSTS_END in source:
        consts = source[source.index(CONSTS_START):source.index(CONSTS_END)]
    utils = tree.read(UTILS_CPP).replace("\r\n", "\n")
    randomisers = []
    for signature in RANDOMISERS:
        try:
            randomisers.append(definition(utils, signature))
        except ValueError:
            pass
    shadow = {relative: tree.read(relative) for relative in SHADOW_HEADERS if tree.read(relative)}
    return compile_and_run(Path(__file__).with_name("FxCrashVfxSparkShower.cpp"), "fxcrashvfx_shower_body.inc",
                           body + "\n", "FxCrashVfxSparkShower", shadow=shadow, extra_sources=(RANDOM_CPP,),
                           extra_files={"fxcrashvfx_shower_consts.inc": consts + "\n",
                                        "fxcrashvfx_shower_randomisers.inc": "\n".join(randomisers) + "\n"})


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", default=None, help="b5 revision to test (default: the working tree)")
    parser.add_argument("--root", default=None, help="a shadow tree root whose src/ files take precedence")
    args = parser.parse_args()
    tree = RootTree(args.rev, args.root)
    checks = list(wiring(tree))
    result = numeric(tree)
    return report("run_fxcrashvfx_spark_shower", checks, result, NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
