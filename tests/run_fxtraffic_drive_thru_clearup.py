"""FX-TRAFFIC (crash parity 2026-09-23, G59-D1): the traffic module's drive-thru clean-up.

  HandleExternalRequests @0x8274B660   jump-table cases 84..87 (action ids 97..100: BODY_SHOP,
                                       PAINT_SHOP, DRIVE_THRU_JUNK_YARD, GAS_STATION) -> 0x8274BFA8:
                                       offline only (lbzx +0x717DC), ClearupCrashedTraffic() then
                                       KillAllTrafficInCylinder(mLocalPlayerPosition, 250.0
                                       flt_82004A24, 1000.0 flt_820BA604, false).
  ClearupCrashedTraffic @0x8273CBE0    snapshot of physical & alive; fatal or GIVE_UP -> RemoveVehicle.
  KillAllTrafficInCylinder @0x82741C58 alive, non-parked unless the bool, d^2 <= r^2 (y zeroed),
                                       |dy| < h -> RemoveVehicle.
Before the fix neither callee had a body and 97..100 fell into `default: break`.

Numeric: tests/FxTrafficDriveThruClearup.cpp compiled against the PRODUCTION bodies extracted from
the source (working tree, or --rev <b5 rev>), hosted on a fixture with the real member types;
RemoveVehicle is a recorder. A revision without a callee body gets a labelled empty stand-in.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxtraffic_drive_thru_clearup.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, definition, compile_and_run, report, STRSTREAM_CPP, REPO

MODULE_CPP = "src/GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.cpp"
VEHICLE_CPP = "src/GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficVehicle.cpp"
FIXTURE = "DriveThruFixture"
NUMERIC_CHECKS = 15

REQUIRED = [
    (MODULE_CPP, "    Vehicle* TrafficEntityModule::GetVehicle(u32"),
    (MODULE_CPP, "    Matrix44Affine TrafficEntityModule::GetVehicleTransform(u32"),
    (MODULE_CPP, "    TrafficPhysicsInfo* TrafficEntityModule::GetTrafficPhysicsInfoForVehicl(u32"),
    (VEHICLE_CPP, "Vehicle::Manoeuvre Vehicle::GetCurrentManoeuvre() const"),
    (MODULE_CPP, "void TrafficEntityModule::HandleExternalRequests("),
]
OPTIONAL = [
    (MODULE_CPP, "void TrafficEntityModule::ClearupCrashedTraffic()",
     "void DriveThruFixture::ClearupCrashedTraffic() {}"),
    (MODULE_CPP, "void TrafficEntityModule::KillAllTrafficInCylinder(",
     "void DriveThruFixture::KillAllTrafficInCylinder(Vector3, f32, f32, bool) {}"),
]
# The queue template's explicit instantiation (its bodies are not all inline).
QUEUE_CPP = REPO / "src/GameShared/GameClasses/Module/VariableEventQueue_13312_16.cpp"


def numeric(tree):
    parts, stood_in = ["namespace BrnTraffic {"], []
    for relative, signature in REQUIRED:
        try:
            parts.append(definition(tree.read(relative), signature).replace("TrafficEntityModule::", FIXTURE + "::"))
        except ValueError:
            print("NUMERIC: cannot build -- production body absent: " + signature.strip())
            return None
    for relative, signature, stand_in in OPTIONAL:
        try:
            parts.append(definition(tree.read(relative), signature).replace("TrafficEntityModule::", FIXTURE + "::"))
        except ValueError:
            parts.append("// [stand-in: body absent in this revision]\n" + stand_in)
            stood_in.append(signature.split("::", 1)[1].strip())
    parts.append("}")
    if stood_in:
        print("NUMERIC: bodies absent in this revision (empty stand-ins): " + ", ".join(stood_in))
    return compile_and_run(Path(__file__).with_name("FxTrafficDriveThruClearup.cpp"), "drive_thru_clearup.inc",
                           "\n".join(parts), "FxTrafficDriveThruClearup",
                           extra_sources=[STRSTREAM_CPP, QUEUE_CPP])


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    args = parser.parse_args()
    return report("run_fxtraffic_drive_thru_clearup", [], numeric(Tree(args.rev)), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
