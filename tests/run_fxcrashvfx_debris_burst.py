"""FX-CRASHVFX (crash parity 2026-09-25, item 3): THE CRASH DEBRIS BURST -- ParticleModule::HandleFireDebrisBurstEvent
@0x8229A660.

What a player sees on the console when a car crashes or is taken down: the car throws its debris -- painted pieces
in its own colour, shiny, dark and detailed ones -- some straight at the camera, the rest in a cone, all tumbling.
ParticleModule::FireDebrisBurst posts the record; this handler, on the dispatch thread, turns it into
BrnDebrisArray::SpawnDebris calls. Before this fix it announced itself NOT RECONSTRUCTED and dropped every record.

  1. WIRING -- the handler runs its body (no announcement), spawns into the debris arrays and colours through the
     DebrisColourRandomiser; its literals are the image's, each cited by address.
  2. NUMERIC -- tests/FxCrashVfxDebrisBurst.cpp compiles the PRODUCTION region onto a fixture and compares every
     SpawnDebris call, bit for bit, with the console's own (0x8229A660 interpreted on emu64 by
     scratch/CRASHPARITY_0922/fxcrashvfx_vmxemu/gen_fdb_data.py; 10 cases x 7 checks).

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxcrashvfx_debris_burst.py [--rev <b5 rev>]
                                                                             [--root <shadow tree root>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import REPO, Tree, code_only, compile_and_run, definition, report

EVENTS_CPP = "src/GameSource/Effects/Particles/ParticleModule_SparkEvents.cpp"
COLOUR_CPP = "src/GameSource/Effects/BrnEffectsDebrisColourRandomiser.cpp"
SHADOW_HEADERS = ("src/GameSource/Effects/BrnEffectsDebrisColourRandomiser.h",
                  "src/GameSource/Effects/Particles/BrnParticleModuleIO_EventTypes.h",
                  "src/GameSource/Effects/Particles/Native/BrnDebrisArray.h")
RANDOM_CPP = REPO / "src/GameShared/GameClasses/Numeric/CgsRandom.cpp"
SIGNATURE = "void ParticleModule::HandleFireDebrisBurstEvent("
REGION_MARK = "THE CRASH DEBRIS BURST -- ParticleModule::HandleFireDebrisBurstEvent"
CONSTS_START = "    static const f32 KF_SHOWER_REFLECTION_SCALE"
CONSTS_END = "    // The once-only announcement helper"

# 10 cases x 7 checks (see FxCrashVfxDebrisBurst.cpp)
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


def region(source):
    """The burst region (banner through the end of the handler), or the revision's lone handler."""
    handler = definition(source, SIGNATURE)
    end = source.index(handler) + len(handler)
    if REGION_MARK in source:
        start = source.rfind("\n", 0, source.index(REGION_MARK))
        start = source.rfind("\n", 0, start) + 1       # the banner's opening rule line
        return source[start:end], True
    return handler, False


