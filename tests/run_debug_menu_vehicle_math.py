"""Exercise repaired vehicle menu consumers against ARTIST-derived witnesses.

Extracts the actual takeoff ramp, wheelie threshold/bleed and preset callback.
This checks bounded production arithmetic and dispatch, not a whole simulation.
The reciprocal seed's hardware estimate bits remain outside this fixture.
--as-std is a negative control: the 1.5-degree sine witness must fail.
"""
import argparse
from pathlib import Path
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, compile_and_run, definition, report

VEHICLE = "src/GameSource/Physics/VehicleManager/VehiclePhysics/VehiclePhysics.cpp"
DEFORM = "src/GameSource/Physics/DeformationManager/BrnDeformationDebugComponent.cpp"


def extracted(preset_rev=None):
    vehicle = Tree().read(VEHICLE)
    deform = Tree(preset_rev).read(DEFORM)
    helpers = [definition(vehicle, "        f32 " + name + "(") for name in (
        "TakeoffFlush", "TakeoffMultiply", "TakeoffSubtract", "TakeoffMultiplyAdd",
        "TakeoffNegativeMultiplySubtract", "TakeoffReciprocal")]
    start = vehicle.index("            const f32 lfRollAmount = TakeoffFlush(")
    end = vehicle.index("            // The yaw damping", start)
    ramp = vehicle[start:end]
    threshold_start = vehicle.index("            const f32 lfWheelieSin =")
    threshold_end = vehicle.index(";", threshold_start) + 1
    threshold = vehicle[threshold_start:threshold_end]
    bleed = definition(vehicle, "            if (lfPitch > lfWheelieSin)")
    preset = definition(deform, "    void DeformationDebugComponent::OnCompressionPresetChange(")
    return "\n".join(helpers) + "\n" + """
float TakeoffRamp(float roll, float full, float noDamp, float minimum)
{
    const VecFloat lvfAbsXAxisY = vpu::Splat(roll);
    const VecFloat lvfXAxisYFullDampThreshold = vpu::Splat(full);
    const VecFloat lvfXAxisYNoDampThreshold = vpu::Splat(noDamp);
    const VecFloat lvfMinRollFactor = vpu::Splat(minimum);
    const VecFloat lvfOne = vpu::Splat(1.0f);
""" + ramp + "\nreturn lvfRollFactor.x;\n}\n" + """
void VehicleFixture::WheelieLimit(float lfPitch, float gfMaxWheelieAngle)
{
    const f32 KF_DEG_TO_RAD = Float(0x3C8EFA35u); // ARTIST 8208F5F4
""" + threshold + "\n" + bleed + "\n}\n" + preset


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--as-std", action="store_true")
    parser.add_argument("--preset-rev", help="negative control: extract the preset from this revision")
    args = parser.parse_args()
    numeric = compile_and_run(
        Path(__file__).with_name("DebugMenuVehicleMath.cpp"),
        "debug_menu_vehicle_math.inc", extracted(args.preset_rev), "DebugMenuVehicleMath",
        extra_flags="/DDEBUG_MENU_USE_STD_SIN" if args.as_std else "")
    return report("run_debug_menu_vehicle_math", [], numeric, 1)


if __name__ == "__main__":
    sys.exit(main())
