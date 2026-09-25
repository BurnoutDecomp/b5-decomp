"""FX-CRASHVFX (crash parity 2026-09-25, item 2): THE GLASS SMASH -- EffectsModule::HandleGlassSmashEventsForAllCars
@0x82297420 and EffectsModule::BurstAreaEmitParticles @0x82292160.

What a player sees when a window breaks on the console: the pane's glass debris showering out of the frame (a
traffic car's, or a race car's while it crashes -- 320 pieces per unit of pane area) and up to three
'Glass_shattering' LION effects along the pane. Before this fix the drain announced itself NOT RECONSTRUCTED and
none of it existed.

  1. WIRING -- the drain runs (no announcement) and reaches BurstAreaEmitParticles with eDebrisArray_Glass,
     FireGlassEffect and UpdateVehicleEffectPositions; the literals are the image's, each cited by address; the
     TrigBaseFunctions5 sin/cos is fused (vmaddfp) as on the console.
  2. NUMERIC -- tests/FxCrashVfxGlass.cpp compiles the PRODUCTION bodies onto a fixture and compares them, bit for
     bit, with the console's own outputs in tests/FxCrashVfxGlassData.h: 0x82297420 / 0x82292160 and their callees
     run on emu64 by scratch/CRASHPARITY_0922/fxcrashvfx_vmxemu/gen_glass_data.py (19 cases x 7 checks; the nineteenth: a NaN size max, which the
     console's first assert SKIPS -- `fcmpu ; bge` at 0x822921B0 takes the unordered compare).

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxcrashvfx_glass.py [--rev <b5 rev>]
                                                                        [--root <shadow tree root>]
(--root reads any file present under <root>/src/... in place of the working tree's: the local compile gate for an
edit that has not reached the shared tree yet.)
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import REPO, Tree, code_only, compile_and_run, definition, report

EFFECTS_CPP = "src/GameSource/Effects/EffectsModule.cpp"
UTILS_CPP = "src/GameSource/Effects/BrnEffectsUtils.cpp"
SHADOW_HEADERS = ("src/GameSource/Effects/ActiveRaceCarData.h",
                  "src/GameSource/Effects/BrnEffectsUtils.h",
                  "src/GameSource/Physics/DeformationManager/SharedIO/BrnDeformationOutputInterface.h")
RANDOM_CPP = REPO / "src/GameShared/GameClasses/Numeric/CgsRandom.cpp"

GLASS_SIGNATURE = "void EffectsModule::HandleGlassSmashEventsForAllCars("
BURST_SIGNATURE = "void EffectsModule::BurstAreaEmitParticles("
REGION_MARK = "THE GLASS SMASH -- FX-CRASHVFX"
HELPERS = ("f32 Dot3(", "f32 Vnmsub(", "f32 RefinedRsqrt(", "f32 RefinedRecip(", "f32 GuardedLength3(",
           "Vector3 Scale4(", "Vector3 CrossPermuted(", "Vector3 AxisY(", "bool IsReplayPlayback(",
           "bool IsReplayRecording(")
TRIG_CONSTANTS = ("KF_ONE_OVER_TWO_PI", "KF_SIN_PHASE", "KF_COS_PHASE", "KF_FOLD_PERIOD", "KF_FOLD_BIAS",
                  "KF_POLY_C1", "KF_POLY_C3", "KF_POLY_C5")
UTILS = ("void Vector3Randomiser::Prepare(", "Vector3 Vector3Randomiser::RandomiseXYZ(", "void SinCosCycles(")
NO_BURST_BODY = """
// [fixture] this revision has no BurstAreaEmitParticles body: an empty stand-in so it compiles (and measures 0).
void EffectsModule::BurstAreaEmitParticles(Vector3*, Vector3, Vector3, BrnParticle::Native::EDebrisArrayID, f32, f32,
                                           f32, f32)
{
}
"""

# 19 cases x 7 checks (see FxCrashVfxGlass.cpp)
NUMERIC_CHECKS = 19 * 7


class RootTree(Tree):
    """The working tree (or --rev) with an optional shadow root whose files take precedence."""

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


def normalised(tree, relative):
    return tree.read(relative).replace("\r\n", "\n")


def optional_definitions(source, signatures):
    texts = []
    for signature in signatures:
        try:
            texts.append(definition(source, signature))
        except ValueError:
            pass
    return texts


def glass_region(effects):
    """The glass region (banner through the end of HandleGlassSmashEventsForAllCars), or the revision's lone drain
    (the parent's announcing stub) plus an empty BurstAreaEmitParticles when there is no such region."""
    drain = definition(effects, GLASS_SIGNATURE)
    end = effects.index(drain) + len(drain)
    if REGION_MARK in effects:
        start = effects.rfind("\n", 0, effects.index(REGION_MARK)) + 1
        return effects[start:end], True
    body = drain
    if BURST_SIGNATURE not in effects:
        body += NO_BURST_BODY
    return body, False


def trig_pieces(utils):
    texts = []
    for name in TRIG_CONSTANTS:
        match = re.search(r"^[ \t]*const\s+f32\s+%s\s*=\s*[^;]+;" % name, utils, flags=re.M)
        if match:
            texts.append(match.group(0).strip())
    texts += optional_definitions(utils, ("inline f32 Cos4_UnitCycles(",))
    return texts


def pieces(tree):
    effects = normalised(tree, EFFECTS_CPP)
    utils = normalised(tree, UTILS_CPP)
    region, landed = glass_region(effects)
    helpers = optional_definitions(effects, HELPERS)
    utils_text = "namespace\n{\n" + "\n".join(trig_pieces(utils)) + "\n}\n" + "\n".join(
        optional_definitions(utils, UTILS)) + "\n"
    return {
        "fxcrashvfx_glass_helpers.inc": "\n".join(helpers) + "\n",
        "fxcrashvfx_glass_utils.inc": utils_text,
    }, region, landed


def wiring(tree):
    effects = normalised(tree, EFFECTS_CPP)
    consts = code_only(effects)
    try:
        drain = code_only(definition(effects, GLASS_SIGNATURE))
    except ValueError:
        drain = ""
    yield ("HandleGlassSmashEventsForAllCars runs its body (no NOT RECONSTRUCTED announcement): the glass debris "
           "burst, the shatter effects, the effect re-seat",
           drain != "" and "LogNotReconstructed" not in drain
           and re.search(r"BurstAreaEmitParticles\([^;]*eDebrisArray_Glass", drain, flags=re.S) is not None
           and "FireGlassEffect(" in drain and "UpdateVehicleEffectPositions(" in drain)
    wanted = (("KF_GLASS_DEBRIS_SIZE_MIN", "1.0f", "flt_82001C98"),
              ("KF_GLASS_DEBRIS_SIZE_MAX", "1.75f", "flt_82004F68"),
              ("KF_GLASS_DEBRIS_DENSITY", "320.0f", "flt_820137D4"),
              ("KF_GLASS_SHATTER_PER_AREA", "3.0f", "flt_8200DD24"),
              ("KF_GLASS_SHATTER_MAX", "3.0f", "flt_8200DD24"),
              ("KF_GLASS_SHATTER_ANGLE_RANGE", "6.28f", "flt_820137D8"),
              ("KF_GLASS_SHATTER_ANGLE_OFFSET", "3.14f", "flt_820137DC"),
              ("KF_BURST_AREA_INHERIT_SPEED_CLAMP", "40.0f", "flt_82004D0C"),
              ("KF_BURST_AREA_SPEED_MIN_BASE", "2.0f", "flt_82001D9C"),
              ("KF_BURST_AREA_SPEED_MIN_PER_MPS", "0.01f", "flt_82002138"),
              ("KF_BURST_AREA_SPEED_MAX_BASE", "10.0f", "flt_82004A20"),
              ("KF_BURST_AREA_SPEED_MAX_PER_MPS", "0.15f", "flt_82004E58"),
              ("KF_BURST_AREA_INHERIT_MIN", "0.4f", "flt_82011C18"),
              ("KF_BURST_AREA_INHERIT_MAX", "0.6f", "flt_82004D00"),
              ("KF_BURST_AREA_RADIAL_MIN", "3.0f", "flt_8200DD24"),
              ("KF_BURST_AREA_RADIAL_MAX", "8.0f", "flt_82004C88"))
    yield ("the glass / burst literals are the image's, each cited by address (flt_820137D4 320, flt_82004F68 1.75, "
           "flt_8200DD24 3, flt_820137D8 / DC 6.28 / 3.14, flt_82004D0C 40, flt_82002138 / 82004E58, ...)",
           all(re.search(r"const f32 %s\s*=\s*%s;" % (name, re.escape(value)), consts) for name, value, _ in wanted)
           and all(re.search(r"const f32 %s\s*=\s*%s;\s*//\s*%s" % (name, re.escape(value), address), effects)
                   for name, value, address in wanted))
    utils = code_only(normalised(tree, UTILS_CPP))
    try:
        cos4 = code_only(definition(normalised(tree, UTILS_CPP), "inline f32 Cos4_UnitCycles("))
        sincos = code_only(definition(normalised(tree, UTILS_CPP), "void SinCosCycles("))
    except ValueError:
        cos4 = sincos = ""
    yield ("the TrigBaseFunctions5 sin/cos is FUSED as on the console: the phase and the two polynomial steps are "
           "vmaddfp (std::fma)",
           cos4.count("std::fma(") == 2 and sincos.count("std::fma(") == 2 and bool(utils))
    # EffectsModule::PreRenderUpdate @0x8227FE10: LockForWrite, memcpy(GetBufferCrashTriangleCache(),
    # &mCrashTriangleCache, 0x1E80) (0x8227FE30..0x8227FE50), UnlockForWrite, then the particle module's slot 72.
    # The GenerateDispatchLists stand-in dropped the copy: the debris jobs (and so the glass debris) never collided.
    try:
        lists = code_only(definition(effects, "void EffectsModule::GenerateDispatchLists("))
    except ValueError:
        lists = ""
    copy = lists.find("GetBufferCrashTriangleCache() = mCrashTriangleCache")
    prerender = lists.find("mParticleModule.PreRenderUpdate(")
    yield ("the effects module publishes its crash triangle cache to the dispatch buffer under the write lock, before "
           "the particle module's PreRenderUpdate (EffectsModule::PreRenderUpdate @0x8227FE10's memcpy of 0x1E80)",
           copy >= 0 and prerender > copy and lists.rfind("LockForWrite()", 0, copy) > lists.rfind("UnlockForWrite()", 0, copy)
           and 0 <= lists.find("UnlockForWrite()", copy) < prerender)


def numeric(tree):
    try:
        incs, region, landed = pieces(tree)
    except ValueError as error:
        print("NUMERIC: cannot build -- " + str(error))
        return None
    if not landed:
        print("NUMERIC: this revision has no glass region -- measuring its lone HandleGlassSmashEventsForAllCars")
    shadow = {relative: tree.read(relative) for relative in SHADOW_HEADERS if tree.read(relative)}
    return compile_and_run(Path(__file__).with_name("FxCrashVfxGlass.cpp"), "fxcrashvfx_glass_body.inc",
                           region + "\n", "FxCrashVfxGlass", shadow=shadow, extra_sources=(RANDOM_CPP,),
                           extra_files=incs)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", default=None, help="b5 revision to test (default: the working tree)")
    parser.add_argument("--root", default=None, help="a shadow tree root whose src/ files take precedence")
    args = parser.parse_args()
    tree = RootTree(args.rev, args.root)
    checks = list(wiring(tree))
    result = numeric(tree)
    return report("run_fxcrashvfx_glass", checks, result, NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
