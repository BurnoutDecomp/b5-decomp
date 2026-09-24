"""Structural regression for the two crash-module bridges un-parked by crash parity FOLLOWUPS 18 (FX-XLANE).

Pure wiring, so the check is the console's CALL SEQUENCE, read from the production bodies in
src/GameSource/World/Bridges/WorldBridgeCrashInputs.cpp (the mounted TU):

  WorldModule::BridgeCrashModuleToPhysicsModule @0x827AAC70
    0x827AACD8 CrashIO::OutputBuffer_PreScene::GetVehicleInputInterface() const   (0x827A2488)
    0x827AACE4 PhysicsModuleIO::InputBuffer::GetVehicleInputInterface()           (0x8279ED28)
    0x827AACEC VehicleInputInterface::Append(<the crash side>)                    (0x823C87C0)

  WorldModule::BridgeInputToCrashModule @0x827ADEE8
    0x827ADF1C/20 the bounce byte -> SetPlayerPressingBoost
    0x827ADF24/30 GetCrashNetworkInterface       -> SetNetworkInputInterface
    0x827ADF38/44 GetGameActionQueue             -> SetGameActionQueue
    0x827ADF4C/58 GetVehicleDriverInputInterface -> SetVehicleDriverInterface
    0x827ADF60/6C GetTimerStatusInterface        -> SetTimerStatusInterface

Run from the workflow checkout:
    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxxlane_crash_bridges.py [--pre-fix <b5 rev>]
The parked bodies (b5 5e1d5a7f) fail 6/9 checks; the un-parked bodies pass 9/9.
"""
import re
import sys
from pathlib import Path

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from fxdeformlat_common import definition, pre_fix_rev, read

TU = "src/GameSource/World/Bridges/WorldBridgeCrashInputs.cpp"


def code_only(text):
    """Strip // and /* */ comments and string literals, so a comment or a log line can never satisfy a check."""
    text = re.sub(r'/\*[\s\S]*?\*/', ' ', text)
    text = re.sub(r'//[^\n]*', ' ', text)
    return re.sub(r'"(?:\\.|[^"\\])*"', '""', text)


def ordered(body, needles):
    """True when every regex in `needles` matches `body`, each after the previous one."""
    position = 0
    for needle in needles:
        m = re.compile(needle).search(body, position)
        if not m:
            return False
        position = m.end()
    return True


def main():
    rev = pre_fix_rev(sys.argv)
    text = read(TU, rev)
    to_physics = code_only(definition(text, "void BridgeCrashModuleToPhysicsModule("))
    input_to_crash = code_only(definition(text, "void BridgeInputToCrashModule("))

    checks = [
        ("to-physics: reads the crash output's vehicle-input interface (0x827AACD8)",
         re.search(r'lpCrashOutput_PreScene\s*->\s*GetVehicleInputInterface\s*\(\s*\)', to_physics) is not None),
        ("to-physics: appends it onto the physics input's vehicle-input interface (0x827AACE4 / 0x827AACEC)",
         re.search(r'lpPhysicsModuleInputBuffer\s*->\s*GetVehicleInputInterface\s*\(\s*\)\s*->\s*Append\s*\(', to_physics) is not None),
        ("to-physics: the crash-side getter runs before the physics getter + Append (console order)",
         ordered(to_physics, [r'lpCrashOutput_PreScene\s*->\s*GetVehicleInputInterface',
                              r'lpPhysicsModuleInputBuffer\s*->\s*GetVehicleInputInterface\s*\(\s*\)\s*->\s*Append'])),
        ("to-physics: both null tripwires kept (:58 / :59; control)",
         to_physics.count("CGS_ASSERT") == 2),
        ("input-to-crash: network leg SetNetworkInputInterface(GetCrashNetworkInterface()) (0x827ADF24/30)",
         re.search(r'SetNetworkInputInterface\s*\(\s*lpUpdateInputBuffer\s*->\s*GetCrashNetworkInterface\s*\(\s*\)\s*\)', input_to_crash) is not None),
        ("input-to-crash: vehicle-driver leg SetVehicleDriverInterface(GetVehicleDriverInputInterface()) (0x827ADF4C/58)",
         re.search(r'SetVehicleDriverInterface\s*\(\s*lpUpdateInputBuffer\s*->\s*GetVehicleDriverInputInterface\s*\(\s*\)\s*\)', input_to_crash) is not None),
        ("input-to-crash: the five legs in the console's order (boost, network, game actions, driver, timer)",
         ordered(input_to_crash, [r'SetPlayerPressingBoost', r'SetNetworkInputInterface', r'SetGameActionQueue',
                                  r'SetVehicleDriverInterface', r'SetTimerStatusInterface'])),
        ("input-to-crash: no reinterpret_cast on the network / driver legs (real types)",
         not re.search(r'reinterpret_cast[^;]*(GetCrashNetworkInterface|GetVehicleDriverInputInterface)', input_to_crash)),
        ("input-to-crash: game-action and timer legs kept (control)",
         ordered(input_to_crash, [r'SetGameActionQueue\s*\(\s*lpUpdateInputBuffer\s*->\s*GetGameActionQueue',
                                  r'SetTimerStatusInterface\s*\('])),
    ]
    failures = 0
    for name, passed in checks:
        if not passed:
            failures += 1
            print("FAIL: " + name)
    print(f"run_fxxlane_crash_bridges: {len(checks)} checks, {failures} failures")
    sys.exit(1 if failures else 0)


if __name__ == "__main__":
    main()
