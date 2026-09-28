"""L4 WORLDVFX (owner's list 2026-09-27, "Not glass breaking particle"): a LION particle inherited its SPAWN POINT as
its velocity.

cParticleEmitter::Emit @0x82914D38 keeps two vectors on its stack: var_90, the emitter's raw velocity (the locator
arm copies cParticleLocator::mVel, `lvx128 v13` of locator+0x40 at 0x82914E28 -> `stvx128` var_90 at 0x82914E58;
the sub-emitter arm mParentVel, `lvx128 v0` of this+0x50 at 0x82914DA4 -> `stvx128` var_90 at 0x82914DC0), and
var_50, the spawn matrix's translation advanced along that velocity (one fmuls and one fadds per lane,
0x82914E94..0x82914ED8, w = 1.0). InitialiseParticle gets r8 = &var_90 (0x82914F28), ParticleInsert r6 = &var_90
(0x82914FB4), and InitialiseParticle @0x829116A8 scales that vector by mEmitterVelWeight into mLocatorVel
(0x82911AF0..0x82911B10). The PC body handed on the ADVANCED SPAWN POINT as the velocity, so every particle whose
behaviour inherits emitter velocity flew off at its own world position in m/s (~3.7 km/s in Paradise City): a
smashed window's Glass_shattering sprites drew 52 m from the pane one frame after the shatter.

  1. WIRING -- both of Emit's hand-offs (InitialiseParticle and ParticleInsert) pass the vector the arms load from
     mVel / mParentVel, and nothing overwrites it with the spawn point.
  2. NUMERIC -- tests/FxLionEmitVelocity.cpp compiles the PRODUCTION Emit onto a fixture and records what reaches
     InitialiseParticle / ParticleInsert / SpawnSubEmitter in four cases (the locator arm into a free slot, the
     locator arm through ParticleInsert into a fresh bucket, the sub-emitter arm, out of buckets). The expected spawn
     points are the console's fmuls / fadds, computed here with IEEE single rounding.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxlion_emit_velocity.py [--rev <b5 rev>]
                                                                                               [--root <shadow root>]
(--root reads any file present under <root>/... in place of the working tree's: the local gate for an edit that has
not reached the shared tree yet.)
"""
from pathlib import Path
import argparse
import re
import struct
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, code_only, compile_and_run, definition, report

EMITTER_CPP = "src/SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleEmitter.cpp"
EMIT_SIGNATURE = "void cParticleEmitter::Emit("
TICKS_PATTERN = re.compile(r"^\s*const\s+f32\s+KF_TICKS_TO_SECONDS\s*=\s*([0-9.eE+-]+)f\s*;", re.M)

# A: 1 + 4 + 1 + 4 + 1 + 1 + 1 = 13; B: 1 + 4 + 4 + 1 + 1 = 11; C: 1 + 4 + 1 + 4 = 10; D: 2  (see FxLionEmitVelocity.cpp)
NUMERIC_CHECKS = 13 + 11 + 10 + 2

# ---- the fixture's inputs (floats exactly representable, or rounded to single on the way in) ----
LOCATOR = ((0.6, 0.0, -0.8, 0.0), (0.0, 1.0, 0.0, 0.0), (0.8, 0.0, 0.6, 0.0), (3210.38, -2.33, -1965.76, 1.0))
LOCATOR_VEL = (-49.5, 0.3125, -48.75, 0.25)
A_TIME_TICKS, A_SPAWN_TICKS = 90000, 89950                  # a burst emitted 50 ticks before the frame
PARENT = ((0.0, 0.0, 1.0, 0.0), (0.0, 1.0, 0.0, 0.0), (-1.0, 0.0, 0.0, 0.0), (1234.5, 10.25, -2345.75, 1.0))
PARENT_VEL = (12.5, -3.25, 7.75, 0.5)
C_PARENT_TICKS, C_SPAWN_TICKS, C_TIME_TICKS = 70000, 70100, 70200


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


def f32(value):
    """IEEE single rounding (round to nearest even), the result of any single-precision VMX / FPU operation."""
    return struct.unpack("<f", struct.pack("<f", value))[0]


def bits(value):
    return struct.unpack("<I", struct.pack("<f", value))[0]


def literal(value):
    return "%.9gf" % f32(value) if value != int(value) else "%.1ff" % value


def vector(values):
    return "{ " + ", ".join(literal(v) for v in values) + " }"


