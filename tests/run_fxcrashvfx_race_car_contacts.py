"""FX-CRASHVFX (crash parity 2026-09-24, item 1): EffectsModule::ProcessRaceCarContacts @0x82297C08 -- the race-car
contact drain -- and its whole callee wall: DoSparkShower @0x822920C0, HandleVehicleVehicleSparks @0x82296790,
HandleRaceCarRaceCarSparks @0x82290A48, HandleBurstDebris @0x82290BC8, ParticleModule::SpawnSparkShowerFromPoint
@0x8228AFC0 and ParticleModule::FireDebrisBurst @0x82289E70.

This is what a player SEES at a scrape or a crash on the console: world-grinding and vehicle-grinding spark showers
(BurstAccumulator-paced), the big crash shower (100..250 sparks), the crashing / takedown debris bursts and the crash
impact dust. Before this fix the drain announced itself NOT RECONSTRUCTED and none of it existed.

  1. WIRING -- the drain runs (no announcement); the race-car spark switch is the image's TRUE (byte_82CDB40D =
     0x01, initialised .data: the console never posts HandleRaceCarRaceCarSparks' record); the constants are the
     image's, cited by address; the two producers post record types 2 and 5.
  2. NUMERIC -- tests/FxCrashVfxRaceCarContacts.cpp compiles the PRODUCTION bodies onto a fixture and compares
     them, bit for bit, with the console's own outputs in tests/FxCrashVfxRaceCarContactsData.h: the drain's real
     instruction words and every callee down to the particle queue, run by
     scratch/CRASHPARITY_0922/fxcrashvfx_vmxemu/gen_prc_data.py.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxcrashvfx_race_car_contacts.py [--rev <b5 rev>]
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
PARTICLE_CPP = "src/GameSource/Effects/Particles/ParticleModule.cpp"
ARCD_CPP = "src/GameSource/Effects/ActiveRaceCarData.cpp"
UTILS_CPP = "src/GameSource/Effects/BrnEffectsUtils.cpp"
LAYOUT_CPP = "src/GameSource/Replays/Serialisers/BrnReplayEffectsSerialiserStaticLayout.cpp"
SHADOW_HEADERS = ("src/GameSource/Effects/ActiveRaceCarData.h",
                  "src/GameSource/Effects/BrnEffectsUtils.h",
                  "src/GameSource/Effects/Particles/BrnParticleModuleIO_EventTypes.h",
                  "src/GameSource/Effects/Particles/Native/BrnSimpleFxDiag.h")
RANDOM_CPP = REPO / "src/GameShared/GameClasses/Numeric/CgsRandom.cpp"

PRC_SIGNATURE = "void EffectsModule::ProcessRaceCarContacts("
REGION_MARK = "THE RACE-CAR CONTACT DRAIN AND ITS SHOWERS"
STRUCTS = ("struct SparkShowerArgs\n{", "struct SparkShowerController\n{")
CONTROLLERS = ("gSparkShowerControllerWorldGrinding", "gSparkShowerControllerVehicleGrinding",
               "gSparkShowerControllerCrashing")
HELPERS = ("f32 Dot3(", "f32 Vnmsub(", "f32 RefinedRsqrt(", "f32 RefinedRecip(", "f32 GuardedLength3(",
           "Vector3 Scale4(", "Vector3 Negate4(", "VecFloat Splat(", "u32 FctidzLowWord(", "Vector3 CrossPermuted(",
           "const u8* DebrisParamsLayout(", "f32 LayoutFloat(", "const Attrib::RefSpec& VfxSurfaceRef(")
PRODUCERS = ("void ParticleModule::SpawnSparkShowerFromPoint(", "void ParticleModule::FireDebrisBurst(")
RANDOMISERS = ("void Vector3Randomiser::Prepare(", "void Vector4Randomiser::Prepare(",
               "Vector3 Vector3Randomiser::RandomiseXYZ(", "Vector4 Vector4Randomiser::RandomiseXYZW(")
BURST = "u32 BurstAccumulator::Update(f32 lfBurstSize, f32 lfTime, CgsNumeric::Random& lrRandom)"
LAYOUT = ("void EffectsSerialiserStaticLayout::GetCarContact(", "int EffectsSerialiserStaticLayout::UpdateCarContact(")

# 12 cases x 8 checks + 2 global checks (see FxCrashVfxRaceCarContacts.cpp)
NUMERIC_CHECKS = 12 * 8 + 2


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


def drain_region(effects):
    """The drain and its callees as one text (the banner through the end of ProcessRaceCarContacts), or the
    revision's lone ProcessRaceCarContacts (the parent's announcing stub) when there is no such region."""
    prc = definition(effects, PRC_SIGNATURE)
    end = effects.index(prc) + len(prc)
    if REGION_MARK in effects:
        start = effects.rfind("\n", 0, effects.index(REGION_MARK)) + 1
        return effects[start:end], True
    return prc, False


def constants_for(effects, text):
    """Every `const <type> K*_NAME = ...;` the text names, transitively, in source order."""
    wanted, seen = set(), set()
    pending = set(re.findall(r"\bK[A-Z]{1,2}_[A-Z0-9_]+\b", code_only(text)))
    found = {}
    while pending:
        name = pending.pop()
        if name in seen or name == "KU_NUM_ACTIVE_RACE_CARS":
            continue
        seen.add(name)
        match = re.search(r"^[ \t]*const\s+[\w:]+\s+%s\s*=\s*[^;]+;" % re.escape(name), effects, flags=re.M)
        if match:
            found[name] = (match.start(), match.group(0).strip())
            wanted.add(name)
            pending |= set(re.findall(r"\bK[A-Z]{1,2}_[A-Z0-9_]+\b", code_only(match.group(0)))) - seen
    return [found[name][1] for name in sorted(wanted, key=lambda n: found[n][0])]


def pieces(tree):
    effects = normalised(tree, EFFECTS_CPP)
    region, landed = drain_region(effects)
    structs = [definition(effects, s) + ";" for s in STRUCTS if s in effects]
    controllers = []
    for name in CONTROLLERS:
        signature = "const SparkShowerController %s =" % name
        if signature in effects:
            controllers.append(definition(effects, signature) + ";")
    spawn_params = []
    if "struct CB4SparkSpawnParams" in effects:
        spawn_params.append(definition(effects, "struct CB4SparkSpawnParams") + ";")
        match = re.search(r"const CB4SparkSpawnParams gSparkSpawnParamsRaceCarVehicle\s*=\s*[^;]+;", effects)
        if match:
            spawn_params.append(match.group(0))
    helpers = optional_definitions(effects, HELPERS) if landed else []
    consts = constants_for(effects, "\n".join([region] + structs + controllers + spawn_params + helpers))
    return {
        "fxcrashvfx_prc_structs.inc": "\n".join(structs) + "\n",
        "fxcrashvfx_prc_consts.inc": "\n".join(consts + spawn_params + controllers + helpers) + "\n",
        "fxcrashvfx_prc_producers.inc": "\n".join(optional_definitions(normalised(tree, PARTICLE_CPP), PRODUCERS)) + "\n",
        "fxcrashvfx_prc_randomisers.inc": "\n".join(optional_definitions(normalised(tree, UTILS_CPP), RANDOMISERS)) + "\n",
        "fxcrashvfx_prc_burst.inc": "\n".join(optional_definitions(normalised(tree, ARCD_CPP), (BURST,))) + "\n",
        "fxcrashvfx_prc_layout.inc": "\n".join(optional_definitions(normalised(tree, LAYOUT_CPP), LAYOUT)) + "\n",
    }, region, effects, landed


def wiring(tree):
    effects = normalised(tree, EFFECTS_CPP)
    consts = code_only(effects)
    try:
        prc = code_only(definition(effects, PRC_SIGNATURE))
    except ValueError:
        prc = ""
    yield ("ProcessRaceCarContacts runs its body (no NOT RECONSTRUCTED announcement)",
           prc != "" and "LogNotReconstructed" not in prc and "HandleBurstDebris(" in prc)
    yield ("the race-car spark switch is the image's TRUE (byte_82CDB40D == 0x01, initialised .data)",
           re.search(r"const bool KB_RACE_CAR_SPARKS_DISABLED\s*=\s*true;", consts) is not None
           and "byte_82CDB40D" in effects and "0x00009501" in effects)
    wanted = (("KF_GRINDING_MIN_SPEED", "4.46944427f", "flt_82013720"),
              ("KF_CRASH_SHOWER_MIN_TANGENTIAL_SPEED", "6.70416689f", "flt_82013278"),
              ("KF_CRASH_SHOWER_SIZE_SLOPE", "0.0559353642f", "flt_820138A4"),
              ("KF_CRASH_SHOWER_INTERVAL_RANGE", "0.13000001f", "flt_820138A0"),
              ("KF_CRASH_SHOWER_INTERVAL_MIN", "0.0199999996f", "flt_82005574"),
              ("KF_CRASH_SHOWER_COUNT_RANGE", "150.0f", "flt_82006530"),
              ("KF_CRASH_SHOWER_COUNT_BASE", "100.5f", "flt_8201387C"),
              ("KF_GRINDING_WOBBLE_FREQUENCY", "2.51327419f", "flt_8201371C"),
              ("KF_CRASH_DUST_PER_METRE", "2.5f", "flt_82005548"),
              ("KF_CRASH_DUST_SIZE_RANGE", "0.400000036f", "flt_820138AC"),
              ("KF_TAKEDOWN_DEBRIS_COIN", "0.5f", "flt_82001DA0"))
    yield ("the drain's constants are the image's, each cited by address (flt_82013720 4.4694443, flt_82013278 "
           "6.7041669, flt_820138A4, flt_820138A0/82005574, flt_82006530/8201387C, flt_8201371C, flt_82005548, "
           "flt_820138AC, flt_82001DA0)",
           all(re.search(r"const f32 %s\s*=\s*%s;" % (name, re.escape(value)), consts) for name, value, _ in wanted)
           and all(address in effects for _, _, address in wanted))
    particle = normalised(tree, PARTICLE_CPP)
    producers = "".join(code_only(t) for t in optional_definitions(particle, PRODUCERS))
    yield ("SpawnSparkShowerFromPoint posts record type 2 and FireDebrisBurst record type 5 on the inter-thread queue",
           "eParticleEvent_SpawnSparkShowerFromPoint" in producers and "eParticleEvent_FireDebrisBurst" in producers)


def numeric(tree):
    try:
        incs, region, _, landed = pieces(tree)
    except ValueError as error:
        print("NUMERIC: cannot build -- " + str(error))
        return None
    if not landed:
        print("NUMERIC: this revision has no drain region -- measuring its lone ProcessRaceCarContacts")
    shadow = {relative: tree.read(relative) for relative in SHADOW_HEADERS if tree.read(relative)}
    return compile_and_run(Path(__file__).with_name("FxCrashVfxRaceCarContacts.cpp"), "fxcrashvfx_prc_body.inc",
                           region + "\n", "FxCrashVfxRaceCarContacts", shadow=shadow, extra_sources=(RANDOM_CPP,),
                           extra_files=incs)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", default=None, help="b5 revision to test (default: the working tree)")
    parser.add_argument("--root", default=None, help="a shadow tree root whose src/ files take precedence")
    args = parser.parse_args()
    tree = RootTree(args.rev, args.root)
    checks = list(wiring(tree))
    result = numeric(tree)
    return report("run_fxcrashvfx_race_car_contacts", checks, result, NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
