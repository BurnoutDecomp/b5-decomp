"""FX-TRAFFIC3 (crash parity wave 5, 2026-09-24, CC-2): the traffic contact-side detector.

  TrafficEntityModule::HandleContactPoints @0x827340C0 and ProcessContactPoint @0x82720C68 had no
  body, and PostPhysicsUpdate's RUNNING arm (meState == 1 -> 0x8274EA20) never made the call the
  console makes at 0x8274EA68, between HandleExternalResponses (0x8274EA50) and
  ProcessDeformationData (0x8274EA7C). TrafficPhysicsInfo::muContactSideFlags therefore stayed 0,
  UpdateVehicleStuckTimers never grew the front/back stuck timers, and GenerateDriverInputs' wedge
  arm (stuck > 0.05 s -> gas = brake = 0) never fired: a traffic car pinned at its nose or tail
  kept driving into the player.

Wiring: the RUNNING arm calls HandleContactPoints(lpInput) after HandleExternalResponses and before
ProcessDeformationData. Numeric: tests/FxTraffic3ContactPoints.cpp compiled against the PRODUCTION
bodies, constants and entity-id helpers (working tree, or --rev <b5 rev>) plus the production
InputBuffer_PostPhysics::GetContactSpyInterface; expectations are the ARTIST values.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxtraffic3_contact_points.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, definition, body_or_empty, compile_and_run, report, STRSTREAM_CPP

MODULE_CPP = "src/GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.cpp"
GETTERS_CPP = "src/GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModuleIO_InputBuffer_Getters.cpp"
FIXTURE = "ContactFixture"
NUMERIC_CHECKS = 32

# Production definitions (brace-balanced) from the module TU.
BODIES = [
    "    Vehicle* TrafficEntityModule::GetVehicle(u32 luIndex)",
    "    Matrix44Affine TrafficEntityModule::GetVehicleTransform(u32 luIndex) const",
    "    const VehicleTypeRuntime* TrafficEntityModule::GetVehicleTypeRuntime(u32 luVehicleType) const",
    "    TrafficPhysicsInfo* TrafficEntityModule::GetTrafficPhysicsInfoForVehicl(u32 luVehicle)",
    "void TrafficEntityModule::HandleContactPoints(",
    "void TrafficEntityModule::ProcessContactPoint(",
    "void TrafficEntityModule::DEBUG_RenderContactPoint(",
]
# Production one-line constants / counters (the entity-id geometry and the detector's constants).
CONSTANTS = [
    "KU_ENTITY_INDEX_SHIFT", "KU_ENTITY_INDEX_MASK", "KU_TRAFFIC_ENTITY_OWNER", "KU_ENTITY_OWNER_SHIFT",
    "KF_CONTACT_FRONT_BACK", "KF_CONTACT_DISCARD_SIDE", "KF_CONTACT_SIDE_DEBOUNCE_TIMER",
    "K_SIDE_STUCK_BOUNDING_BOX_ADD", "KF_DEBUG_CONTACT_ARROW_LENGTH",
    "KU_DEBUG_CONTACT_WORLD_COLOUR", "KU_DEBUG_CONTACT_OTHER_COLOUR",
    "KI_CONTACT_SIDE_DIAG_CAP", "giContactSideDiagLines", "giContactPassDiagLines",
]
HELPERS = [
    "inline u32 EntityIndexOf(EntityId lId)",
    "inline u32 EntityOwnerOf(EntityId lId)",
]


def constant_line(source, name):
    match = re.search(r"^[ \t]*(?:const[ \t]+)?[\w:]+[ \t]+" + re.escape(name) + r"[ \t]*=[^;]*;", source, re.M)
    if match is None:
        raise ValueError("constant absent: " + name)
    return match.group(0).strip()


def running_arm(module):
    post = body_or_empty(module, "void TrafficEntityModule::PostPhysicsUpdate(")
    start = post.find("case E_STATE_RUNNING:")
    end = post.find("case E_STATE_TEARING_DOWN:", start)
    return post[start:end] if 0 <= start < end else ""


def wiring(tree):
    arm = running_arm(tree.read(MODULE_CPP))
    external = arm.find("HandleExternalResponses(lpInput);")
    contacts = arm.find("HandleContactPoints(lpInput);")
    deformation = arm.find("ProcessDeformationData(")
    return [("PostPhysicsUpdate's RUNNING arm calls HandleContactPoints(lpInput) after HandleExternalResponses "
             "and before ProcessDeformationData (0x8274EA50 -> 0x8274EA68 -> 0x8274EA7C)",
             0 <= external < contacts < deformation)]


def numeric(tree):
    module = tree.read(MODULE_CPP)
    try:
        parts = ["namespace BrnTraffic {", "namespace {"]
        parts += [constant_line(module, name) for name in CONSTANTS]
        parts += [definition(module, helper) for helper in HELPERS]
        parts.append("}")
        parts += [definition(module, body).replace("TrafficEntityModule::", FIXTURE + "::", 1) for body in BODIES]
        parts.append("namespace BrnTrafficIO {")
        parts.append(definition(tree.read(GETTERS_CPP),
                                "    const InputBuffer_PostPhysics::ContactSpyInterface* "
                                "InputBuffer_PostPhysics::GetContactSpyInterface() const"))
        parts.append("}")
        parts.append("}")
    except ValueError as error:
        print("NUMERIC: cannot build -- production body absent: " + str(error))
        return None
    return compile_and_run(Path(__file__).with_name("FxTraffic3ContactPoints.cpp"), "contact_points.inc",
                           "\n".join(parts), "FxTraffic3ContactPoints", extra_sources=[STRSTREAM_CPP])


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxtraffic3_contact_points", wiring(tree), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
