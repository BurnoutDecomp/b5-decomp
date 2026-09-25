"""FX-CRASHVFX (crash parity 2026-09-25, C2): THE JUMP-LANDING SPARKS -- BrnEffects::JumpStateMachine::FireWheelSparks
@0x82299670 and its callee EffectsModule::FireJumpSparks @0x822969E0 (with gSparkShowerControllerJumpSparks,
unk_82CDB1E0 <- thunk 0x82C4A790).

What a player sees on the console when a car lands a jump fast: a spray of sparks off the rear axle -- one shower
off each half of it, framed on the ground and on the car's slide along it -- for as long as the machine holds its
FiringSparks state. Before this work FireWheelSparks announced itself PARKED (its callee had no declaration and no
body), so a landing threw dust and debris but never a spark.

  1. WIRING -- FireWheelSparks has its body (no PARKED announcement); FireJumpSparks is declared public in
     EffectsModule.h and bodied; its literals and the controller are the image's, each cited; the EffectsModule.cpp
     banner no longer lists the prop-locator VFX as not reconstructed.
  2. NUMERIC -- tests/FxCrashVfxJumpSparks.cpp compiles the PRODUCTION FireWheelSparks and FireJumpSparks (with the
     vector idioms, constants and controller they are built from) onto a fixture and compares every DoSparkShower
     call -- controller, lerp, emitter frame, velocity to inherit, time, ground height, count -- the ring and the
     asserts, bit for bit, with 0x82299670 run on emu64 (scratch/CRASHPARITY_0922/fxcrashvfx_vmxemu/
     gen_jumpsparks_data.py; 26 cases x 4 checks), plus the controller's words against the thunk's (1 check).

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxcrashvfx_jump_sparks.py
                                                                   [--rev <b5 rev>] [--root <shadow tree root>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import REPO, Tree, code_only, compile_and_run, definition, report

EFFECTS_CPP = "src/GameSource/Effects/EffectsModule.cpp"
EFFECTS_H = "src/GameSource/Effects/EffectsModule.h"
JUMP_CPP = "src/GameSource/Effects/Jump/JumpStateMachine.cpp"
SHADOW_HEADERS = ("src/GameSource/Effects/Jump/JumpStateMachine.h",)
RANDOM_CPP = REPO / "src/GameShared/GameClasses/Numeric/CgsRandom.cpp"

FIRE_JUMP_SPARKS = "void EffectsModule::FireJumpSparks("
FIRE_WHEEL_SPARKS = "void JumpStateMachine::FireWheelSparks("
CONTROLLER = "const SparkShowerController gSparkShowerControllerJumpSparks ="
STRUCTS = ("struct SparkShowerArgs\n{", "struct SparkShowerController\n{")
HELPERS = ("f32 Dot3(", "f32 Vnmsub(", "f32 RefinedRsqrt(", "f32 RefinedRecip(", "f32 GuardedLength3(",
           "Vector3 Scale4(", "VecFloat Splat(", "u32 FctidzLowWord(", "Vector3 CrossPermuted(", "f32 LayoutFloat(",
           "const Attrib::RefSpec& VfxSurfaceRef(")

NUMERIC_CHECKS = 26 * 4 + 1


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


def body(source, signature):
    try:
        return code_only(definition(source, signature))
    except ValueError:
        return ""


def optional_definitions(source, signatures):
    texts = []
    for signature in signatures:
        try:
            texts.append(definition(source, signature))
        except ValueError:
            pass
    return texts


def constants_for(effects, text):
    """Every `const <type> K*_NAME = ...;` the text names, transitively, in source order."""
    seen, found = set(), {}
    pending = set(re.findall(r"\bK[A-Z]{1,2}_[A-Z0-9_]+\b", code_only(text)))
    while pending:
        name = pending.pop()
        if name in seen:
            continue
        seen.add(name)
        match = re.search(r"^[ \t]*const\s+[\w:]+\s+%s\s*=\s*[^;]+;" % re.escape(name), effects, flags=re.M)
        if match:
            found[name] = (match.start(), match.group(0).strip())
            pending |= set(re.findall(r"\bK[A-Z]{1,2}_[A-Z0-9_]+\b", code_only(match.group(0)))) - seen
    return [found[name][1] for name in sorted(found, key=lambda n: found[n][0])]


def wiring(tree):
    jump = tree.read(JUMP_CPP)
    wheel = body(jump, FIRE_WHEEL_SPARKS)
    yield ("FireWheelSparks has its body, no PARKED announcement: both REAR wheels on the ground, two RandomVecFloat "
           "lerps (A = t1 * 0.5, B = fma(0.5, t2, 0.5)), FireJumpSparks off wheel 2's then wheel 3's tag (0x82299670)",
           wheel != "" and "PARKED" not in wheel and "JumpDiagText" not in wheel
           and "maWheels[2]" in wheel and "maWheels[3]" in wheel and wheel.count("RandomVecFloat()") == 2
           and wheel.count("FireJumpSparks(") == 2
           and wheel.find("lRearLeftWheel.mRoadContact.mCollisionTag") < wheel.find("lRearRightWheel.mRoadContact.mCollisionTag")
           and "GetGroundPositionY()" in wheel)

    header = code_only(tree.read(EFFECTS_H))
    effects = tree.read(EFFECTS_CPP)
    fire = body(effects, FIRE_JUMP_SPARKS)
    public = header.find("SurfaceList()")
    private = header.find("private:", public) if public >= 0 else -1
    declared = header.find("void FireJumpSparks(")
    yield ("EffectsModule::FireJumpSparks is declared PUBLIC (the DWARF's last public member) and bodied: the +0x54 "
           "spark-scale gate, the frame, the speed gate, one RandomFloat, DoSparkShower(gSparkShowerControllerJumpSparks)",
           0 <= public < declared < private and fire != ""
           and "KU_VFX_SPARK_SCALE" in fire and "KF_MIN_EFFECT_SCALE" in fire and "CrossPermuted(" in fire
           and "RefinedRecip(" in fire and fire.count("mRandom.RandomFloat()") == 1
           and "DoSparkShower(gSparkShowerControllerJumpSparks," in fire and "FctidzLowWord(" in fire)

    consts = code_only(effects)
    wanted = (("KF_JUMP_SPARKS_MIN_SPEED", "4.46944427f", "flt_82013720"),
              ("KF_JUMP_SPARKS_PER_METRE_RANGE", "40.0f", "flt_82004D0C"),
              ("KF_JUMP_SPARKS_PER_METRE_MIN", "50.5f", "flt_82013724"))
    yield ("the literals are the image's, each cited (flt_82013720 4.4694443, flt_82004D0C 40, flt_82013724 50.5), and "
           "gSparkShowerControllerJumpSparks is unk_82CDB1E0 as thunk 0x82C4A790 leaves it",
           all(re.search(r"const f32 %s\s*=\s*%s;\s*//\s*%s" % (name, re.escape(value), address), effects)
               for name, value, address in wanted)
           and CONTROLLER in effects and "0x82C4A790" in effects and "unk_82CDB1E0" in effects
           and re.search(r"gSparkShowerControllerJumpSparks\s*=\s*\{\s*\{\s*\{\s*2\.0f,\s*10\.0f,\s*150\.0f,\s*210\.0f\s*\}",
                         consts) is not None)

    banner = effects[:effects.find("namespace BrnEffects")]
    yield ("the EffectsModule.cpp banner no longer lists the prop-locator VFX as not reconstructed (item 6b)",
           "for it) and the prop-locator VFX." not in banner)


def numeric(tree):
    effects = tree.read(EFFECTS_CPP)
    jump = tree.read(JUMP_CPP)
    try:
        wheel = definition(jump, FIRE_WHEEL_SPARKS)
    except ValueError as error:
        print("NUMERIC: cannot build -- " + str(error))
        return None
    structs = [definition(effects, s) + ";" for s in STRUCTS if s in effects]
    controller = [definition(effects, CONTROLLER) + ";"] if CONTROLLER in effects else []
    helpers = optional_definitions(effects, HELPERS)
    fire = optional_definitions(effects, (FIRE_JUMP_SPARKS,))
    consts = constants_for(effects, "\n".join(controller + helpers + fire))
    config = "#define FXJS_HAS_FIRE_JUMP_SPARKS %d\n#define FXJS_HAS_CONTROLLER %d\n" % (1 if fire else 0,
                                                                                         1 if controller else 0)
    if not fire:
        print("NUMERIC: this revision has no EffectsModule::FireJumpSparks; its FireWheelSparks runs alone")
    shadow = {relative: tree.read(relative) for relative in SHADOW_HEADERS if tree.read(relative)}
    extra = {
        "fxcrashvfx_jumpsparks_config.inc": config,
        "fxcrashvfx_jumpsparks_structs.inc": "\n".join(structs) + "\n",
        "fxcrashvfx_jumpsparks_consts.inc": "\n".join(consts + controller + helpers) + "\n",
        "fxcrashvfx_jumpsparks_body.inc": "\n".join(fire) + "\n",
    }
    return compile_and_run(Path(__file__).with_name("FxCrashVfxJumpSparks.cpp"), "fxcrashvfx_jumpsparks_wheel.inc",
                           wheel + "\n", "FxCrashVfxJumpSparks", shadow=shadow, extra_sources=(RANDOM_CPP,),
                           extra_files=extra)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", default=None, help="b5 revision to test (default: the working tree)")
    parser.add_argument("--root", default=None, help="a shadow tree root whose src/ files take precedence")
    args = parser.parse_args()
    tree = RootTree(args.rev, args.root)
    checks = list(wiring(tree))
    result = numeric(tree)
    return report("run_fxcrashvfx_jump_sparks", checks, result, NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
