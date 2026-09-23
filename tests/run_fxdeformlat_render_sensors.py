"""Regression for DeformableObject::RenderSensors @0x825E08C0 (crash parity G17-D2, FX-DEFORM-LAT).

Run from the workflow checkout:
    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxdeformlat_render_sensors.py [--pre-fix <b5 rev>]

The console draws the deformation rig (DrawLine 0x825E0A08/0x825E0C50, DrawBox 0x825E0B60/0x825E0C28);
the tree's body was a no-draw stub behind a stale "renderer only forward-declared" FLAG. The
shipped RenderSensors, GetTransform and GetDeformationSensorSpec are extracted and run on the real
types with recorder DrawLine/DrawBox bodies.
"""
import sys

sys.dont_write_bytecode = True
from pathlib import Path
from fxdeformlat_common import PHYS, REPO, build_and_run, definition, pre_fix_rev, read


def main():
    rev = pre_fix_rev(sys.argv)
    update = read(PHYS + "/BrnDeformableObject_Update.cpp", rev)
    accessors = read(PHYS + "/BrnDeformableObject_Accessors.cpp", rev)
    spec = read(PHYS + "/BrnStreamedDeformationSpec.cpp", rev)
    inc = "\n".join([
        "namespace BrnPhysics { namespace Deformation {",
        "namespace vpu = rw::math::vpu;",
        definition(accessors, "    void DeformableObject::GetTransform("),
        definition(spec, "    const SensorSpec* StreamedDeformationSpec::GetDeformationSensorSpec("),
        definition(update, "    void DeformableObject::RenderSensors("),
        "} }",
    ])
    extra = [REPO / "src/GameShared/GameClasses/RenderWare/RwRGBA.cpp"]
    rc = build_and_run(Path(__file__).with_name("FxDeformLatRenderSensors.cpp"), {"methods.inc": inc},
                       "fxdeformlat_render_sensors", extra_sources=extra, open_access=True)
    sys.exit(rc)


if __name__ == "__main__":
    main()
