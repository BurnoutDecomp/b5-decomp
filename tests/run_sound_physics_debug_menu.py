"""Check the five Sound physics controls through actual body and RW accessor code."""
import argparse
import os
import re
from pathlib import Path
from fxgs_common import Tree, definition, compile_and_run, report

os.environ.pop("NoDefaultCurrentDirectoryInExePath", None)
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--rev")
args = parser.parse_args()
tree = Tree(args.rev)
source = tree.read("src/GameSource/Physics/PhysicsUtilities/ExternallySimulatedBody.cpp")
rigid = tree.read("src/vendor/renderware/physics/RigidBody.cpp")
try:
    globals_ = [line.strip() for line in source.splitlines()
                if re.match(r"\s*(?:s32|f32) K[IF]_(?:HOLD_CAR_|SPINNING_SPEED_)", line)]
    if len(globals_) != 5:
        raise ValueError("five original physics cells")
    code = "namespace BrnPhysics {\n" + "\n".join(globals_)
    code += "\n" + definition(source, "void ExternallySimulatedBody::ReadFromRenderware(") + "\n}\n"
    code += "namespace rw { namespace physics {\n"
    for signature in ("rw::math::vpu::Matrix44Affine RigidBody::GetTransform() const",
                      "rw::math::vpu::Vector3 RigidBody::GetLinearVelocity() const",
                      "rw::math::vpu::Vector3 RigidBody::GetAngularVelocity() const"):
        code += definition(rigid, signature) + "\n"
    code += "} }\n"
except ValueError as error:
    print("Missing production definition: " + str(error))
    raise SystemExit(1)
shadow = {"src/GameSource/Physics/PhysicsUtilities/ExternallySimulatedBody.h":
          tree.read("src/GameSource/Physics/PhysicsUtilities/ExternallySimulatedBody.h")}
numeric = compile_and_run(Path(__file__).with_name("SoundPhysicsDebugMenu.cpp"),
                          "sound_physics_debug_menu.inc", code, "SoundPhysicsDebugMenu", shadow=shadow)
raise SystemExit(report("run_sound_physics_debug_menu", [], numeric, 9))
