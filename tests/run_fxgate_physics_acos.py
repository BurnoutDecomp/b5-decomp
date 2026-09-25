"""FX-GATE item 6, the physics sites (crash parity 2026-09-25): every crash / drift physics site that the console
routes through XMVectorACos (X360 0x821F0980) calls XboxMath::XMVectorACos, and the clamp in front of it keeps the
console's NaN arm.

Each site is `clamp(dot, -1, 1)` then `bl XMVectorACos`:
  VehiclePhysics::GetSteeringAngle               vmaxfp 0x825D40FC / vminfp128 0x825D4100, bl 0x825D4104
  VehiclePhysics::GetMaxSteeringAngleDuringDrift fsel 0x825D3618 / 0x825D3624 (fpu Clamp: NaN -> 1.0), bl 0x825D3634
  VehiclePhysics::UpdateDriftScale               vmaxfp128 0x825FA8A0 / vminfp128 0x825FA8A4, bl 0x825FA8A8
  VehiclePhysics::ApplyDriftYaw                  vmaxfp 0x825D2678 / vminfp128 0x825D267C, bl 0x825D2680
  VehiclePhysics::ApplyDriftLatForce             vmaxfp 0x825D2BDC / vminfp128 0x825D2BE0, bl 0x825D2BE4
  VehicleManager::HandleRaceCarRaceCarContact    vmaxfp 0x82643524 / vminfp128 0x82643528, bl 0x8264352C
  DeformationManager::CalculateTangentPoints     vmaxfp 0x825DB5B8 / vminfp128 0x825DB5BC, bl 0x825DB5C4
vmaxfp / vminfp hand a NaN operand back, so a NaN dot reaches XMVectorACos as NaN; the fsel pair of the fpu Clamp
turns it into 1.0. The old UpdateDriftScale / ApplyDriftYaw / ApplyDriftLatForce clamp was std::min(1, std::max(-1, x)),
which turns a NaN into -1 (an angle of pi), and GetMaxSteeringAngleDuringDrift's if-clamp kept the NaN.

  1. WIRING -- each site's angle statement calls XboxMath::XMVectorACos(<the clamped dot>); no std::acos is left.
  2. NUMERIC -- the clamp statements between each site's Dot line and its angle line are EXTRACTED and run on NaN,
     +-inf, out-of-range and in-range dots against the console's clamp (tests/FxGatePhysicsACos.cpp).

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxgate_physics_acos.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, code_only, compile_and_run, definition, report

VP = "src/GameSource/Physics/VehicleManager/VehiclePhysics/VehiclePhysics.cpp"
VM = "src/GameSource/Physics/VehicleManager/BrnVehicleManager.cpp"
DM = "src/GameSource/Physics/DeformationManager/BrnDeformationManager_VehicleContactFixUp.cpp"
NUMERIC_CHECKS = 105

# (name, file, function signature, the Dot line (start), the angle statement's start (end), the clamped variable,
#  the console clamp: "vmx" keeps a NaN, "fsel" turns it into 1.0, the bl address)
SITES = [
    ("GetSteeringAngle", VP, "VecFloat VehiclePhysics::GetSteeringAngle() const",
     "f32 lfDot = vpu::Dot(lUnitVel, mTransform.zAxis);", "f32 lfAngle =", "lfDot", "vmx", "0x825D4104"),
    ("GetMaxSteeringAngleDuringDrift", VP, "f32 VehiclePhysics::GetMaxSteeringAngleDuringDrift(",
     "f32 lfForwardDot = vpu::Dot(lvUnitVelocity, mTransform.zAxis);", "const f32 lfVelocityAngle =", "lfForwardDot",
     "fsel", "0x825D3634"),
    ("UpdateDriftScale", VP, "void VehiclePhysics::UpdateDriftScale(",
     "f32 lfCosAngle = vpu::Dot(lForwardDir, lLinearVelocityDir);", "f32 lfDriftAngle =", "lfCosAngle", "vmx",
     "0x825FA8A8"),
    ("ApplyDriftYaw", VP, "void VehiclePhysics::ApplyDriftYaw(",
     "f32 lfCosAngle = vpu::Dot(lForwardDir, lLinearVelocityDir);", "f32 lfDriftAngle =", "lfCosAngle", "vmx",
     "0x825D2680"),
    ("ApplyDriftLatForce", VP, "void VehiclePhysics::ApplyDriftLatForce(",
     "f32 lfCosAngle = vpu::Dot(lUnitVel, mTransform.zAxis);", "const f32 lfAngle =", "lfCosAngle", "vmx",
     "0x825D2BE4"),
    ("HandleRaceCarRaceCarContact", VM, "void VehicleManager::HandleRaceCarRaceCarContact(",
     "f32 lfAlignment = vpu::Dot(lvForwardA, lvForwardB);", "lInfo.mfAngleBetweenCars =", "lfAlignment", "vmx",
     "0x8264352C"),
    ("CalculateTangentPoints", DM, "void DeformationManager::CalculateTangentPoints(",
     "f32 lfCosTheta = (lfRadiusA - lfRadiusB) * (1.0f / lfAToBDist);", "const f32 lfTheta =", "lfCosTheta", "vmx",
     "0x825DB5C4"),
]


def site_parts(tree, site):
    name, path, signature, start, end, var = site[:6]
    try:
        body = definition(tree.read(path).replace("\r\n", "\n"), signature)
    except ValueError:
        return None, None
    s = body.find(start)
    if s < 0:
        return None, None
    s += len(start)
    e = body.find(end, s)
    if e < 0:
        return None, None
    statement_end = body.find(";", e)
    return body[s:e], body[e:statement_end + 1]


def wiring(tree):
    for site in SITES:
        name, path, _, _, _, var, _, bl = site
        _, angle = site_parts(tree, site)
        ok = angle is not None and f"XboxMath::XMVectorACos({var})" in re.sub(r"\s+", "", code_only(angle)).replace(
            "XboxMath::XMVectorACos(", "XboxMath::XMVectorACos(")
        yield (f"{name}: bl XMVectorACos {bl} -> XboxMath::XMVectorACos({var})", ok)
    for path in sorted({site[1] for site in SITES}):
        code = code_only(tree.read(path))
        yield (f"{Path(path).name}: no std::acos / acosf left", "std::acos(" not in code
               and not re.search(r"(?<![\w:])acosf?\s*\(", code))


def numeric(tree):
    functions = []
    for site in SITES:
        name, _, _, _, _, var, kind, _ = site
        span, _ = site_parts(tree, site)
        if span is None:
            print(f"NUMERIC: cannot build -- {name}'s clamp span was not found")
            return None
        functions.append(f"static float Clamp_{name}(float {var})\n{{\n{span}\n    return {var};\n}}\n")
    table = ",\n".join(f'    {{ "{site[0]}", &Clamp_{site[0]}, {"true" if site[6] == "fsel" else "false"} }}'
                       for site in SITES)
    inc = "".join(functions) + "\nstatic const ClampSite kaSites[] = {\n" + table + "\n};\n"
    return compile_and_run(Path(__file__).with_name("FxGatePhysicsACos.cpp"), "fxgate_physics_acos.inc", inc,
                           "FxGatePhysicsACos")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxgate_physics_acos", list(wiring(tree)), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
