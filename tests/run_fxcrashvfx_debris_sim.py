"""FX-CRASHVFX (crash parity 2026-09-25): THE DEBRIS SIMULATION -- ParticleModule::BeginSimulateDebris @0x82289A98,
DebrisUpdateJob::Execute @0x82C08298, BrnDebrisArrayLite::Update @0x82C08B58 and the per-bucket integrator
sub_82C08410, plus EndSimulateDebris @0x8227A1F0. Before this fix none of them ran: BeginSimulateDebris and
EndSimulateDebris were announced NOT REPRODUCED and BrnDebrisArrayLite.cpp was an unmounted assert stub, so every
debris particle the PC spawned (the jump wheel debris) sat where it was born until it faded.

WIRING -- DispatchThreadUpdate calls BeginSimulateDebris straight after ProcessEventQueue, RenderFullResParticles calls
EndSimulateDebris ahead of the debris pass, neither is announced any more, and the parent build mounts
BrnDebrisArrayLite.cpp.

NUMERIC (two programs, both against the console's own words on emu64 -- tests/FxCrashVfxDebrisSimData.h, written by
scratch/CRASHPARITY_0922/fxcrashvfx_vmxemu/gen_debris_sim_data.py):
  tests/FxCrashVfxDebrisSim.cpp      -- the revision's whole BrnDebrisArrayLite.cpp: every particle, every collide
                                        call's segments, the job Random (7 cases x 3 checks; the seventh: NaN / infinite birth times --
                                        a NaN age is SKIPPED, `ble` at 0x82C0875C takes the unordered compare).
  tests/FxCrashVfxDebrisSimBegin.cpp -- the production BeginSimulateDebris body: the expiry calls, the five job
                                        slots (the step read VERBATIM off DispatchThreadUpdateData+4, the update
                                        frame's sum of sub-steps), the jobs started, the module Random
                                        (5 cases x 4 checks).

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxcrashvfx_debris_sim.py [--rev <b5 rev>]
                                                                             [--root <shadow tree root>]
"""
from pathlib import Path
import argparse
import sys

sys.dont_write_bytecode = True
from fxgs_common import REPO, WORKFLOW, Tree, code_only, compile_and_run, definition, report

LITE_CPP = "src/GameSource/Effects/Particles/Native/BrnDebrisArrayLite.cpp"
PM_CPP = "src/GameSource/Effects/Particles/ParticleModule.cpp"
SHADOW_HEADERS = ("src/GameSource/Effects/Particles/Native/BrnDebrisArrayLite.h",
                  "src/GameSource/Effects/Particles/Native/BrnDebrisArray.h",
                  "src/GameSource/Effects/BrnCrashTriangleCache.h")
RANDOM_CPP = REPO / "src/GameShared/GameClasses/Numeric/CgsRandom.cpp"
BEGIN_SIGNATURE = "void ParticleModule::BeginSimulateDebris("
DISPATCH_SIGNATURE = "void ParticleModule::DispatchThreadUpdate("
RENDER_SIGNATURE = "void ParticleModule::RenderFullResParticles("
MOUNT = WORKFLOW / "tools/build/build_game_exe.bat"

EXEC_CHECKS = 7 * 3
BEGIN_CHECKS = 5 * 4


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


def body(tree, relative, signature):
    source = tree.read(relative).replace("\r\n", "\n")
    try:
        return code_only(definition(source, signature))
    except ValueError:
        return ""


def wiring(tree):
    dispatch = body(tree, PM_CPP, DISPATCH_SIGNATURE)
    render = body(tree, PM_CPP, RENDER_SIGNATURE)
    yield ("DispatchThreadUpdate calls BeginSimulateDebris after ProcessEventQueue and no longer announces it",
           "BeginSimulateDebris(lpDispatchThreadInput)" in dispatch and "ProcessEventQueue(" in dispatch
           and dispatch.index("ProcessEventQueue(") < dispatch.index("BeginSimulateDebris(lpDispatchThreadInput)")
           and "LogNotReconstructed" not in dispatch.split("BeginSimulateDebris(lpDispatchThreadInput)")[0])
    yield ("RenderFullResParticles calls EndSimulateDebris ahead of the debris pass, unannounced",
           "EndSimulateDebris(" in render and "RenderDebrisArray(" in render
           and render.index("EndSimulateDebris(") < render.index("RenderDebrisArray(")
           and "EndSimulateDebris --" not in tree.read(PM_CPP))
    mount = MOUNT.read_text(encoding="utf-8", errors="replace") if MOUNT.exists() else ""
    yield ("the parent build mounts BrnDebrisArrayLite.cpp (the job side)",
           "Particles\\Native\\BrnDebrisArrayLite.cpp" in mount)


def numeric(tree):
    shadow = {relative: tree.read(relative) for relative in SHADOW_HEADERS if tree.read(relative)}
    lite = tree.read(LITE_CPP).replace("\r\n", "\n")
    results = []
    if "DebrisUpdateJob::Execute" not in lite:
        print("NUMERIC: cannot build the job test -- BrnDebrisArrayLite.cpp has no DebrisUpdateJob::Execute")
        results.append(None)
    else:
        results.append(compile_and_run(Path(__file__).with_name("FxCrashVfxDebrisSim.cpp"), "fxcrashvfx_debris_sim.inc",
                                       lite + "\n", "FxCrashVfxDebrisSim", shadow=shadow,
                                       extra_sources=(RANDOM_CPP,)))
    source = tree.read(PM_CPP).replace("\r\n", "\n")
    try:
        begin = definition(source, BEGIN_SIGNATURE)
    except ValueError:
        begin = ""
    if not begin:
        print("NUMERIC: cannot build the dispatch test -- ParticleModule::BeginSimulateDebris is absent")
        results.append(None)
    else:
        results.append(compile_and_run(Path(__file__).with_name("FxCrashVfxDebrisSimBegin.cpp"),
                                       "fxcrashvfx_debris_sim_begin.inc", begin + "\n", "FxCrashVfxDebrisSimBegin",
                                       shadow=shadow, extra_sources=(RANDOM_CPP,)))
    checks = failures = 0
    for result, total in zip(results, (EXEC_CHECKS, BEGIN_CHECKS)):
        if result is None:
            result = (total, total)
        checks += result[0]
        failures += result[1]
    return checks, failures


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", default=None, help="b5 revision to test (default: the working tree)")
    parser.add_argument("--root", default=None, help="a shadow tree root whose src/ files take precedence")
    args = parser.parse_args()
    tree = RootTree(args.rev, args.root)
    return report("run_fxcrashvfx_debris_sim", list(wiring(tree)), numeric(tree), EXEC_CHECKS + BEGIN_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
