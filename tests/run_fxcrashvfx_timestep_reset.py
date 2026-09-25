"""FX-CRASHVFX (crash parity 2026-09-25): THE PARTICLE TIME STEP IS CLEARED AT THE START OF EVERY UPDATE FRAME --
BrnGameModule::OnStartOfUpdateFrame @0x823A8BB0 -> EffectsModule::StartOfFrame -> ParticleModule::StartOfFrame.

ParticleModule::Update @0x822817D8 adds each simulation sub-step's scaled step to mRenderData.mfCurrentTimeStep
(0x8228185C..0x82281870). The console clears it at the start of every update frame: OnStartOfUpdateFrame's first
statement is the two inline StartOfFrame calls, compiled to ONE store (`lis r10,0x88 ; ori r9,r10,0x194C ;
lfs f0,flt_82001CC0 ; stfsx f0,r11,r9` -- the particle module is gameModule + 0x878B40, see OnEndOfUpdateFrame at
0x823DC378, so 0x88194C is its +0x8E0C). So the record GenerateRenderRequests / PreRenderUpdate publish carries the
frame's SUM of sub-steps. The PC dropped the store, so the field grew from boot, and the spark ring, the trails and
the debris each carried a first-difference workaround while the motion blur read it verbatim.

  1. WIRING -- OnStartOfUpdateFrame calls mEffectsModule.StartOfFrame() before mRenderModule.StartOfFrame();
     EffectsModule::StartOfFrame calls mParticleModule.StartOfFrame(); ParticleModule::StartOfFrame stores 0.0f to
     mRenderData.mfCurrentTimeStep; the spark ring, the trails and BeginSimulateDebris read the field VERBATIM (the
     three first differences are gone); the PC bring-up stand-in clears its own record's step before its one Update
     of the frame.
  2. NUMERIC -- tests/FxCrashVfxTimeStepReset.cpp compiles the PRODUCTION OnStartOfUpdateFrame, both StartOfFrame
     bodies and ParticleModule::Update onto a fixture and runs update frames of k sub-steps; the expected values are
     the console's own (0x823A8BB0 and 0x822817D8 interpreted on emu64 by
     scratch/CRASHPARITY_0922/fxcrashvfx_vmxemu/gen_tstep_data.py; 10 cases x 6 checks).

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxcrashvfx_timestep_reset.py [--rev <b5 rev>]
                                                                               [--root <shadow tree root>]
(--root reads any file present under <root>/src/... in place of the working tree's.)
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, code_only, compile_and_run, definition, report

GAME_CPP = "src/GameSource/Game/BrnGameModule.cpp"
EFFECTS_H = "src/GameSource/Effects/EffectsModule.h"
PARTICLE_H = "src/GameSource/Effects/Particles/ParticleModule.h"
PARTICLE_CPP = "src/GameSource/Effects/Particles/ParticleModule.cpp"
LIFECYCLE_CPP = "src/GameSource/Effects/Particles/ParticleModule_Lifecycle.cpp"
BRINGUP_CPP = "src/GameSource/Effects/Particles/ParticleModuleBringUp.cpp"

UPDATE_CONSTANTS = ("KF_LION_TIME_TICKS_PER_SECOND", "KF_SLOWMO_LOWER", "KF_SLOWMO_UPPER",
                    "KU_CAMERA_FLAG_TRAILS_OFF", "KU_CAMERA_FLAG_NEW_FRAME")
NO_START_OF_FRAME = ("// [fixture] this revision has no StartOfFrame here: an empty stand-in so the fixture compiles.\n"
                     "void StartOfFrame() {}\n")

# 10 cases x 6 checks (see FxCrashVfxTimeStepReset.cpp)
NUMERIC_CHECKS = 10 * 6


class RootTree(Tree):
    """The working tree (or --rev) with an optional shadow root whose files take precedence."""

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


def optional_definition(source, signature):
    try:
        return definition(source, signature)
    except ValueError:
        return ""


def inline_start_of_frame(header):
    """The header's inline `void StartOfFrame() { ... }` definition -- matched at the start of a code line, so the
    DWARF citations in the comments (`void StartOfFrame();`) cannot be taken for it."""
    match = re.search(r"^[ \t]*void StartOfFrame\(\)\s*\{", header, flags=re.M)
    if match is None:
        return ""
    return definition(header[match.start():], "void StartOfFrame()")


def start_of_frame(header):
    text = inline_start_of_frame(header)
    return (text + "\n") if text else NO_START_OF_FRAME


def update_pieces(lifecycle):
    constants = []
    for name in UPDATE_CONSTANTS:
        match = re.search(r"^[ \t]*const\s+(?:f32|u32)\s+%s\s*=\s*[^;]+;" % name, lifecycle, flags=re.M)
        if match is None:
            raise ValueError("no constant " + name)
        constants.append(match.group(0).strip())
    body = definition(lifecycle, "void ParticleModule::Update(")
    return "namespace\n{\n" + "\n".join(constants) + "\n}\n" + body + "\n"


def wiring(tree):
    game = tree.read(GAME_CPP)
    on_start = code_only(optional_definition(game, "void BrnGameModule::OnStartOfUpdateFrame()"))
    effects = code_only(inline_start_of_frame(tree.read(EFFECTS_H)))
    particle = code_only(inline_start_of_frame(tree.read(PARTICLE_H)))
    effects_call = on_start.find("mEffectsModule.StartOfFrame();")
    render_call = on_start.find("mRenderModule.StartOfFrame();")
    yield ("BrnGameModule::OnStartOfUpdateFrame runs the effects module's StartOfFrame FIRST, then the renderer's "
           "(0x823A8BB0: the store, then the tail call at 0x823A8BCC)",
           0 <= effects_call < render_call)
    yield ("EffectsModule::StartOfFrame (DWARF EffectsModule.h:277) is the particle module's",
           "mParticleModule.StartOfFrame();" in effects)
    yield ("ParticleModule::StartOfFrame (DWARF ParticleModule.h:309) clears mRenderData.mfCurrentTimeStep to 0.0f "
           "(`lfs f0, flt_82001CC0 ; stfsx f0, r11, r9`, r9 = 0x88194C = gameModule + 0x878B40 + 0x8E0C)",
           re.search(r"mRenderData\.mfCurrentTimeStep\s*=\s*0\.0f\s*;", particle) is not None)

    particles = tree.read(PARTICLE_CPP)
    sparks = code_only(optional_definition(particles, "void ParticleModule::BeginParticleRenderJob("))
    trails = code_only(optional_definition(particles, "void ParticleModule::BuildLionVertexBuffers("))
    debris = code_only(optional_definition(particles, "void ParticleModule::BeginSimulateDebris("))
    whole = code_only(particles)
    yield ("the spark ring is handed the field VERBATIM (`lfs f1, 0xC(r30)` at 0x8228A82C) -- no first difference",
           "sfLastConsumedTimeStepSum" not in whole
           and re.search(r"lfRingDelta\s*=\s*lpRenderData->mfCurrentTimeStep\s*;", sparks) is not None
           and "mSparkFrameDataSetUpdate.Update(lrView, lrProj, lfRingDelta)" in sparks)
    yield ("the trail system is handed the field VERBATIM (0x8228AD10 / 0x8228AD2C) -- no first difference",
           "sfLastTrailTimeStepSum" not in whole
           and re.search(r"mTrailSystem\.Update\(\s*lpRenderData->mfCurrentTimeStep\s*,", trails) is not None)
    yield ("BeginSimulateDebris gates on and integrates DispatchThreadUpdateData+4 VERBATIM (`lfs f13, 4(r29)` at "
           "0x82289B18) -- no first difference",
           "sfLastDebrisTimeStepSum" not in whole
           and re.search(r"if\s*\(\s*!\s*\(\s*lpData->mfCurrentTimeStep\s*>\s*0\.0f\s*\)\s*\)", debris) is not None
           and re.search(r"lrJob\.mfTimeStep\s*=\s*lpData->mfCurrentTimeStep\s*;", debris) is not None)
    bringup = code_only(optional_definition(tree.read(BRINGUP_CPP), "void PCBringUpProduceParticleRenderData("))
    clear = bringup.find("gRenderData.mfCurrentTimeStep = 0.0f;")
    update = bringup.find("ParticleModuleUpdateBringUp(gRenderData")
    yield ("the PC bring-up stand-in clears its own record's step before its one Update of the frame",
           0 <= clear < update)


def numeric(tree):
    try:
        update = update_pieces(tree.read(LIFECYCLE_CPP))
        body = definition(tree.read(GAME_CPP), "void BrnGameModule::OnStartOfUpdateFrame()")
    except ValueError as error:
        print("NUMERIC: cannot build -- " + str(error))
        return None
    incs = {
        "fxcrashvfx_tstep_update.inc": update,
        "fxcrashvfx_tstep_particle_sof.inc": start_of_frame(tree.read(PARTICLE_H)),
        "fxcrashvfx_tstep_effects_sof.inc": start_of_frame(tree.read(EFFECTS_H)),
    }
    return compile_and_run(Path(__file__).with_name("FxCrashVfxTimeStepReset.cpp"), "fxcrashvfx_tstep_body.inc",
                           body + "\n", "FxCrashVfxTimeStepReset", extra_files=incs)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", default=None, help="b5 revision to test (default: the working tree)")
    parser.add_argument("--root", default=None, help="a shadow tree root whose src/ files take precedence")
    args = parser.parse_args()
    tree = RootTree(args.rev, args.root)
    checks = list(wiring(tree))
    result = numeric(tree)
    return report("run_fxcrashvfx_timestep_reset", checks, result, NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
