"""L1 (owner's list 2026-09-28, item 2 "the camera is behind walls / below the map for crashes / takedowns /
junkyard", piece 7): BrnDirector::Camera::CollisionPolicyAttachedToVehicle, the collision policy of every camera that
hangs off a car (the chase cam BehaviourGameplayExternal -- also the crash state's fallback camera --, the gyro cam,
the ICE-anim car-relative takes, the aftertouch / deathcam / loose-attachment / orbit cameras).

  src/GameSource/Director/Camera/BrnCollisionPolicy.h                      (the class, its inline Construct)
  src/GameSource/Director/Camera/BrnCollisionPolicyAttachedToVehicle.cpp   (its out-of-line bodies)

Stage 7a (88cf3b6d) -- the DWARF layout and Construct @0x82224890. Three reserved spans and a flat float stood where
the DWARF puts mFrustrumCollisionResolver (+0x10), mCarToCamera (+0x170), mGroundConstraint (+0x1C0) and mPitchMover
(+0x230), so Construct could not seed them.
Stage 7b -- the scene-query pair, the plain arm. The class inherited CollisionPolicy's two empty bodies, so the
director's scene-query pass asked every car-attached camera to do nothing: the chase cam never pulled in front of the
wall between it and the car (L2's crash cell h225_s80: the fallback camera 8.3 m behind the car, on the far side of
the wall it hit). GenerateSceneQueries @0x82252690, ProcessSceneQueryResults @0x82252888, ResolveCollisions
@0x82224948, UpdateRadius @0x8220E4D0 and UpdateMinElevation @0x822405B8 are bodied; the FrustrumCollisionResolver
cameras (mbUseFrustrumResolver) stay on the empty pair until piece 6.
Stage 6a -- the frustum arm. The cameras that set mbUseFrustrumResolver (the aftertouch crash cam, the spiralling
deathcam, the loose-attachment and orbit cameras -- and on the console the chase cam and the ICE takes) returned from
both overrides: no near clip, no line tests, the eye left inside whatever it was in. The policy's arm (the traffic
resolution eased, the resolver handed the car) and FrustrumCollisionResolver's GenerateSceneQueries @0x82252540,
CalculateFrustumLineTests @0x8220DDE8, RequestFrustumLineTests @0x8223FD70 and ProcessSceneQueryResults @0x822242F8 are
bodied (ResolveVehicleCollisions @0x82223890 is 6b).

Numeric: tests/L1AttachedPolicy.cpp compiles against the revision's BrnCollisionPolicy.h (and includes its
BrnCollisionPolicyAttachedToVehicle.cpp) and replays tests/L1AttachedPolicyData.h, generated from the console's words
on emu64 by scratch/OWNERLIST_0927/L1/emu/gen_attached_policy.py (seed 929):
  group C (6) the store set of Construct(false) and Construct(true) from a pattern-filled policy;
  group P (5) 160 cases x 3 frames of Generate -> the car-to-camera answer -> Process through the base pointer, every
              other callee a recorder with the generator's deterministic effect: the calls and their arguments, the
              camera after each half, the policy's state, the asserts. A revision without the DWARF layout cannot
              build group P: its five checks are then counted failed.
  group F (5) 126 cases x 3 frames of the same on the FRUSTUM arm, the resolver interpreted whole on emu64 (its
              tunables written by the console's own CRT thunks) and XMVectorTan in the loop; the corner resolve is a
              recorder. Checked as group P, plus the camera's near clip and the four frustum boxes and the resolver's
              mVehicleResolveVector. Counted failed with group P when the layout is missing.
Wiring: the class names the DWARF members and overrides the pair.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_l1_attached_policy.py [--rev <b5 rev>]
"""
import argparse
import re
import sys

sys.dont_write_bytecode = True
from pathlib import Path
from fxgs_common import Tree, code_only, compile_and_run, report

POLICY_H = "src/GameSource/Director/Camera/BrnCollisionPolicy.h"
POLICY_CPP = "src/GameSource/Director/Camera/BrnCollisionPolicyAttachedToVehicle.cpp"
VEHICLEREF_CPP = Path(__file__).resolve().parents[1] / "src/GameSource/Director/Utils/BrnVehicleRef.cpp"
NUMERIC_CHECKS = 16   # C1..C6, P1..P5, F1..F5
UTILS_H = "src/GameSource/Director/Camera/Utils/CameraUtils.h"
TAN_H = "src/SDKs/XboxMath/XMVectorTan.h"


def _read(tree, path):
    try:
        return tree.read(path)
    except (OSError, ValueError):
        return ""


def attached_class(header):
    """The code of `class CollisionPolicyAttachedToVehicle ... };` (comments stripped), or ''."""
    code = code_only(header)
    match = re.search(r"class\s+CollisionPolicyAttachedToVehicle\s*:\s*public\s+CollisionPolicy\s*\{", code)
    if match is None:
        return ""
    depth = 0
    for index in range(match.end() - 1, len(code)):
        if code[index] == "{":
            depth += 1
        elif code[index] == "}":
            depth -= 1
            if depth == 0:
                return code[match.start():index + 1]
    return ""


def wiring(tree):
    body = attached_class(_read(tree, POLICY_H))
    for member, offset in (("FrustrumCollisionResolver\\s+mFrustrumCollisionResolver", "+0x10"),
                           ("LineTestNearestPostBox\\s+mCarToCamera", "+0x170"),
                           ("GroundConstraint\\s+mGroundConstraint", "+0x1C0"),
                           ("VehicleRef\\s+mAttachedTo", "+0x220"),
                           ("SmoothMover\\s+mPitchMover", "+0x230")):
        yield ("CollisionPolicyAttachedToVehicle names the DWARF member %s at %s" % (member.split("\\s+")[1], offset),
               re.search(member + r"\s*;", body) is not None)
    yield ("no reserved span is left in CollisionPolicyAttachedToVehicle",
           body != "" and re.search(r"\bmaReserved\w*\s*\[", body) is None)
    for name, address in (("GenerateSceneQueries", "@0x82252690"), ("ProcessSceneQueryResults", "@0x82252888")):
        yield ("CollisionPolicyAttachedToVehicle overrides %s (%s)" % (name, address),
               re.search(r"void\s+" + name + r"\s*\(\s*const\s+CollisionPolicySharedInfo\s*&\s*\w*\s*,\s*Camera\s*&"
                         r"\s*\w*\s*\)\s*override\s*;", body) is not None)


def numeric(tree):
    header = _read(tree, POLICY_H)
    source = _read(tree, POLICY_CPP)
    if "class CollisionPolicyAttachedToVehicle" not in header:
        print("NUMERIC: cannot build -- no CollisionPolicyAttachedToVehicle in BrnCollisionPolicy.h")
        return None
    body = attached_class(header)
    flags = "" if re.search(r"SmoothMover\s+mPitchMover\s*;", body) else "/DL1_NO_LAYOUT"
    shadow = {POLICY_H: header}
    for path in (UTILS_H, TAN_H):     # the revision's own, when it has them
        text = _read(tree, path)
        if text:
            shadow[path] = text
    return compile_and_run(Path(__file__).with_name("L1AttachedPolicy.cpp"), "l1_attached_policy.inc", source,
                           "L1AttachedPolicy", extra_flags=flags, shadow=shadow,
                           extra_sources=(VEHICLEREF_CPP,))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the sources from this git revision (RED: <fix>~1)")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_l1_attached_policy", list(wiring(tree)), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
