"""Run bounded ARTIST witnesses against the actual sky/grip consumer bodies.

The fixture uses the real color-data and occlusion declarations, with typed call
spies for the surrounding environment/window owners. It makes no object-layout,
GPU, full-page or hardware rsqrt/dot-precision claim. --rev 5ef9611d is the
negative control for the five source findings repaired after the 199-site review.
"""
import argparse
from pathlib import Path
import re
import sys

sys.dont_write_bytecode = True
import fxgs_common as fx

SKY = "src/GameSource/World/DebugComponents/BrnSkyDebugComponent.cpp"
SKY_H = SKY[:-3] + "h"
GRIP = "src/GameSource/Physics/VehicleManager/VehiclePhysics/BrnGripCurveDebugWindow.cpp"
OCCLUSION = "src/GameShared/GameClasses/Graphics/Dispatch/CgsOcclusionCullManager.h"
DATA = ["src/GameSource/World/EnvironmentSettings/BrnEnv" + name + "Data.h"
        for name in ("Scattering", "Lighting", "Clouds")]


def extracted(tree):
    sky, header, grip = tree.read(SKY), tree.read(SKY_H), tree.read(GRIP)
    member_start = header.index("        EnvironmentManager* mpEnvironmentManager;")
    member_end = header.index("\n    };", member_start)
    members = header[member_start:member_end]
    constants = "\n".join(re.findall(r"    const f32 KF_[^;]+;", sky))
    helpers = [fx.definition(sky, "    f32 ClockToSeconds(")]
    # Old revisions have the scalar normalization directly in Update.
    if "    f32 JunkyardFlush(" in sky:
        helpers += [fx.definition(sky, "    f32 " + name + "(") for name in (
            "JunkyardFlush", "JunkyardMultiply", "JunkyardMultiplyAdd",
            "JunkyardNegativeMultiplySubtract")]
        helpers += [fx.definition(sky, "    Vector3 NormalizeJunkyardKeyLightDirection(")]
    sky_bodies = [fx.definition(sky, "void DebugComponent::" + name + "(")
                  for name in ("DebugToManager", "Update")]
    grip_helpers = []
    if "        f32 GripFlush(" in grip:
        grip_helpers = [fx.definition(grip, "        f32 " + name + "(") for name in (
            "GripFlush", "GripMaximum", "GripMultiply", "InitialGripRange")]
    grip_bodies = [fx.definition(grip, "    " + result + " GripCurveDebugWindow::" + name + "(")
                   for result, name in (("void", "Update"), ("f32", "GetSlipRatioRange"),
                                        ("f32", "GetCoefficientRange"))]
    return "\n".join([
        "namespace BrnWorld { namespace EnvironmentSettings {",
        "// Test adapter: production field declarations; no engine base or layout claim.",
        "class DebugComponent { public:", members,
        "void DebugToManager(); void Update();",
        "void ManagerToDebug() { ++mpEnvironmentManager->managerReads; Push(4); }",
        "}; namespace {", constants, *helpers, "}", *sky_bodies, "}}",
        "namespace BrnPhysics { namespace Vehicle { namespace {", *grip_helpers,
        "}", *grip_bodies, "}}",
    ])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rev", help="extract production bodies/declarations from a b5 revision")
    parser.add_argument("--source-root", type=Path, help="b5 checkout to inspect")
    parser.add_argument("--workflow-root", type=Path, help="canonical build settings checkout")
    args = parser.parse_args()
    if args.source_root:
        fx.REPO = args.source_root.resolve()
    if args.workflow_root:
        fx.WORKFLOW = args.workflow_root.resolve()
    else:
        fx.WORKFLOW = next(parent for parent in fx.REPO.parents
                           if (parent / "tools/build/msvc_flags.txt").is_file())
    tree = fx.Tree(args.rev)
    shadow = {name: tree.read(name) for name in [OCCLUSION, *DATA]}
    numeric = fx.compile_and_run(
        Path(__file__).with_name("DebugMenuEnvironmentMath.cpp"),
        "debug_menu_environment_math.inc", extracted(tree), "DebugMenuEnvironmentMath",
        shadow=shadow)
    return fx.report("run_debug_menu_environment_math", [], numeric, 1)


if __name__ == "__main__":
    sys.exit(main())
