"""FX-TRAFFIC (crash parity 2026-09-23, G58-X2 / G58-X3 / G58-X4): the three console callers of
TrafficEntityModule::EnsureVehicleRemovedFromCrashModule @0x8271FBE8 that the PC left as logged
"no body" gates although the callee has been bodied since 2026-08-28.

  StopVehicleBeingPhysical @0x8271FED0   0x8271FFA4 `bl Ensure` -- unconditional, after the
                                          lbSuppressPhysicsRemoval-gated Append, BEFORE
                                          SetNotPhysical wipes muCrashTrafficType (0x8270F614).
  KillParam @0x82721FB8                  0x82722318 SetDead ; 0x82722324 Ensure(luParam) ; then a
                                          STANDARD cab whose trailer is not physical (0x82722370):
                                          Detach(trailer) 0x82722384, SetDead(trailer) 0x82722394,
                                          Ensure(trailer) 0x827223A0, Detach(cab) 0x827223B0.
  StaticVehicles_KillParam @0x82721C50   0x82721E00 SetDead ; 0x82721E18
                                          Ensure(GetVehicleIndexFromStaticIndex(luParam)).

Numeric: tests/FxTrafficCrashRelease.cpp compiled against the PRODUCTION bodies of the three
callers, EnsureVehicleRemovedFromCrashModule, the module accessors they use and the Param
helpers, all extracted from the source (working tree, or --rev <b5 rev>) and hosted on a fixture
that holds the real member types. Vehicle's four mutators are recorders in the fixture, so the
console CALL ORDER is checked, not only the end state.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxtraffic_crash_release.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, definition, compile_and_run, report, STRSTREAM_CPP

MODULE_CPP = "src/GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.cpp"
PARAM_CPP = "src/GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficParam.cpp"
FIXTURE = "CrashReleaseFixture"
NUMERIC_CHECKS = 31

MODULE_BODIES = [
    "    Vehicle* TrafficEntityModule::GetVehicle(u32",
    "    Vehicle* TrafficEntityModule::GetStaticVehicle(u32",
    "    StaticTrafficParam* TrafficEntityModule::GetStaticTrafficParam(u32",
    "    u32 TrafficEntityModule::GetVehicleIndexFromStaticIndex(u32",
    "    Param* TrafficEntityModule::GetParam(u32",
    "bool TrafficEntityModule::IsDecisionFrame()",
    "void TrafficEntityModule::PutParamInPurgatory(u32",
    "void TrafficPhysicsInfo::Destruct()",
    "void TrafficEntityModule::EnsureVehicleRemovedFromCrashModule(u32",
    # the three bodies under test
    "void TrafficEntityModule::StopVehicleBeingPhysical(u32",
    "void TrafficEntityModule::KillParam(u32",
    "void TrafficEntityModule::StaticVehicles_KillParam(u32",
]
PARAM_BODIES = [
    "void Param::SetInPurgatory(bool",
    "void Param::ClearDying(u32",
    "void Param::SetDyingState(u32",
]


def numeric(tree):
    module = tree.read(MODULE_CPP)
    param = tree.read(PARAM_CPP)
    parts = ["namespace BrnTraffic {"]
    mask = re.search(r"const u8 KU_STATIC_PARAM_KILL_KEEP_MASK\s*=[^;]+;", module)
    if mask is None:
        print("NUMERIC: cannot build -- KU_STATIC_PARAM_KILL_KEEP_MASK not found")
        return None
    parts.append(mask[0])
    for source, signatures in ((module, MODULE_BODIES), (param, PARAM_BODIES)):
        for signature in signatures:
            try:
                parts.append(definition(source, signature).replace("TrafficEntityModule::", FIXTURE + "::"))
            except ValueError:
                print("NUMERIC: cannot build -- production body absent: " + signature.strip())
                return None
    parts.append("}")
    return compile_and_run(Path(__file__).with_name("FxTrafficCrashRelease.cpp"), "crash_release.inc",
                           "\n".join(parts), "FxTrafficCrashRelease", extra_sources=[STRSTREAM_CPP])


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    args = parser.parse_args()
    return report("run_fxtraffic_crash_release", [], numeric(Tree(args.rev)), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
