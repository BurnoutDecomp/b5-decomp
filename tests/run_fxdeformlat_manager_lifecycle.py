"""Regression for DeformationManager::Construct @0x82621510 / ::Prepare @0x82630230
(crash parity G23-D2, G23-D3, G23-D4, FX-DEFORM-LAT).

Run from the workflow checkout:
    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxdeformlat_manager_lifecycle.py [--pre-fix <b5 rev>]

G23-D2: Construct clears the detached-wheel used-set (inlined DetachedWheelManager::Construct,
0x826215A8), sets mpaModels = NULL (0x8262177C) and maGlobalEntityIDs[0..27] = -1 (0x8262178C..98).
G23-D3: Prepare's tail clears the wheel used-set (0x82630404) and mStateOutput.mxLiveSlots (0x8263040C).
G23-D4: Prepare runs the inlined DeformableObject::Construct per model -- six stores, then
ClearVariables (0x826303B4..0x826303CC; PS3 0x6BEFC4).
The shipped bodies are extracted (Construct/Prepare with the static debug component and the placement
ctor swapped for fixtures) and run on 0xCD-poisoned storage of the real types.
"""
import re
import sys

sys.dont_write_bytecode = True
from pathlib import Path
from fxdeformlat_common import DEFORM, PHYS, REPO, build_and_run, definition, pre_fix_rev, read


def optional(source, signature):
    try:
        return definition(source, signature)
    except ValueError:
        return ""


def main():
    rev = pre_fix_rev(sys.argv)
    construct_tu = read(DEFORM + "/BrnDeformationManager_Construct.cpp", rev)
    manager_tu = read(DEFORM + "/BrnDeformationManager.cpp", rev)
    lifecycle = read(PHYS + "/BrnDeformableObject_Lifecycle.cpp", rev)
    wheels = read(PHYS + "/BrnDetachedWheelManager.cpp", rev)
    passer = read(PHYS + "/BrnImpulsePasser.cpp", rev)

    construct = definition(construct_tu, "    void DeformationManager::Construct()")
    construct = construct.replace("DeformationDebugComponent_Construct(&mDebugComponent, this);",
                                  "DeformationDebugComponent_Construct(nullptr, this);")
    prepare = definition(manager_tu, "    bool DeformationManager::Prepare(")
    prepare = prepare.replace("DeformationDebugComponent_Register(&mDebugComponent);",
                              "DeformationDebugComponent_Register(nullptr);")
    prepare = prepare.replace("new (&mpaModels[li]) DeformableObject();", "HarnessPlaceModel(&mpaModels[li]);")
    assert ("DeformationDebugComponent_Construct(nullptr, this);" in construct
            and "DeformationDebugComponent_Register(nullptr);" in prepare and "HarnessPlaceModel" in prepare)
    constants = re.findall(r"static const s32 KI_MAX_[A-Z_]+\s*=[^;]+;", construct_tu)
    inc = "\n".join([
        "namespace BrnPhysics { namespace Deformation {",
        *constants,
        definition(passer, "    void ImpulsePasser::ClearVariables()"),
        definition(passer, "    void ImpulsePasser::Construct()"),
        definition(lifecycle, "    void DeformableObject::ClearVariables()"),
        definition(lifecycle, "    void DeformableObject::SetLastLinearVelocity("),
        definition(lifecycle, "    void DeformableObject::SetEntitySphereSize("),
        optional(lifecycle, "    void DeformableObject_ClearVariables("),   # pre-fix trampoline
        optional(lifecycle, "    void DeformableObject::Construct()"),        # the fix
        optional(wheels, "    void DetachedWheelManager::Construct()"),       # the fix
        optional(wheels, "    bool DetachedWheelManager::Prepare()"),         # the fix
        construct,
        prepare,
        "} }",
    ])
    extra = [REPO / "src/GameShared/GameClasses/Numeric/CgsRandom.cpp"]
    rc = build_and_run(Path(__file__).with_name("FxDeformLatManagerLifecycle.cpp"), {"methods.inc": inc},
                       "fxdeformlat_manager_lifecycle", extra_sources=extra, open_access=True)
    sys.exit(rc)


if __name__ == "__main__":
    main()
