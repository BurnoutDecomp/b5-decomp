"""Regression for DeformationManager::SolvePenetration @0x82621B08 (crash parity G23-D1, FX-DEFORM-LAT).

Run from the workflow checkout:
    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxdeformlat_world_pseudo_body.py [--pre-fix <b5 rev>]

Every solve the console writes the WORLD pseudo-body slot: an inlined AddObject(28, identity, 0) at
0x82621B44..0x82621C1C (rows to solver+0x710..+0x740, zero weighting to +0x910). The tree never
wrote slot 28. The shipped statements between the phase-1 StartMonitor and the per-model loop are
extracted, with the TU's model-count constant and the real PenetrationSolver::AddObject, and run
on a poisoned solver.
"""
import re
import sys

sys.dont_write_bytecode = True
from pathlib import Path
from fxdeformlat_common import DEFORM, PHYS, build_and_run, definition, pre_fix_rev, read


def main():
    rev = pre_fix_rev(sys.argv)
    contacts = read(DEFORM + "/BrnDeformationManager_Contacts.cpp", rev)
    solver = read(PHYS + "/BrnPenetrationSolver.cpp", rev)
    fn = contacts.index("    void DeformationManager::SolvePenetration(")
    start = contacts.index("CgsDev::PerfMonCpu::StartMonitor(miPostPhysicsUpdateAddContactsToPenSolverPerfMon);", fn)
    end = contacts.index("for (s32 liModelIndex = mModelsAdded.GetFirstNonZeroBit();", start)
    head = contacts[start:end]
    count = re.search(r"const s32 KI_MAX_DEFORMATION_MODELS\s*=[^;]+;", contacts).group(0)
    methods = "\n".join(["namespace BrnPhysics { namespace Deformation {",
                         count,
                         definition(solver, "    void PenetrationSolver::AddObject("),
                         "} }"])
    rc = build_and_run(Path(__file__).with_name("FxDeformLatWorldPseudoBody.cpp"),
                       {"methods.inc": methods, "head.inc": head}, "fxdeformlat_world_pseudo_body",
                       open_access=True)
    sys.exit(rc)


if __name__ == "__main__":
    main()