def spawn_point(translation, velocity, ticks, ticks_to_seconds):
    """wa + vel * elapsed as the console runs it: elapsed = fmuls(frsp(fcfid(extsw ticks)), K) (0x82914E48..
    0x82914E90), then per lane fmuls vel * elapsed and fadds with the translation (0x82914E98..0x82914EC4)."""
    elapsed = f32(f32(float(ticks)) * ticks_to_seconds)
    return tuple(bits(f32(f32(f32(v) * elapsed) + f32(w))) for v, w in zip(velocity[:3], translation[:3]))


def expectations(ticks_to_seconds):
    lines = [
        "static const cMatrix KM_LOCATOR = { " + ", ".join(vector(row) for row in LOCATOR) + " };",
        "static const cVector KV_LOCATOR_VEL = " + vector(LOCATOR_VEL) + ";",
        f"static const s32 KI_A_TIME_TICKS  = {A_TIME_TICKS};",
        f"static const s32 KI_A_SPAWN_TICKS = {A_SPAWN_TICKS};",
        "static const u32 KAU_A_SPAWN[3] = { "
        + ", ".join("0x%08Xu" % b for b in spawn_point(LOCATOR[3], LOCATOR_VEL, A_SPAWN_TICKS - A_TIME_TICKS,
                                                     ticks_to_seconds)) + " };",
        "static const cMatrix KM_PARENT = { " + ", ".join(vector(row) for row in PARENT) + " };",
        "static const cVector KV_PARENT_VEL = " + vector(PARENT_VEL) + ";",
        f"static const s32 KI_C_PARENT_TICKS = {C_PARENT_TICKS};",
        f"static const s32 KI_C_SPAWN_TICKS  = {C_SPAWN_TICKS};",
        f"static const s32 KI_C_TIME_TICKS   = {C_TIME_TICKS};",
        "static const u32 KAU_C_SPAWN[3] = { "
        + ", ".join("0x%08Xu" % b for b in spawn_point(PARENT[3], PARENT_VEL, C_SPAWN_TICKS - C_PARENT_TICKS,
                                                     ticks_to_seconds)) + " };",
    ]
    return "\n".join(lines) + "\n"


def wiring(emit):
    code = code_only(emit)
    # InitialiseParticle(nucleus, vector, matrix, lMatrix, <velocity>, ...) / ParticleInsert(bucket, &lMatrix, <velocity>, ...)
    initialise = re.findall(r"InitialiseParticle\s*\(([^;]*)\);", code)
    insert = re.findall(r"ParticleInsert\s*\(([^;]*)\);", code)
    names = ([re.split(r"\s*,\s*", " ".join(c.split()))[4] for c in initialise]
             + [re.split(r"\s*,\s*", " ".join(c.split()))[2] for c in insert])
    same = len(names) == 2 and names[0] == names[1]
    name = names[0] if same else None
    loaded = (name is not None
              and re.search(re.escape(name) + r"\s*=\s*lrLocator\.mVel\s*;", code) is not None
              and re.search(re.escape(name) + r"\s*=\s*mParentVel\s*;", code) is not None)
    overwritten = name is not None and re.search(re.escape(name) + r"\.[xyz]\s*=", code) is not None
    yield ("Emit hands InitialiseParticle (0x82914F28) and ParticleInsert (0x82914FB4) the SAME vector, loaded whole "
           "from locator->mVel (0x82914E28) / mParentVel (0x82914DA4) and never overwritten with the spawn point",
           same and loaded and not overwritten)


def numeric(tree):
    source = tree.read(EMITTER_CPP).replace("\r\n", "\n")
    if not source:
        print("NUMERIC: cannot build -- no " + EMITTER_CPP)
        return None, ""
    try:
        emit = definition(source, EMIT_SIGNATURE)
    except ValueError:
        print("NUMERIC: cannot build -- no " + EMIT_SIGNATURE)
        return None, ""
    match = TICKS_PATTERN.search(source)
    if match is None:
        print("NUMERIC: cannot build -- no KF_TICKS_TO_SECONDS in " + EMITTER_CPP)
        return None, emit
    ticks_to_seconds = f32(float(match.group(1)))
    result = compile_and_run(Path(__file__).with_name("FxLionEmitVelocity.cpp"), "fxlion_emit_body.inc", emit + "\n",
                             "FxLionEmitVelocity",
                             extra_files={"fxlion_emit_constants.inc": match.group(0).strip() + "\n",
                                          "fxlion_emit_expect.inc": expectations(ticks_to_seconds)})
    return result, emit


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", default=None, help="b5 revision to test (default: the working tree)")
    parser.add_argument("--root", default=None, help="a shadow tree root whose files take precedence")
    args = parser.parse_args()
    tree = RootTree(args.rev, args.root)
    result, emit = numeric(tree)
    checks = list(wiring(emit)) if emit else [("Emit's body was found", False)]
    return report("run_fxlion_emit_velocity", checks, result, NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
