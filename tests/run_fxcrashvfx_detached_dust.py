"""FX-CRASHVFX (crash parity 2026-09-24, item 2): the detached-part DUST arm of
EffectsModule::ProcessCarDetatchedPartContacts @0x82292FA0.

A deformable part scraping the world with EBodyParts id 91 sheds CRASH IMPACT DUST: its accumulator
(mafAccumulatedParticleCountTyres[slot]) gains mDt * 20 per frame (fused), the whole part of it is spawned as
ParticleModule::SpawnSimple(type 2) calls -- position = the contact point, velocity = the contact normal plus a
fused Random()*6-3 jitter per lane (drawn z, y, x after the size), size = Random()*0.5+0.2, the frame time, alpha
1.0 -- and the fraction is carried. Before this fix the arm announced itself ("BLOCKED: SpawnSimple ... has no
body") and spawned nothing.

  1. WIRING -- the dust arm calls SpawnSimple with eParticleArray_CrashImpactDust and no longer announces
     itself; its constants are the image's (20.0 / 6.0 / 3.0 / 0.5 / 0.2 / 1.0 cited by address).
  2. NUMERIC -- tests/FxCrashVfxDetachedDust.cpp compiles the PRODUCTION body (and the constants / helpers it
     names) onto a fixture and compares it, bit for bit, with the console's own outputs in
     tests/FxCrashVfxDetachedDustData.h (the drain's real instruction words run by
     scratch/CRASHPARITY_0922/fxcrashvfx_vmxemu/gen_dust_data.py).

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxcrashvfx_detached_dust.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import REPO, Tree, body_or_empty, code_only, compile_and_run, definition, report

EFFECTS_CPP = "src/GameSource/Effects/EffectsModule.cpp"
ARRAY_H = "src/GameSource/Effects/Particles/Native/BrnSimpleParticleArray.h"
RANDOM_CPP = REPO / "src/GameShared/GameClasses/Numeric/CgsRandom.cpp"
SIGNATURE = "void EffectsModule::ProcessCarDetatchedPartContacts("

# 4 cases x 6 checks + 2 global checks (see FxCrashVfxDetachedDust.cpp)
NUMERIC_CHECKS = 26

# Helpers defined in EffectsModule.cpp's anonymous namespace that the body may name. LogNotReconstructed is
# NOT extracted: the fixture supplies a counting stand-in, so an announcing (parent) body is measured.
HELPERS = ("s32 FloorToS32Fctiwz(", "const Attrib::RefSpec& VfxSurfaceRef(")


def wiring(tree):
    effects = tree.read(EFFECTS_CPP).replace("\r\n", "\n")
    body = body_or_empty(effects, SIGNATURE)
    dust = body.split("KI_DETACHED_DUST_PART_TYPE", 1)[1] if "KI_DETACHED_DUST_PART_TYPE" in body else ""
    dust = dust.split("KF_DETACHED_SPARK_MIN_SPEED", 1)[0] if dust else ""
    yield ("the dust arm spawns CRASH IMPACT DUST through ParticleModule::SpawnSimple (0x8229332C, `li r4, 2`)",
           "mParticleModule.SpawnSimple(" in dust and "eParticleArray_CrashImpactDust" in dust)
    yield ("the dust arm no longer announces itself NOT RECONSTRUCTED",
           "LogNotReconstructed" not in body)
    consts = code_only(effects)
    wanted = (("KF_DETACHED_DUST_PER_SECOND", "20.0f"), ("KF_DETACHED_DUST_JITTER_SPAN", "6.0f"),
              ("KF_DETACHED_DUST_JITTER_HALF", "3.0f"), ("KF_DETACHED_DUST_SIZE_RANGE", "0.5f"),
              ("KF_DETACHED_DUST_SIZE_MIN", "0.2f"), ("KF_DETACHED_DUST_ALPHA", "1.0f"))
    yield ("the dust constants are the image's: flt_820054CC 20.0, flt_8200DD28 6.0, flt_8200DD24 3.0, "
           "flt_82001DA0 0.5, flt_8200DD40 0.2, flt_82001C98 1.0",
           all(re.search(r"const f32 %s\s*=\s*%s;" % (name, re.escape(value)), consts) for name, value in wanted)
           and all(addr in effects for addr in ("flt_820054CC", "flt_8200DD28", "flt_8200DD24", "flt_82001DA0",
                                                "flt_8200DD40", "flt_82001C98")))


def anonymous_names(effects, body):
    """The helper functions the extracted body names and the K* constants the body and those helpers
    name, as definitions (constants first)."""
    helpers = []
    for signature in HELPERS:
        if signature.split("(")[0].split()[-1] in body:
            try:
                helpers.append(definition(effects, signature))
            except ValueError:
                pass
    consts = []
    text = code_only(body + "\n" + "\n".join(helpers))
    for name in sorted(set(re.findall(r"\bK[A-Z]{1,2}_[A-Z0-9_]+\b", text))):
        if name == "KU_NUM_ACTIVE_RACE_CARS":          # EffectsModule's own static member; the fixture has it
            continue
        match = re.search(r"^\s*const\s+[\w:]+\s+%s\s*=\s*[^;]+;" % re.escape(name), effects, flags=re.M)
        if match:
            consts.append(match.group(0).strip())
    return "\n".join(consts + helpers) + "\n"


def numeric(tree):
    effects = tree.read(EFFECTS_CPP).replace("\r\n", "\n")
    try:
        body = definition(effects, SIGNATURE)
    except ValueError:
        print("NUMERIC: cannot build -- ProcessCarDetatchedPartContacts is absent")
        return None
    body = body.replace("EffectsModule::ProcessCarDetatchedPartContacts(",
                        "DetachedDustFixture::ProcessCarDetatchedPartContacts(")
    consts = anonymous_names(effects, body)
    shadow = {ARRAY_H: tree.read(ARRAY_H)}
    return compile_and_run(Path(__file__).with_name("FxCrashVfxDetachedDust.cpp"), "fxcrashvfx_dust_body.inc",
                           body, "FxCrashVfxDetachedDust", shadow=shadow, extra_sources=(RANDOM_CPP,),
                           extra_files={"fxcrashvfx_dust_consts.inc": consts})


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", default=None, help="b5 revision to test (default: the working tree)")
    args = parser.parse_args()
    tree = Tree(args.rev)
    checks = list(wiring(tree))
    result = numeric(tree)
    return report("run_fxcrashvfx_detached_dust", checks, result, NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
