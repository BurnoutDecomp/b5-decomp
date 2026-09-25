"""FX-CRASHVFX (crash parity 2026-09-25, item 5): THE SHOWTIME BOUNCE -- EffectsModule::HandleShowtimeTrafficBounce
@0x82292808.

What a player sees on the console when a Showtime bounce lands on a car: the wreck bursts where it hit -- the car's
own debris, a spray of glass, a shower of sparks and the 'ExploShort' explosion, at most once every half second.
HandleGameActions hands the handler every JUST_BOUNCED record (FX-SHOWTIME2 posts them since b5 f2e66b94). Before this
fix it announced itself NOT RECONSTRUCTED and the bounce produced none of it.

  1. WIRING -- the handler runs its body (no announcement) into FireDebrisBurst, BurstAreaEmitParticles (glass),
     DoSparkShower with gSparkShowerControllerShowtimeBounce and the ExploShort LION effect; the literals and the
     controller's words are the image's, each cited; HandleGameActions hands it the typed record.
  2. NUMERIC -- tests/FxCrashVfxShowtimeBounce.cpp compiles the PRODUCTION code onto a fixture and compares every
     call, the glass pieces, the LION slot, the module's bookkeeping, the replay layout and mRandom bit for bit with
     0x82292808 (and the real BurstAreaEmitParticles) run on emu64
     (scratch/CRASHPARITY_0922/fxcrashvfx_vmxemu/gen_bounce_data.py; 16 cases x 9 checks).

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxcrashvfx_showtime_bounce.py
                                                                   [--rev <b5 rev>] [--root <shadow tree root>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import REPO, Tree, code_only, compile_and_run, definition, report

EFFECTS_CPP = "src/GameSource/Effects/EffectsModule.cpp"
UTILS_CPP = "src/GameSource/Effects/BrnEffectsUtils.cpp"
SHADOW_HEADERS = ("src/GameSource/Effects/Particles/ParticleModule.h",
                  "src/GameSource/Effects/BrnEffectsUtils.h")
RANDOM_CPP = REPO / "src/GameShared/GameClasses/Numeric/CgsRandom.cpp"
HASH_CPP = REPO / "src/GameSource/Effects/Particles/BrnParticleDescription.cpp"
SIGNATURE = "void EffectsModule::HandleShowtimeTrafficBounce("
BURST_SIGNATURE = "void EffectsModule::BurstAreaEmitParticles("
REGION_MARK = "THE SHOWTIME BOUNCE -- FX-CRASHVFX"
HELPERS = ("f32 Dot3(", "f32 Vnmsub(", "f32 RefinedRsqrt(", "f32 RefinedRecip(", "f32 GuardedLength3(",
           "Vector3 Scale4(", "Vector3 CrossPermuted(", "Vector3 AxisX(", "Vector3 AxisY(", "Vector3 Add4(",
           "Vector3 Sub4(", "Vector3 Mul4(", "Vector3 MaddSplat4(", "Vector3 MakeVector3(",
           "Vector3 RandomUnitVector(", "VecFloat Splat(", "const u8* DebrisParamsLayout(",
           "bool IsReplayPlayback(", "bool IsReplayRecording(")
CONSTANT_PATTERNS = (r"^[ \t]*const u32 KU_DEBRIS_EMITTER_HALF_EXTENTS\s*=[^;]+;",
                     r"^[ \t]*const f32 KF_BURST_AREA_[A-Z_]+\s*=[^;]+;")
STRUCTS = ("struct SparkShowerArgs\n{", "struct SparkShowerController\n{")
CONTROLLER = "const SparkShowerController gSparkShowerControllerShowtimeBounce"
TRIG_CONSTANTS = ("KF_ONE_OVER_TWO_PI", "KF_SIN_PHASE", "KF_COS_PHASE", "KF_FOLD_PERIOD", "KF_FOLD_BIAS",
                  "KF_POLY_C1", "KF_POLY_C3", "KF_POLY_C5")
UTILS = ("void Vector3Randomiser::Prepare(", "Vector3 Vector3Randomiser::RandomiseXYZ(", "void SinCosCycles(")
NO_BOUNCE_BODY = """
// [fixture] this revision's HandleShowtimeTrafficBounce is the old untyped stub: a stand-in that announces, so the
// fixture compiles and measures what the revision does (nothing).
void EffectsModule::HandleShowtimeTrafficBounce(const BrnGameState::GameStateModuleIO::JustBouncedAction*,
                                                const EffectsIO::InputBuffer*)
{
    static bool sbLogged = false;
    LogNotReconstructed(sbLogged, "EffectsModule::HandleShowtimeTrafficBounce");
}
"""

# 16 cases x 9 checks (see FxCrashVfxShowtimeBounce.cpp)
NUMERIC_CHECKS = 16 * 9


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


def optional_definitions(source, signatures):
    texts = []
    for signature in signatures:
        try:
            texts.append(definition(source, signature))
        except ValueError:
            pass
    return texts


def bounce_region(effects):
    """The bounce region (banner through the end of the handler), or None when the revision has none."""
    if REGION_MARK not in effects:
        return None
    start = effects.rfind("\n", 0, effects.index(REGION_MARK))
    start = effects.rfind("\n", 0, start) + 1          # the banner's opening rule line
    handler = definition(effects[start:], SIGNATURE)
    return effects[start:effects.index(handler, start) + len(handler)]


def pieces(tree):
    effects = tree.read(EFFECTS_CPP)
    utils = tree.read(UTILS_CPP)
    helpers = optional_definitions(effects, HELPERS)
    for pattern in CONSTANT_PATTERNS:
        helpers += [m.group(0).strip() for m in re.finditer(pattern, effects, flags=re.M)]
    helpers += [text + ";" for text in optional_definitions(effects, STRUCTS)]
    helpers += [text + ";" for text in optional_definitions(effects, (CONTROLLER,))]
    trig = []
    for name in TRIG_CONSTANTS:
        match = re.search(r"^[ \t]*const\s+f32\s+%s\s*=\s*[^;]+;" % name, utils, flags=re.M)
        if match:
            trig.append(match.group(0).strip())
    trig += optional_definitions(utils, ("inline f32 Cos4_UnitCycles(",))
    utils_text = "namespace\n{\n" + "\n".join(trig) + "\n}\n" + "\n".join(optional_definitions(utils, UTILS)) + "\n"
    burst = "\n".join(optional_definitions(effects, (BURST_SIGNATURE,))) + "\n"
    return {"fxcrashvfx_bounce_helpers.inc": "\n".join(helpers) + "\n",
            "fxcrashvfx_bounce_utils.inc": utils_text,
            "fxcrashvfx_bounce_burst.inc": burst}


def wiring(tree):
    effects = tree.read(EFFECTS_CPP)
    try:
        handler = code_only(definition(effects, SIGNATURE))
    except ValueError:
        handler = ""
    yield ("HandleShowtimeTrafficBounce runs its body (no NOT RECONSTRUCTED announcement): FireDebrisBurst, "
           "BurstAreaEmitParticles with eDebrisArray_Glass, DoSparkShower with gSparkShowerControllerShowtimeBounce, "
           "the ExploShort LION effect",
           handler != "" and "LogNotReconstructed" not in handler and "mParticleModule.FireDebrisBurst(" in handler
           and re.search(r"BurstAreaEmitParticles\([^;]*eDebrisArray_Glass", handler, flags=re.S) is not None
           and re.search(r"DoSparkShower\(\s*gSparkShowerControllerShowtimeBounce", handler) is not None
           and "StartLionEffect(" in handler and "KAC_SHOWTIME_BOUNCE_EFFECT" in handler)
    consts = code_only(effects)
    wanted = (("KF_SHOWTIME_BOUNCE_MIN_INTERVAL", "0.5f", "flt_82001DA0"),
              ("KF_SHOWTIME_BOUNCE_DEBRIS_SCALE", "1.0f", "flt_82001C98"),
              ("KF_SHOWTIME_BOUNCE_GLASS_SIZE_MIN", "1.25f", "flt_820092CC"),
              ("KF_SHOWTIME_BOUNCE_GLASS_SIZE_MAX", "2.75f", "flt_82013104"),
              ("KF_SHOWTIME_BOUNCE_GLASS_DENSITY", "100.0f", "flt_820049E0"),
              ("KF_SHOWTIME_BOUNCE_GROUND_Y", "-1000.0f", "flt_8200D4F8"),
              ("KF_VECFLOAT_TWOPI", "6.28318548f", "unk_82FAB8D0"))
    yield ("the bounce literals are the image's, each cited by address (flt_82001DA0 0.5 s, flt_82001C98 1.0, "
           "flt_820092CC / flt_82013104 1.25 / 2.75, flt_820049E0 100, flt_8200D4F8 -1000, unk_82FAB8D0 2 pi)",
           all(re.search(r"const f32 %s\s*=\s*%s;" % (name, re.escape(value)), consts) for name, value, _ in wanted)
           and all(re.search(r"const f32 %s\s*=\s*%s;\s*//\s*%s" % (name, re.escape(value), address), effects)
                   for name, value, address in wanted)
           and re.search(r"const s32 KI_SHOWTIME_BOUNCE_SMALL_BURST\s*=\s*150;", consts) is not None
           and re.search(r"const s32 KI_SHOWTIME_BOUNCE_LARGE_BURST\s*=\s*300;", consts) is not None)
    try:
        controller = code_only(definition(effects, CONTROLLER))
    except ValueError:
        controller = ""
    numbers = re.findall(r"-?\d+\.\d+f", controller)
    yield ("gSparkShowerControllerShowtimeBounce is the image's (unk_82CDB020, CRT thunk 0x82C4A260 run on emu64): "
           "small (-45, 45, -45, 45) (8, 16, 0.8, 1.2) (0.5, 1.25, 0.1, 0.1), large (-80, 80, -80, 80) (8, 32, 0.8, 1.2) "
           "(0.75, 1.75, 0.5, 0.5), 0.0, 44.694443, eSparkArray_Crashing",
           numbers == ["-45.0f", "45.0f", "-45.0f", "45.0f", "8.0f", "16.0f", "0.800000012f", "1.20000005f", "0.5f",
                       "1.25f", "0.100000001f", "0.100000001f", "-80.0f", "80.0f", "-80.0f", "80.0f", "8.0f", "32.0f",
                       "0.800000012f", "1.20000005f", "0.75f", "1.75f", "0.5f", "0.5f", "0.0f", "44.6944427f"]
           and "eSparkArray_Crashing" in controller
           and "unk_82CDB020 <- thunk 0x82C4A260" in effects)
    try:
        actions = code_only(definition(effects, "void EffectsModule::HandleGameActions("))
    except ValueError:
        actions = ""
    yield ("HandleGameActions hands case 144 the record itself, typed (`mr r4, r28` at 0x822972E4)",
           re.search(r"case E_ACTION_JUST_BOUNCED:\s*HandleShowtimeTrafficBounce\(\s*reinterpret_cast<const "
                     r"JustBouncedAction\*>\(lpEvent\)", actions) is not None)


def numeric(tree):
    effects = tree.read(EFFECTS_CPP)
    region = bounce_region(effects)
    if region is None:
        print("NUMERIC: this revision has no bounce region -- measuring the announcing stand-in")
        region = NO_BOUNCE_BODY
    shadow = {relative: tree.read(relative) for relative in SHADOW_HEADERS if tree.read(relative)}
    return compile_and_run(Path(__file__).with_name("FxCrashVfxShowtimeBounce.cpp"), "fxcrashvfx_bounce_body.inc",
                           region + "\n", "FxCrashVfxShowtimeBounce", shadow=shadow,
                           extra_sources=(RANDOM_CPP, HASH_CPP), extra_files=pieces(tree))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", default=None, help="b5 revision to test (default: the working tree)")
    parser.add_argument("--root", default=None, help="a shadow tree root whose src/ files take precedence")
    args = parser.parse_args()
    tree = RootTree(args.rev, args.root)
    checks = list(wiring(tree))
    result = numeric(tree)
    return report("run_fxcrashvfx_showtime_bounce", checks, result, NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
