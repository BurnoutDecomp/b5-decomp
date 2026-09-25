"""FX-CRASHVFX (crash parity 2026-09-25, item 6): THE PROP-STRIKE VFX -- BrnEffects::PropCollisions::Initialise
@0x822937A8, BuildPropToMaterialTable @0x82288640, UpdateLocatorVfx @0x822993A0 and VFXRuntimeMaterialLef::
TriggerLocators @0x82299168 / CreateEffect @0x822936F0.

What a player sees on the console when a prop is struck near the camera: the LION effects baked on the prop's
material locators (sparks off a lamp post, splinters off a fence), placed along the car's direction of travel. The
prop VFX locator queue PropEntityModule posts reaches the effects input every frame; the console's EffectsModule::
Update hands it to UpdateLocatorVfx. Before this work PropCollisions had no type, the queue went nowhere, and the one
body there was (TriggerLocators) ran all four rows of the effect's frame through the prop transform.

  1. WIRING -- the bodies exist and run unannounced; their literals are the image's, each cited; TriggerLocators
     carries only the locator's position through the prop transform (three fused rows) and refines vrsqrtefp.
  2. NUMERIC -- tests/FxCrashVfxPropCollisions.cpp compiles the PRODUCTION PropCollisions.cpp whole and compares the
     table Initialise builds, the LION calls, every placed effect's slot, the effect ring and mRandom, bit for bit,
     with 0x822937A8 / 0x822993A0 run on emu64 (scratch/CRASHPARITY_0922/fxcrashvfx_vmxemu/gen_propcoll_data.py;
     2 Initialise cases x 3 checks, 14 UpdateLocatorVfx cases x 4 checks).
  3. LOAD PATH (item 6b) -- ParticleModule embeds the real PropCollisions at +0x4270; LoadFXBundle stage 14 binds
     the VFX prop collection from the acquire reply, stage 17 asks the game-data module for the prop physics data
     (the LoadGameDataEvent PropEntityModule posts, pool 1, event id 17) and stage 18 binds its handle and runs
     Initialise; EffectsModule::Update runs UpdateLocatorVfx right after the glass smashes, with the debug
     component's material override, the player's race-car state and the camera.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxcrashvfx_prop_collisions.py
                                                                   [--rev <b5 rev>] [--root <shadow tree root>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import REPO, Tree, code_only, compile_and_run, definition, report

PROP_CPP = "src/GameSource/Effects/Props/PropCollisions.cpp"
MODULE_H = "src/GameSource/Effects/Particles/ParticleModule.h"
LIFECYCLE_CPP = "src/GameSource/Effects/Particles/ParticleModule_Lifecycle.cpp"
EFFECTS_CPP = "src/GameSource/Effects/EffectsModule.cpp"
SHADOW_HEADERS = ("src/GameSource/Effects/Props/PropCollisions.h",
                  "src/GameSource/Effects/Particles/ParticleModule.h")
EXTRA_SOURCES = (REPO / "src/GameShared/GameClasses/Numeric/CgsRandom.cpp",
                 REPO / "src/GameShared/GameClasses/System/Resource/CgsResourcePtr.cpp",
                 REPO / "src/GameShared/GameClasses/System/Resource/CgsBaseResourcePtr.cpp")

NUMERIC_CHECKS = 2 * 3 + 14 * 4


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
    source = tree.read(PROP_CPP)
    update = body(source, "void PropCollisions::UpdateLocatorVfx(")
    initialise = body(source, "void PropCollisions::Initialise(")
    build = body(source, "void PropCollisions::BuildPropToMaterialTable(")
    yield ("PropCollisions has its bodies (Initialise, BuildPropToMaterialTable, UpdateLocatorVfx), none announcing: "
           "the table from the collection and the physics data, the locator events into TriggerLocators",
           update != "" and initialise != "" and build != ""
           and "LogNotReconstructed" not in update + initialise + build
           and "BuildPropToMaterialTable();" in initialise and "mRandom.Construct();" in initialise
           and "VFXRuntimeMaterialLef::Initialise();" in initialise
           and "VFXRuntimeMaterialLef::TriggerLocators(" in update and "ContactVisible(" in update
           and "GetResourceId()" in build)
    consts = code_only(source)
    wanted = (("KF_PROP_VFX_VISIBLE_DISTANCE_SQR", "2500.0f", "flt_8200D510"),
              ("KF_PROP_VFX_SMASH_SPEED_MPH", "50.0f", "flt_820138DC"))
    yield ("the literals are the image's, each cited by address (flt_8200D510 2500 = 50 m squared, flt_820138DC 50 mph)",
           all(re.search(r"const f32 %s\s*=\s*%s;" % (name, re.escape(value)), consts) for name, value, _ in wanted)
           and all(re.search(r"const f32 %s\s*=\s*%s;\s*//\s*%s" % (name, re.escape(value), address), source)
                   for name, value, address in wanted))
    trigger = body(source, "void VFXRuntimeMaterialLef::TriggerLocators(")
    yield ("TriggerLocators carries only the locator's position through the prop transform (three fused rows) and "
           "stores the velocity basis as it is, refined rsqrt, no Normalize / TransformVector",
           trigger != "" and "TransformVector(" not in trigger and "TransformPoint(" not in trigger
           and "Normalize(" not in trigger and trigger.count("MaddSplat4(lPropTransform.") == 3
           and trigger.count("RefinedRsqrt(Dot3(") == 2 and "SetVelocity(" in trigger
           and "SetStateBlendFactor(lRandom.RandomFloat())" in trigger)


def segment(text, start, end):
    """The text from the first `start` to the next `end` after it ("" when either is absent)."""
    first = text.find(start)
    if first < 0:
        return ""
    last = text.find(end, first + len(start))
    return text[first:last] if last > first else ""


def load_path(tree):
    header = code_only(tree.read(MODULE_H))
    yield ("ParticleModule embeds the real BrnEffects::PropCollisions at +0x4270 (DWARF ParticleModule.h:28), not the "
           "ContainedListInterface + EffectPair[500] placeholder",
           re.search(r"BrnEffects::PropCollisions\s+mPropCollisions\s*;", header) is not None
           and "EffectPair" not in header and "ContainedListInterface" not in header)

    lifecycle = tree.read(LIFECYCLE_CPP)
    ladder = body(lifecycle, "bool ParticleModule::LoadFXBundle(")
    stage14 = segment(ladder, "case E_LOADSTAGE_WAIT_VFX_PROPS:", "case E_LOADSTAGE_ACQUIRE_TEXTURE_NAME_MAP:")
    yield ("LoadFXBundle stage 14 binds the VFX prop collection from every AcquireResourceResponse (type 4) answering "
           "stage 13's event id 0, else \"Invalid event id\" (0x8229CB80..0x8229CC1C)",
           "mPropCollisions.SetPropCollection(" in stage14 and "KI_EVENT_ACQUIRE_RESOURCE_RESPONSE" in stage14
           and "miEventId != 0" in stage14 and "LogNotReconstructed" not in stage14
           and re.search(r"KI_EVENT_ACQUIRE_RESOURCE_RESPONSE\s*=\s*4;", code_only(lifecycle)) is not None)
    stage17 = segment(ladder, "case E_LOADSTAGE_LOAD_PROP_COLLISIONS:", "case E_LOADSTAGE_DONE:")
    initialise = stage17.find("mPropCollisions.Initialise()")
    yield ("LoadFXBundle stage 17 posts LoadPropPhysics(&mReceiverQueue, 17, pool 1) and stage 18 waits for ONE reply, "
           "binds its mHandle, then Initialise (0x8229D51C..0x8229D618)",
           re.search(r"LoadPropPhysics\(\s*&mReceiverQueue,\s*static_cast<s32>\(E_LOADSTAGE_LOAD_PROP_COLLISIONS\),"
                     r"\s*KI_PROP_PHYSICS_POOL\)", stage17) is not None
           and re.search(r"KI_PROP_PHYSICS_POOL\s*=\s*1;", code_only(lifecycle)) is not None
           and "GetCount() < 1" in stage17
           and 0 < stage17.find("mPropCollisions.SetPropDataResource(lpLoaded->mHandle)") < initialise
           and "LogNotReconstructed" not in stage17)

    effects = tree.read(EFFECTS_CPP)
    update = body(effects, "void EffectsModule::Update(CgsModule::IOBufferStack*")
    glass = update.find("HandleGlassSmashEventsForAllCars(")
    call = update.find("mParticleModule.mPropCollisions.UpdateLocatorVfx(")
    step = update.find("mParticleModule.Update(")
    yield ("EffectsModule::Update runs UpdateLocatorVfx after the glass smashes and before the particle step, with "
           "mbEnable ? mMaterialIndex : -1 (+0x2C3A8 / +0x2C3AC), the player's state and the camera "
           "(0x8229F3D8..0x8229F438)",
           0 < glass < call < step
           and re.search(r"lrPropParams\.mbEnable\s*\?\s*static_cast<s32>\(lrPropParams\.mMaterialIndex\)\s*:\s*-1",
                         update) is not None
           and re.search(r"UpdateLocatorVfx\(lfDt,\s*lfTime,\s*mParticleModule,\s*\*lpInputBuffer->"
                         r"GetPropVFXLocatorQueue\(\),\s*liMaterialOverride,\s*lpPlayerRaceCarState,\s*lpCamera\)",
                         update) is not None
           and "LogNotReconstructed" not in update[glass:step])


def numeric(tree):
    source = tree.read(PROP_CPP)
    shadow = {relative: tree.read(relative) for relative in SHADOW_HEADERS if tree.read(relative)}
    return compile_and_run(Path(__file__).with_name("FxCrashVfxPropCollisions.cpp"), "fxcrashvfx_propcoll_tu.inc",
                           source + "\n", "FxCrashVfxPropCollisions", shadow=shadow, extra_sources=EXTRA_SOURCES)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", default=None, help="b5 revision to test (default: the working tree)")
    parser.add_argument("--root", default=None, help="a shadow tree root whose src/ files take precedence")
    args = parser.parse_args()
    tree = RootTree(args.rev, args.root)
    checks = list(wiring(tree)) + list(load_path(tree))
    result = numeric(tree)
    return report("run_fxcrashvfx_prop_collisions", checks, result, NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
