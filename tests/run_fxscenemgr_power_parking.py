"""FX-SCENEMGR (crash parity 2026-09-24, item 4): the Power Parking CONSUMER -- the embedded
PowerParkingManager (module +0x18250; its Construct inlined at 0x822FDB14/0x822FDB1C, its Prepare
called at 0x82304048), RaceCarEntityModule::ProcessPowerParking (ARTIST 0x822CDF10) and
UpdatePowerParking (0x822FF5B0), their two gated call sites (PostSceneUpdate 0x822FE588,
PostPhysicsUpdate 0x8230778C) and the two tallies' writes (the inlined AddContactTraffic /
AddNearTraffic). All absent before: no member, no bodies, no calls -- the tallies sat on two
placeholder module seats nothing read.

Run from the workflow checkout:
    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxscenemgr_power_parking.py [--pre-fix <b5 rev>]
Extracts the bodies and the two gate conditions into tests/FxScenemgrPowerParking.cpp; a body the
revision lacks is replayed as an empty stand-in and a missing gate as `false`. Structural checks on
the call order and the member follow the harness.
"""
import re
import sys

sys.dont_write_bytecode = True
from fxrcem3_common import RCEM, REPO, build_and_run, code_mask, definition, optional_definition, pre_fix_rev, read

CRASHEXIT = RCEM + "BrnRaceCarEntityModule_CrashExit.cpp"
NEARMISS = RCEM + "BrnRaceCarEntityModule_NearMissTailgate.cpp"
MODULE = RCEM + "BrnRaceCarEntityModule.cpp"
HEADER = RCEM + "BrnRaceCarEntityModule.h"
PPM = RCEM + "PowerParking/BrnPowerParkingManager.cpp"
PPDC = RCEM + "PowerParking/BrnPowerParkingDebugComponent.cpp"
TRAFFIC_IF = RCEM + "SharedIO/BrnRaceCarToTrafficInterface.h"

STANDINS = {
    "process": ("void RaceCarEntityModule::ProcessPowerParking(const RaceCarEntityModuleIO::InputBuffer_PostScene*, "
                "RaceCarEntityModuleIO::OutputBuffer_PostScene*) {}"),
    "update": ("void RaceCarEntityModule::UpdatePowerParking(const RaceCarEntityModuleIO::InputBuffer_PostPhysics*, "
               "RaceCarEntityModuleIO::OutputBuffer_PostPhysics*) {}"),
    "construct": "void PowerParkingManager::Construct() {}",
    "dc_construct": "void PowerParkingDebugComponent::Construct(PowerParkingManager*) {}",
}


def bodied(source, signature):
    """The brace-balanced definition starting at `signature` when it is DEFINED there (the next
    code token after the parameter list is `{`), else ''."""
    start = source.find(signature)
    while start >= 0:
        tail = re.sub(r"//[^\n]*", "", source[start + len(signature):start + len(signature) + 400])
        if tail.lstrip().startswith("{"):
            return definition(source[start:], signature)
        start = source.find(signature, start + 1)
    return ""


def piece(source, signature, key, label):
    text = optional_definition(source, signature)
    print(("found   " if text else "MISSING ") + label)
    return text or STANDINS[key]


def gate_condition(body, call_pattern):
    """The condition of the `if` whose controlled statement is the call matching `call_pattern`
    (the last `if (` before the call, with nothing but `{` between its `)` and the call)."""
    masked = code_mask(body)
    call = re.search(call_pattern, masked)
    if not call:
        return None
    heads = list(re.finditer(r"\bif\s*\(", masked[:call.start()]))
    if not heads:
        return None
    open_at = heads[-1].end() - 1
    depth = 0
    for index in range(open_at, call.start()):
        if masked[index] == "(":
            depth += 1
        elif masked[index] == ")":
            depth -= 1
            if depth == 0:
                if masked[index + 1:call.start()].strip() not in ("{", ""):
                    return None
                return body[open_at + 1:index]
    return None


def order(masked, patterns):
    """Positions of each regex in `masked` (-1 when absent)."""
    found = []
    for pattern in patterns:
        match = re.search(pattern, masked)
        found.append(match.start() if match else -1)
    return found


