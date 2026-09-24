"""FX-CRASHVFX (crash parity 2026-09-24, item 7 CPU half): the native simple-particle family.

Impact smoke, crash impact dust and the ten skid-smoke surfaces are BrnParticle::Native::BrnSimpleParticleArray
records. Before this fix the array was a two-field placeholder with only AcquireTexture bodied (and the file was
not even on the link), so ParticleModule::Prepare @0x8229BEA0, LoadFXBundle stage 12 @0x8229D468 and
EffectsModule::LoadNativeParticleParams @0x82290510 each announced their simple-particle leg, and no producer
(ParticleModule::SpawnSimple @0x82281A10 -- no body at all) could write a single particle.

  1. WIRING -- Prepare constructs and prepares the thirteen arrays; stage 12 publishes the thirteen textures and
     asserts every array ready; LoadNativeParticleParams resolves the twelve nativeparticleparams collections
     (class 0x43DA904B_E836238A) into UpdateParams; SpawnSimple exists and feeds SpawnParticle.
  2. NUMERIC -- tests/FxCrashVfxSimpleParticles.cpp compiles the PRODUCTION BrnSimpleParticleArray.cpp and the
     production SpawnSimple body (on a two-member fixture) against the revision's own headers, the real
     CgsNumeric::Random and the real TextureNameMap hash, and checks the bank sizing, the Prepare stamp, every
     UpdateParams store (including the colour byte WRAP), Initialize, SpawnParticle's record and cursor,
     AcquireTexture's three arms and SpawnSimple's argument shuffle and ring draw.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxcrashvfx_simple_particles.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import REPO, Tree, body_or_empty, code_only, compile_and_run, definition, report

ARRAY_H = "src/GameSource/Effects/Particles/Native/BrnSimpleParticleArray.h"
ARRAY_CPP = "src/GameSource/Effects/Particles/Native/BrnSimpleParticleArray.cpp"
PARAMS_H = "src/GameSource/AttribSys/Generated/classes/nativeparticleparams.h"
MODULE_CPP = "src/GameSource/Effects/Particles/ParticleModule.cpp"
LIFECYCLE_CPP = "src/GameSource/Effects/Particles/ParticleModule_Lifecycle.cpp"
EFFECTS_CPP = "src/GameSource/Effects/EffectsModule.cpp"
RANDOM_CPP = REPO / "src/GameShared/GameClasses/Numeric/CgsRandom.cpp"
HASH_CPP = REPO / "src/SharedClasses/Graphics/TextureNameMapEntry.cpp"

NUMERIC_CHECKS = 42


def wiring(tree):
    lifecycle = tree.read(LIFECYCLE_CPP).replace("\r\n", "\n")
    prepare = body_or_empty(lifecycle, "bool ParticleModule::Prepare(")
    yield ("ParticleModule::Prepare constructs all 13 simple arrays and runs both banks' Prepare (0x8229C2A8..0x8229C398)",
           re.search(r"for\s*\(\s*u32\s+(\w+)\s*=\s*0\s*;\s*\1\s*<\s*KU_NUM_SIMPLE_ARRAYS", prepare) is not None
           and ".Construct(mpHeapMalloc" in prepare and "mBankRegular.Prepare(" in prepare
           and "mBankCrash.Prepare(" in prepare)

    load = body_or_empty(lifecycle, "bool ParticleModule::LoadFXBundle(")
    yield ("LoadFXBundle stage 12 publishes each reply into the 13 simple arrays and asserts them ready (0x8229D468..0x8229D518)",
           re.search(r"maSimpleParticles\[\w+\]\.AcquireTexture\(", load) is not None
           and "Missing Native Particle Texture" in lifecycle and ".IsReady()" in load)

    effects = tree.read(EFFECTS_CPP).replace("\r\n", "\n")
    native = body_or_empty(effects, "void EffectsModule::LoadNativeParticleParams(")
    yield ("LoadNativeParticleParams resolves nativeparticleparams into maSimpleParticles[i].UpdateParams (0x82290684)",
           "UpdateParams(" in native and "nativeparticleparams" in native and "LogNotReconstructed" not in native)

    params = code_only(tree.read(PARAMS_H))
    yield ("Attrib::Gen::nativeparticleparams keys its collections under 0x43DA904B_E836238A (0x82290624..0x82290638)",
           "0x43DA904BE836238A" in params.upper().replace("ULL", "") or "0x43DA904BE836238A" in params)

    module = tree.read(MODULE_CPP).replace("\r\n", "\n")
    spawn = body_or_empty(module, "    void ParticleModule::SpawnSimple(")
    yield ("ParticleModule::SpawnSimple has a body that draws mRandom and calls SpawnParticle (0x82281A10)",
           "SpawnParticle(" in spawn and "mRandom.RandomFloat(" in spawn)


def numeric(tree):
    array_cpp = tree.read(ARRAY_CPP)
    array_h = tree.read(ARRAY_H)
    params_h = tree.read(PARAMS_H)
    module = tree.read(MODULE_CPP).replace("\r\n", "\n")
    missing = []
    if "void BrnSimpleParticleArray::CB4ParticleBank::Prepare(" not in array_cpp.replace("\r\n", "\n"):
        missing.append("BrnSimpleParticleArray::CB4ParticleBank::Prepare / the array bodies")
    if not params_h:
        missing.append("nativeparticleparams.h")
    try:
        spawn = definition(module, "    void ParticleModule::SpawnSimple(")
    except ValueError:
        missing.append("ParticleModule::SpawnSimple")
        spawn = ""
    if missing:
        print("NUMERIC: cannot build -- production bodies absent: " + "; ".join(missing))
        return None
    spawn = spawn.replace("ParticleModule::SpawnSimple(", "ParticleModuleFixture::SpawnSimple(")
    fixture = (
        "\nnamespace BrnParticle {\n"
        "struct ParticleModuleFixture {\n"
        "    CgsNumeric::Random mRandom;\n"
        "    Native::BrnSimpleParticleArray maSimpleParticles[Native::eParticleArray_Max];\n"
        "    void SpawnSimple(Vector3 lvPosition, Vector3 lvVelocity, Native::ENativeParticleType leParticleType,\n"
        "                     f32 lfSizeScale, f32 lfSpawnTime, f32 lfAlpha);\n"
        "};\n" + spawn + "\n}\n")
    inc = array_cpp + fixture
    shadow = {ARRAY_H: array_h, PARAMS_H: params_h}
    return compile_and_run(Path(__file__).with_name("FxCrashVfxSimpleParticles.cpp"), "fxcrashvfx_simple.inc",
                           inc, "FxCrashVfxSimple", shadow=shadow, extra_sources=(RANDOM_CPP, HASH_CPP))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", default=None, help="b5 revision to test (default: the working tree)")
    args = parser.parse_args()
    tree = Tree(args.rev)
    checks = list(wiring(tree))
    result = numeric(tree)
    return report("run_fxcrashvfx_simple_particles", checks, result, NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
