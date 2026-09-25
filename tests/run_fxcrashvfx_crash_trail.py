"""FX-CRASHVFX (crash parity 2026-09-25, item 4): THE CRASHING TRAIL -- EffectsModule::HandleCrashingTrail @0x82290D30.

What a player sees on the console while a car crashes: the wreck sheds debris all along its slide -- painted, shiny,
dark, detailed and glass pieces and impact smoke, more of them the faster it slides. UpdateActiveRaceCars calls the
handler for every crashing car on every sim step. Before this fix it announced itself NOT RECONSTRUCTED and the
wreck shed nothing.

  1. WIRING -- the handler runs its body (no announcement) into SpawnDebris and SpawnSimple; its literals and layout
     words are the image's, each cited; the caller hands it the car colour and the player / AI trail debrisparams.
  2. NUMERIC -- tests/FxCrashVfxCrashTrail.cpp compiles the PRODUCTION region onto a fixture and compares every
     SpawnDebris / SpawnSimple, the accumulators and mRandom bit for bit with 0x82290D30 run on emu64
     (scratch/CRASHPARITY_0922/fxcrashvfx_vmxemu/gen_trail_data.py; 10 cases x 7 checks).

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxcrashvfx_crash_trail.py [--rev <b5 rev>]
                                                                             [--root <shadow tree root>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import REPO, Tree, code_only, compile_and_run, definition, report

EFFECTS_CPP = "src/GameSource/Effects/EffectsModule.cpp"
RANDOM_CPP = REPO / "src/GameShared/GameClasses/Numeric/CgsRandom.cpp"
SIGNATURE = "void EffectsModule::HandleCrashingTrail("
REGION_MARK = "THE CRASHING TRAIL -- FX-CRASHVFX"
HELPERS = ("f32 Dot3(", "f32 Vnmsub(", "f32 RefinedRsqrt(", "f32 GuardedLength3(", "Vector3 Scale4(",
           "const u8* DebrisParamsLayout(", "f32 LayoutFloat(", "Vector3 Add4(", "Vector3 Sub4(", "Vector3 Mul4(",
           "Vector3 MaddSplat4(", "Vector3 MakeVector3(", "Vector3 RandomUnitVector(")
NO_TRAIL_BODY = """
// [fixture] this revision's HandleCrashingTrail has the old five-parameter stub: a stand-in that announces, so the
// fixture compiles and measures what the revision spawns (nothing).
void EffectsModule::HandleCrashingTrail(ActiveRaceCarData&, f32, f32, const RaceCarState*, EActiveRaceCarIndex,
                                        const RwRGBAReal&, const Attrib::Gen::debrisparams&)
{
    static bool sbLogged = false;
    LogNotReconstructed(sbLogged, "EffectsModule::HandleCrashingTrail");
}
"""

# 10 cases x 7 checks (see FxCrashVfxCrashTrail.cpp)
NUMERIC_CHECKS = 10 * 7


class RootTree(Tree):
    def __init__(self, rev=None, root=None):
        super().__init__(rev)
        self.root = Path(root) if root else None

    def read(self, relative):
        if self.root is not None and (self.root / relative).exists():
            return (self.root / relative).read_text(encoding="utf-8-sig").replace("\r\n", "\n")
        try:
            return super().read(relative).replace("\r\n", "\n")
        except FileNotFoundError:
            return ""


def trail_region(effects):
    """The trail region (banner through the end of the handler), or None when the revision has none."""
    if REGION_MARK not in effects:
        return None
    start = effects.rfind("\n", 0, effects.index(REGION_MARK))
    start = effects.rfind("\n", 0, start) + 1          # the banner's opening rule line
    handler = definition(effects[start:], SIGNATURE)
    return effects[start:effects.index(handler, start) + len(handler)]


def helpers(effects):
    texts = []
    for signature in HELPERS:
        try:
            texts.append(definition(effects, signature))
        except ValueError:
            pass
    return "\n".join(texts) + "\n"


def wiring(tree):
    effects = tree.read(EFFECTS_CPP)
    try:
        handler = code_only(definition(effects, SIGNATURE))
    except ValueError:
        handler = ""
    yield ("HandleCrashingTrail runs its body (no NOT RECONSTRUCTED announcement): the five debris arrays' SpawnDebris "
           "and the impact smoke's SpawnSimple",
           handler != "" and "LogNotReconstructed" not in handler and "mParticleModule.SpawnDebris(" in handler
           and "mParticleModule.SpawnSimple(" in handler and "eParticleArray_ImpactSmoke" in handler)
    consts = code_only(effects)
    wanted = (("KF_CRASH_TRAIL_INHERIT_MIN", "0.1f", "flt_82004014"),
              ("KF_CRASH_TRAIL_INHERIT_RANGE", "0.3f", "flt_82004740"),
              ("KF_CRASH_TRAIL_SMOKE_THRESHOLD", "0.0f", "flt_82001CC0"),
              ("KF_CRASH_TRAIL_SMOKE_ALPHA", "1.0f", "flt_82001C98"))
    yield ("the trail literals are the image's, each cited by address (flt_82004014 0.1 / flt_82004740 0.3 inherited "
           "share, flt_82001CC0 the smoke threshold, flt_82001C98 its alpha)",
           all(re.search(r"const f32 %s\s*=\s*%s;" % (name, re.escape(value)), consts) for name, value, _ in wanted)
           and all(re.search(r"const f32 %s\s*=\s*%s;\s*//\s*%s" % (name, re.escape(value), address), effects)
                   for name, value, address in wanted))
    yield ("the debrisparams words are the handler's loads (Trail_VelocityMin / Max 0x10 / 0x14, the smoke's 0x48 / "
           "0x18 / 0x30, start / max speed 0x74 / 0x78, and per array rate / size min / size max / threshold)",
           all(re.search(pattern, consts) for pattern in (
               r"KU_TRAIL_VELOCITY_MIN\s*=\s*0x10;", r"KU_TRAIL_VELOCITY_MAX\s*=\s*0x14;",
               r"KU_TRAIL_SMOKE_SIZE_MIN\s*=\s*0x18;", r"KU_TRAIL_SMOKE_SIZE_MAX\s*=\s*0x30;",
               r"KU_TRAIL_SMOKE_RATE\s*=\s*0x48;", r"KU_TRAIL_START_SPEED\s*=\s*0x74;", r"KU_TRAIL_MAX_SPEED\s*=\s*0x78;",
               r"\{\s*0x5C,\s*0x2C,\s*0x44,\s*0x70\s*\}", r"\{\s*0x4C,\s*0x1C,\s*0x34,\s*0x60\s*\}",
               r"\{\s*0x58,\s*0x28,\s*0x40,\s*0x6C\s*\}", r"\{\s*0x50,\s*0x20,\s*0x38,\s*0x64\s*\}",
               r"\{\s*0x54,\s*0x24,\s*0x3C,\s*0x68\s*\}")))
    try:
        caller = code_only(definition(effects, "void EffectsModule::UpdateActiveRaceCars("))
    except ValueError:
        caller = ""
    yield ("UpdateActiveRaceCars hands the trail the car colour and the player / AI debrisparams "
           "(0x8229E2E4..0x8229E328: mCrashingDebrisParams for the player's car, mAIRaceCarCrashingTrailDebris else)",
           re.search(r"\(\s*leIndex\s*==\s*lePlayerIndex\s*\)\s*\?\s*mCrashingDebrisParams\s*:\s*"
                     r"mAIRaceCarCrashingTrailDebris", caller) is not None
           and re.search(r"HandleCrashingTrail\([^;]*GetRaceCarColour\(\s*leIndex\s*\)[^;]*lrTrailDebris\s*\)", caller,
                         flags=re.S) is not None)


def numeric(tree):
    effects = tree.read(EFFECTS_CPP)
    region = trail_region(effects)
    if region is None:
        print("NUMERIC: this revision has no trail region -- measuring the announcing stand-in")
        region = NO_TRAIL_BODY
    return compile_and_run(Path(__file__).with_name("FxCrashVfxCrashTrail.cpp"), "fxcrashvfx_trail_body.inc",
                           region + "\n", "FxCrashVfxCrashTrail", extra_sources=(RANDOM_CPP,),
                           extra_files={"fxcrashvfx_trail_helpers.inc": helpers(effects)})


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", default=None, help="b5 revision to test (default: the working tree)")
    parser.add_argument("--root", default=None, help="a shadow tree root whose src/ files take precedence")
    args = parser.parse_args()
    tree = RootTree(args.rev, args.root)
    checks = list(wiring(tree))
    result = numeric(tree)
    return report("run_fxcrashvfx_crash_trail", checks, result, NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