def main():
    rev = pre_fix_rev(sys.argv)
    crashexit, nearmiss, module = read(CRASHEXIT, rev), read(NEARMISS, rev), read(MODULE, rev)
    header, ppm, ppdc = read(HEADER, rev), read(PPM, rev), read(PPDC, rev)

    process = piece(crashexit, "void RaceCarEntityModule::ProcessPowerParking(", "process",
                    "RaceCarEntityModule::ProcessPowerParking")
    update = piece(nearmiss, "void RaceCarEntityModule::UpdatePowerParking(", "update",
                   "RaceCarEntityModule::UpdatePowerParking")
    construct = piece(ppm, "void PowerParkingManager::Construct(", "construct", "PowerParkingManager::Construct")
    dc_construct = piece(ppdc, "void PowerParkingDebugComponent::Construct(", "dc_construct",
                         "PowerParkingDebugComponent::Construct")
    set_nearby = definition(ppm, "void PowerParkingManager::SetNearbyParkedTrafficData(")
    set_flag = bodied(read(TRAFFIC_IF, rev), "void SetFlag(Flag leFlag, bool lbValue)")
    print(("found   " if set_flag else "MISSING ") + "RaceCarToTrafficInterface::SetFlag body")
    set_flag = set_flag or "void SetFlag(Flag, bool) {}"

    post_scene = definition(crashexit, "void RaceCarEntityModule::PostSceneUpdate(")
    post_physics = definition(module, "void RaceCarEntityModule::PostPhysicsUpdate(")
    gates = []
    for name, body, pattern in (("SceneGate", post_scene, r"\bProcessPowerParking\s*\(\s*lpInput\s*,\s*lpOutput\s*\)"),
                                ("PhysicsGate", post_physics, r"\bUpdatePowerParking\s*\(\s*lpInput\s*,\s*lpOutput\s*\)")):
        condition = gate_condition(body, pattern)
        print(("found   " if condition else "MISSING ") + name + " (the call's `if` condition)")
        gates.append("bool %s() const { return (\n%s\n); }" % (name, condition if condition else "false"))

    pieces = {
        "fxsm_pp_manager.inc": "\n\n".join((dc_construct, construct, set_nearby)),
        "fxsm_pp_setflag.inc": set_flag,
        "fxsm_pp_process.inc": process,
        "fxsm_pp_update.inc": update,
        "fxsm_pp_gates.inc": "\n".join(gates),
    }
    rc = build_and_run(REPO / "tests" / "FxScenemgrPowerParking.cpp", pieces, "fxsm_pp")

    # ---- structural: the member, the lifecycle calls, the call order, the tallies ----------------------
    failures = []
    masked_header = code_mask(header)
    crash_at, manager_at = order(masked_header, [r"\bCrashPlayManager\s+mCrashPlayManager\s*;",
                                                 r"\bPowerParkingManager\s+mPowerParkingManager\s*;"])
    if manager_at < 0 or not crash_at < manager_at:
        failures.append("S1 RaceCarEntityModule embeds PowerParkingManager mPowerParkingManager right after "
                        "mCrashPlayManager (DWARF :356, X360 +0x18250)")
    seats = [name for name in ("miPowerParkingNearTrafficCount", "miPowerParkingContactTrafficCount")
             for text in (masked_header, code_mask(module), code_mask(nearmiss)) if name in text]
    if seats:
        failures.append("S2 the placeholder tally seats are retired (still referenced: %s)" % ", ".join(sorted(set(seats))))
    construct_body = code_mask(definition(module, "void RaceCarEntityModule::Construct()"))
    at = order(construct_body, [r"mCrashPlayManager\.Construct\(\s*\)", r"mPowerParkingManager\.Construct\(\s*\)"])
    if -1 in at or not at[0] < at[1]:
        failures.append("S3 RaceCarEntityModule::Construct runs the inlined PowerParkingManager::Construct after the "
                        "crash-play block (0x822FDB14 / 0x822FDB1C)")
    prepare_body = code_mask(definition(module, "bool RaceCarEntityModule::Prepare("))
    stage0 = prepare_body[:prepare_body.find("case 1:")] if "case 1:" in prepare_body else prepare_body
    at = order(stage0, [r"mBoostManager\.Prepare\(\s*\)", r"mPowerParkingManager\.Prepare\(\s*\)"])
    if -1 in at or not at[0] < at[1]:
        failures.append("S4 Prepare stage 0 calls PowerParkingManager::Prepare after BoostManager::Prepare "
                        "(0x82303FB8 then 0x82304048)")
    at = order(code_mask(post_scene), [r"SetShowtimeTrafficDensityScale\(",
                                       r"\bProcessPowerParking\s*\(\s*lpInput\s*,\s*lpOutput\s*\)",
                                       r"SendResetOnTrackRequests\("])
    if -1 in at or not at[0] < at[1] < at[2]:
        failures.append("S5 PostSceneUpdate calls ProcessPowerParking( lpInput, lpOutput ) after the Showtime publish "
                        "and before SendResetOnTrackRequests (0x822FE554 < 0x822FE588 < 0x822FE5A4)")
    at = order(code_mask(post_physics), [r"\bUpdateNearMisses\s*\(\s*lpInput\s*,\s*lpOutput\s*\)",
                                         r"\bUpdatePowerParking\s*\(\s*lpInput\s*,\s*lpOutput\s*\)",
                                         r"mAirTimeManager\.Update\("])
    if -1 in at or not at[0] < at[1] < at[2]:
        failures.append("S6 PostPhysicsUpdate calls UpdatePowerParking( lpInput, lpOutput ) right after UpdateNearMisses "
                        "(0x82307754 < 0x8230778C) and before the air-time tick")
    contacts = code_mask(definition(module, "void RaceCarEntityModule::UpdateRaceCarContacts("))
    if len(re.findall(r"mPowerParkingManager\.AddContactTraffic\(", contacts)) != 2:
        failures.append("S7 UpdateRaceCarContacts' two player-contact arms bump the manager's +0x64 through "
                        "AddContactTraffic (lwz/addi/stw 0x64(module + 0x18250))")
    near = code_mask(definition(nearmiss, "void RaceCarEntityModule::UpdateTrafficAndRaceCarNearMisses("))
    gate_at = near.find("if( lbCountForPowerParking )")
    add_at = [m.start() for m in re.finditer(r"mPowerParkingManager\.AddNearTraffic\(", near)]
    if len(add_at) != 1 or gate_at < 0 or not gate_at < add_at[0]:
        failures.append("S8 UpdateTrafficAndRaceCarNearMisses bumps the manager's +0x68 through AddNearTraffic, "
                        "under the power-parking gate")
    for failure in failures:
        print("FAIL:", failure)
    print(f"structural: 8 checks, {len(failures)} failures; harness rc={rc}")
    sys.exit(1 if (rc or failures) else 0)


if __name__ == "__main__":
    main()
