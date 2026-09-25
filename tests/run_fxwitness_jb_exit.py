"""Regression for the FX-WITNESS hinge witnesses (crash parity 2026-09-24).

Run from the workflow checkout:
    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxwitness_jb_exit.py [--pre-fix <b5 rev>]

NUMERIC (FxWitnessJbExit.cpp): the [jb-exit] decode in DetachedPartManager::TestJointForBreaking must
name the exit the REAL PhysicalBodyPart::TestJointForBreaking @0x8260C0F8 body took (g2 / g3a / g3b /
g3c / short / BREAK) and report the exact ratios that body computed, from the census counters and the
JbDecade ratio ring alone. Extracted: the census block + JbDecade + ReadJointBreakCensusDiag
(BrnPhysicalBodyPart.cpp), JbExitDecoded + JbExitDecode (BrnDetachedPartManager.cpp), and the body
itself, re-hosted on run_joint_break.py's stand-ins.

STRUCTURAL: the witnesses may not change the console path they watch.
  * TestJointForBreaking's body names none of the witness symbols (run_joint_break.py compiles that
    body against its own stand-ins; the witness lives in the forwarder);
  * the forwarder returns exactly the body's result, and both witness calls sit under its env latch;
  * every witness is behind its own getenv: BRN_JB_EXIT_DIAG, BRN_IK_CADENCE_DIAG,
    BRN_ABSORB_PER_ENTITY, BRN_AIR_DIAG.
On the pre-witness source the pieces do not exist, so the runner fails (the RED side).
"""
import re
import sys

sys.dont_write_bytecode = True
from pathlib import Path
from fxdeformlat_common import PHYS, build_and_run, definition, pre_fix_rev, read

VEHPHYS = "src/GameSource/Physics/VehicleManager/VehiclePhysics/VehiclePhysics.cpp"


def census_block(tu):
    start = tu.index("        u32 gxJbCalls = 0,")
    jb = definition(tu, "        inline u32 JbDecade(f32 lfRatio)")
    end = tu.index(jb) + len(jb)
    return tu[start:end]


def main():
    rev = pre_fix_rev(sys.argv)
    part = read(PHYS + "/BrnPhysicalBodyPart.cpp", rev)
    mgr = read(PHYS + "/BrnDetachedPartManager.cpp", rev)
    update = read(PHYS + "/BrnDeformableObject_Update.cpp", rev)
    detach = read(PHYS + "/BrnDeformableObject_Detach.cpp", rev)
    vehicle = read(VEHPHYS, rev)

    failures = []
    pieces = {}
    try:
        pieces["jb_census.inc"] = census_block(part)
        if "guJbRatioCount" not in pieces["jb_census.inc"]:
            raise ValueError("JbDecade has no ratio ring (guJbRatioCount)")
        pieces["jb_read.inc"] = definition(part, "    void ReadJointBreakCensusDiag(")
        pieces["jb_decode.inc"] = (definition(mgr, "        struct JbExitDecoded") + ";\n"
                                   + definition(mgr, "        JbExitDecoded JbExitDecode("))
    except ValueError as err:
        failures.append(f"a witness piece is absent: {err}")

    body = definition(part, "    bool PhysicalBodyPart::TestJointForBreaking(")
    for symbol in ("JbExit", "guDiagDeformationStep", "JointBreakCensusDiag", "ReadJointBreakCensusDiag"):
        if symbol in body:
            failures.append(f"TestJointForBreaking's body names witness symbol {symbol}")
    constants = "\n".join(re.findall(
        r"const f32\s+KF_(?:JOINT_DETACH_DISABLED_THRESHOLD|ROTATION_PROPORTION_GATE|INERTIA_DEGENERATE_EPSILON)[^;]+;",
        part))
    pieces["jb_body.inc"] = constants + "\n" + body.replace("PhysicalBodyPart::", "JointBreakFixture::") \
        .replace("BrnPhysics::PhysicsModuleIO::OutputBuffer*", "OutputFixture*")

    fwd = definition(mgr, "    bool DetachedPartManager::TestJointForBreaking(")
    if "const bool lbBroke = lpPart->TestJointForBreaking(lpSimInput, lpOutput);" not in fwd \
            or "return lbBroke;" not in fwd:
        failures.append("the forwarder must return exactly the body's result")
    if not re.search(r"if \( lbJbExit \)\s*\{\s*JbExitCapturePre\(", fwd) \
            or not re.search(r"if \( lbJbExit \)\s*\{\s*JbExitReport\(", fwd):
        failures.append("both [jb-exit] calls must sit under the BRN_JB_EXIT_DIAG latch")

    gates = ((mgr, 'getenv("BRN_JB_EXIT_DIAG")', "BrnDetachedPartManager.cpp"),
             (detach, 'getenv("BRN_JB_EXIT_DIAG")', "BrnDeformableObject_Detach.cpp"),
             (part, 'getenv("BRN_JB_EXIT_DIAG")', "BrnPhysicalBodyPart.cpp"),
             (update, 'getenv("BRN_IK_CADENCE_DIAG")', "BrnDeformableObject_Update.cpp"),
             (update, 'getenv( "BRN_ABSORB_PER_ENTITY" )', "BrnDeformableObject_Update.cpp"),
             (vehicle, 'getenv("BRN_AIR_DIAG")', "VehiclePhysics.cpp"))
    for text, gate, name in gates:
        if gate not in text:
            failures.append(f"{name}: no {gate} latch")
    for f in failures:
        print("FAIL:", f)
    print(f"structural: 11 checks, {len(failures)} failures")

    rc = 1
    if "jb_census.inc" in pieces and "jb_decode.inc" in pieces:
        rc = build_and_run(Path(__file__).with_name("FxWitnessJbExit.cpp"), pieces, "fxwitness_jb_exit")
    sys.exit(1 if (rc or failures) else 0)


if __name__ == "__main__":
    main()