def wiring(tree):
    source = tree.read(EVENTS_CPP)
    try:
        handler = code_only(definition(source, SIGNATURE))
    except ValueError:
        handler = ""
    yield ("HandleFireDebrisBurstEvent runs its body (no NOT RECONSTRUCTED announcement): the four burst arrays' "
           "SpawnDebris, coloured through the DebrisColourRandomiser",
           handler != "" and "LogSparkEventNotReconstructed" not in handler
           and ".SpawnDebris(" in handler and "DebrisColourRandomiser" in handler and ".Randomise(" in handler)
    consts = code_only(source)
    wanted = (("KF_DEBRIS_BURST_DEG_TO_RAD", "0.0174532924f", "flt_8200D964"),
              ("KF_DEBRIS_BURST_TWO_PI", "6.28318548f", "flt_8200D970"),
              ("KF_DEBRIS_BURST_CAMERA_SPEED", "7.0f", "flt_820054D0"),
              ("KF_DEBRIS_BURST_SECONDS_PER_M", "0.142857149f", "flt_82013AB4"),
              ("KF_DEBRIS_BURST_HALF", "0.5f", "flt_82001DA0"),
              ("KF_DEBRIS_BURST_GRAVITY", "-9.81000042f", "flt_82013AB8"),
              ("KF_DEBRIS_BURST_INHERIT_MIN", "0.0509999990f", "flt_82013AC0"),
              ("KF_DEBRIS_BURST_INHERIT_RANGE", "0.149000004f", "flt_82013ABC"),
              ("KF_DEBRIS_BURST_SPIN_RANGE", "3.5f", "flt_82009BA0"),
              ("KF_DEBRIS_BURST_SPIN_MIN", "0.5f", "flt_82001DA0"),
              ("KF_DEBRIS_BURST_GREY_WEIGHT", "0.333330005f", "unk_82FAB840"),
              ("KF_DEBRIS_BURST_SATURATION_LOW", "0.25f", "unk_82FAB7F0"),
              ("KF_DEBRIS_BURST_SATURATION_HIGH", "0.75f", "unk_82FAB890"),
              ("KF_DEBRIS_BURST_ALPHA_LOW", "0.5f", "unk_82FAB7D0"),
              ("KF_DEBRIS_BURST_ALPHA_HIGH", "0.899999976f", "unk_82FACC10"))
    yield ("the burst literals are the image's, each cited by address (flt_8200D964 deg->rad, flt_8200D970 2pi, "
           "flt_820054D0 7 m/s, flt_82013AB8 -9.81, the CRT colour splats unk_82FAB840 / 7F0 / 890 / 7D0 / unk_82FACC10, "
           "...)",
           all(re.search(r"const f32 %s\s*=\s*%s;" % (name, re.escape(value)), consts) for name, value, _ in wanted)
           and all(re.search(r"const f32 %s\s*=\s*%s;\s*//\s*%s" % (name, re.escape(value), address), source)
                   for name, value, address in wanted))
    yield ("the debrisparams words are the handler's own loads (0x7C / 0x80 speeds, 0xD0 angle, 0xD4 camera "
           "probability; counts 0xB4 / 0xA8 / 0xB0 / 0xAC, sizes 0x94..0x8C / 0xA4..0x9C per type)",
           all(re.search(pattern, consts) for pattern in (
               r"KU_BURST_VELOCITY_MIN\s*=\s*0x7C;", r"KU_BURST_VELOCITY_MAX\s*=\s*0x80;",
               r"KU_BURST_EMISSION_ANGLE_DEG\s*=\s*0xD0;", r"KU_BURST_CAMERA_PROBABILITY\s*=\s*0xD4;",
               r"\{\s*0xB4,\s*0x94,\s*0xA4\s*\}", r"\{\s*0xA8,\s*0x88,\s*0x98\s*\}",
               r"\{\s*0xB0,\s*0x90,\s*0xA0\s*\}", r"\{\s*0xAC,\s*0x8C,\s*0x9C\s*\}")))


def numeric(tree):
    source = tree.read(EVENTS_CPP)
    try:
        body, landed = region(source)
    except ValueError:
        print("NUMERIC: cannot build -- HandleFireDebrisBurstEvent is absent")
        return None
    if not landed:
        print("NUMERIC: this revision has no burst region -- measuring its lone HandleFireDebrisBurstEvent")
    consts = ""
    if CONSTS_START in source and CONSTS_END in source:
        consts = source[source.index(CONSTS_START):source.index(CONSTS_END)]
    colour = tree.read(COLOUR_CPP)
    shadow = {relative: tree.read(relative) for relative in SHADOW_HEADERS if tree.read(relative)}
    return compile_and_run(Path(__file__).with_name("FxCrashVfxDebrisBurst.cpp"), "fxcrashvfx_burst_body.inc",
                           body + "\n", "FxCrashVfxDebrisBurst", shadow=shadow, extra_sources=(RANDOM_CPP,),
                           extra_files={"fxcrashvfx_burst_consts.inc": consts + "\n",
                                        "fxcrashvfx_burst_colour.inc": colour + "\n"})


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", default=None, help="b5 revision to test (default: the working tree)")
    parser.add_argument("--root", default=None, help="a shadow tree root whose src/ files take precedence")
    args = parser.parse_args()
    tree = RootTree(args.rev, args.root)
    checks = list(wiring(tree))
    result = numeric(tree)
    return report("run_fxcrashvfx_debris_burst", checks, result, NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
