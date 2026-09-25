"""FX-CRASHVFX (crash parity 2026-09-25, C1): THE TYRE SMOKE -- BrnEffects::WheelStateMachine::Update @0x82293EB8,
HandleSmokeLayer @0x82288E38, FireNativeParticle @0x82288C30 and BrnParticle::ParticleModule::SpawnWheelSmoke
@0x82281AF0.

What a player sees on the console when a tyre slides -- a drift, a hard stop, a shunt, a burnout, a car scrubbing
sideways into a takedown: smoke off every skidding wheel, one particle per metre-and-a-bit of skid, spread back along
the wheel's path over the frame. Before this work FireNativeParticle announced itself NOT RECONSTRUCTED (its note
said the particle pool did not exist -- stale since the simple particles landed), so no wheel ever smoked; and the
crash gate read the console's byte offset +0x130 of ActiveRaceCarData, which on x64 is the low half of mID (mFlags
sits at +0x138), so bit 1 of the car's id, not its crash state, would have silenced the smoke.

  1. WIRING -- the bodies exist and do not announce; their literals are the image's, each cited; HandleSmokeLayer
     has the console's branch polarities (the skid gate returns on NaN, the spawn loop runs on while the
     accumulator is NaN -- the console hangs there -- with the one-shot NOT-IN-THE-X360-BINARY line before it) and
     one fused accumulation; Update reads the crash flag by name and computes the wheel speed as the console does.
  2. NUMERIC -- tests/FxCrashVfxWheelSmoke.cpp compiles the PRODUCTION WheelStateMachine.cpp whole, plus the
     production SpawnWheelSmoke, and compares every particle (type, position, time, velocity, size, rotation speed,
     bank, alpha), the accumulators, both rings, the asserts, and whether the spawn loop was still running at the
     64th record, bit for bit, with 0x82293EB8 run on emu64 (scratch/CRASHPARITY_0922/fxcrashvfx_vmxemu/
     gen_wheelsmoke_data.py; 26 cases x 5 checks), plus the one-shot NaN line (1 check).

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxcrashvfx_wheel_smoke.py
                                                                   [--rev <b5 rev>] [--root <shadow tree root>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import REPO, Tree, code_only, compile_and_run, definition, report

WHEEL_CPP = "src/GameSource/Effects/Wheel/WheelStateMachine.cpp"
MODULE_CPP = "src/GameSource/Effects/Particles/ParticleModule.cpp"
SHADOW_HEADERS = ("src/GameSource/Effects/Wheel/WheelStateMachine.h",
                  "src/GameSource/Effects/Particles/ParticleModule.h")
EXTRA_SOURCES = (REPO / "src/GameShared/GameClasses/Numeric/CgsRandom.cpp",)
SPAWN_SIGNATURE = "    void ParticleModule::SpawnWheelSmoke("

NUMERIC_CHECKS = 26 * 5 + 1


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


def wiring(tree):
    source = tree.read(WHEEL_CPP)
    fire = body(source, "void WheelStateMachine::FireNativeParticle(")
    yield ("FireNativeParticle has a body that does not announce: size, the unit vector and the kick from the effects "
           "ring, the velocity fused lane by lane, then SpawnWheelSmoke (0x82288C30)",
           fire != "" and "WriteToLog" not in fire and "NOT RECONSTRUCTED" not in fire
           and "RandomVector(" in fire and fire.count("RandomFloat()") == 2 and "SpawnWheelSmoke(" in fire
           and "KF_WHEEL_SMOKE_SIZE_RANGE" in fire and "KF_WHEEL_SMOKE_SIZE_MIN" in fire)

    module = tree.read(MODULE_CPP)
    spawn = body(module, SPAWN_SIGNATURE)
    yield ("ParticleModule::SpawnWheelSmoke has a body: one draw over the array's rotation-speed range, times the "
           "angular scale, negated for the reversed wheels, into the REGULAR bank at alpha 1.0 (0x82281AF0)",
           spawn != "" and "mRandom.RandomFloat(lpParams->mrRotationSpeedMin, lpParams->mrRotationSpeedMax)" in spawn
           and "* lfAngularVelocityScale" in spawn and "-lfRotationalVelocity" in spawn
           and re.search(r"SpawnParticle\(lvPosition,\s*lvVelocity,\s*lfSpawnTime,\s*lfSizeScale,\s*"
                         r"lfRotationalVelocity,\s*false,\s*1\.0f\)", spawn) is not None)

    consts = code_only(source)
    wanted = (("KF_WHEEL_SMOKE_SIZE_MIN", "0.4f", "flt_82011C18"),
              ("KF_WHEEL_SMOKE_SIZE_RANGE", "0.80000007f", "flt_82011C1C"))
    yield ("the literals are the image's, each cited: the size range flt_82011C18 0.4 / flt_82011C1C 0.80000007, "
           "K_VELOCITY_SPREAD (1.5, 0.6, 1.5, 0) and K_VELOCITY_INHERITANCE (0.4, 0.4, 0.4, 0) from their CRT thunks "
           "0x82C4AAF8 / 0x82C4AB38",
           all(re.search(r"const f32 %s\s*=\s*%s;\s*//\s*%s" % (name, re.escape(value), address), source)
               for name, value, address in wanted)
           and re.search(r"K_VELOCITY_SPREAD\s*=\s*\{\s*1\.5f,\s*0\.6f,\s*1\.5f,\s*0\.0f\s*\}", consts) is not None
           and re.search(r"K_VELOCITY_INHERITANCE\s*=\s*\{\s*0\.4f,\s*0\.4f,\s*0\.4f,\s*0\.0f\s*\}", consts) is not None
           and "0x82C4AAF8" in source and "0x82C4AB38" in source)

    layer = body(source, "void WheelStateMachine::HandleSmokeLayer(")
    line = layer.find("[wheel-smoke] NaN ACCUMULATOR")
    loop = layer.find("while (!(lrfAccumulator < 1.0f))")
    yield ("HandleSmokeLayer: the skid gate returns on NaN (`ble` 0x82288EB4), ONE fused accumulation (0x82288ED8), "
           "the loop runs while !(acc < 1) (`bge` 0x82289034: a NaN accumulator hangs, as on the console), and the "
           "one-shot NOT-IN-THE-X360-BINARY line is written before it",
           "if (!(lfSkidFactor > lfSkidStartThreshold))" in layer
           and "lrfAccumulator = std::fma(lfSkidFactor * lfMaxWheelTravel, lfParticlesPerMetre, lrfAccumulator);" in layer
           and 0 < line < loop and "NOT IN THE X360 BINARY" in layer[line - 40:loop]
           and "static bool sbSaid" in layer and "while (lrfAccumulator >= 1.0f)" not in layer)

    update = body(source, "void WheelStateMachine::Update(")
    yield ("Update reads the crash flag BY NAME (ActiveRaceCarData::mFlags eARDFlagIsCrashing -- the console's "
           "`lhz 0x130` at 0x82293F2C; +0x130 is mID's low half on x64) and takes |v| as the refined rsqrt with the "
           "vsel zero guard (0x82293F6C..0x82293FDC)",
           "ActiveRaceCar()->GetFlags() & ActiveRaceCarData::eARDFlagIsCrashing" in update
           and "0x130" not in code_only(source) and "GuardedMagnitude(lWheelVelocity)" in update
           and "Magnitude(lWheelVelocity)" not in update.replace("GuardedMagnitude(lWheelVelocity)", ""))


def numeric(tree):
    source = tree.read(WHEEL_CPP)
    module = tree.read(MODULE_CPP)
    shadow = {relative: tree.read(relative) for relative in SHADOW_HEADERS if tree.read(relative)}
    inc = source + "\n"
    try:
        # SpawnWheelSmoke joins the unit when the revision has it (a revision without it never calls it).
        inc += "\nnamespace BrnParticle\n{\n" + definition(module, SPAWN_SIGNATURE) + "\n}\n"
    except ValueError:
        print("NUMERIC: this revision has no ParticleModule::SpawnWheelSmoke; its WheelStateMachine.cpp runs alone")
    return compile_and_run(Path(__file__).with_name("FxCrashVfxWheelSmoke.cpp"), "fxcrashvfx_wheelsmoke_tu.inc", inc,
                           "FxCrashVfxWheelSmoke", shadow=shadow, extra_sources=EXTRA_SOURCES)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", default=None, help="b5 revision to test (default: the working tree)")
    parser.add_argument("--root", default=None, help="a shadow tree root whose src/ files take precedence")
    args = parser.parse_args()
    tree = RootTree(args.rev, args.root)
    checks = list(wiring(tree))
    result = numeric(tree)
    return report("run_fxcrashvfx_wheel_smoke", checks, result, NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
