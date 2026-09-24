"""FX-TAILS-A item 5 (crash parity 2026-09-24): RaceCarPhysics::GetRecentBounce @0x825B8B08 has the DWARF signature.

DWARF RaceCarPhysics.h:319 declares `bool GetRecentBounce(int32_t *, bool *, bool *, bool *, bool *, EntityId *,
Vector3 *)` and the BrnPhysicsUnity2 dump names the parameters lpiBounceChain, lpbFromStationary, lpbOnCar,
lpbBoostedBounce, lpbGoodImpact, lpidImpactEntityId, lpContactPoint. The tree declared the sixth as `s32*` and
named the bools after guesses (lpOverMinStress / lpCarBounce / lpGoodImpact / lpExtraFlag), so its one caller,
VehicleManager::ProcessAftertouchEvents @0x82633DE8, had to hand the event record's EntityId over through a
reinterpret_cast. The console's stores are unchanged (0x825B8B68..0x825B8BA4: lhz+extsh+stw r4, stb r5..r8,
lwz+stw r9, lvx128+stvx128 r10) -- no behaviour change; the compile is the numeric test, this runner pins the text.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxtailsa_recent_bounce_signature.py [--rev <b5 rev>]
"""
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, body_or_empty, code_only, report

HEADER = "src/GameSource/Physics/VehicleManager/VehiclePhysics/RaceCarPhysics.h"
BODY = "src/GameSource/Physics/VehicleManager/VehiclePhysics/RaceCarPhysics.cpp"
CALLER = "src/GameSource/Physics/VehicleManager/BrnVehicleManager_UpdateVehiclePhysics.cpp"
DWARF = ("s32*lpiBounceChain,bool*lpbFromStationary,bool*lpbOnCar,bool*lpbBoostedBounce,bool*lpbGoodImpact,"
         "EntityId*lpidImpactEntityId,Vector3*lpContactPoint)")


def squash(text):
    return re.sub(r"\s+", "", code_only(text))


def wiring(tree):
    header = squash(tree.read(HEADER))
    yield ("RaceCarPhysics.h declares GetRecentBounce with the DWARF types and names (sixth out EntityId*)",
           ("boolGetRecentBounce(" + DWARF + ";") in header)
    body = squash(body_or_empty(tree.read(BODY), "bool RaceCarPhysics::GetRecentBounce("))
    yield ("the body keeps the console's seven stores, the entity word through EntityId::muValue",
           body.startswith("boolRaceCarPhysics::GetRecentBounce(" + DWARF)
           and "lpidImpactEntityId->muValue=static_cast<u32>(MS.miOtherEntityId);" in body
           and "*lpContactPoint=MS.mBounceDirection;" in body)
    caller = squash(body_or_empty(tree.read(CALLER), "void VehicleManager::ProcessAftertouchEvents("))
    yield ("ProcessAftertouchEvents hands the record's EntityId over as an EntityId* (no reinterpret_cast)",
           "&lBounce.mbGoodImpact,&lBounce.midImpactEntityId,&lBounce.mContactPoint))" in caller
           and "reinterpret_cast<s32*>(&lBounce.midImpactEntityId" not in caller)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    args = parser.parse_args()
    return report("run_fxtailsa_recent_bounce_signature", list(wiring(Tree(args.rev))), (0, 0), 0)


if __name__ == "__main__":
    sys.exit(main())
