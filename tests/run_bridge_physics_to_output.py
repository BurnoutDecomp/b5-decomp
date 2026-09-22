"""Structural regression for WorldModule::BridgePhysicsToOutput @0x827AEB18.
Run from the workflow checkout: python b5-decomp/tests/run_bridge_physics_to_output.py [source.cpp]

The console body has six legs (ppcdis 0x827AEB18..0x827AEC04). Legs 4-6 were parked on PC until
2026-09-22, so world event 31 (VEHICLE_IMPACT) never reached the game state or the GUI. This check
extracts the production function from the real source file and requires the three console calls,
in the console's order, after leg 3; and UpdateOutputBuffer::Construct must construct the
deformation interface between the crash-network and sound-world-load constructs (0x827CA2C4).
The live counterpart is tests/RivalImpactEvents.ps1.
Optional argument: a pre-fix snapshot of WorldBridgeEntityModulesToOutput.cpp (the RED side).
"""
from pathlib import Path
import re
import sys

REPO = Path(__file__).resolve().parents[1]
BRIDGE = REPO / "src/GameSource/World/Bridges/WorldBridgeEntityModulesToOutput.cpp"
CONSTRUCT = REPO / "src/GameSource/World/BrnWorldModuleIO_UpdateOutputBuffer.cpp"


def function_body(source, signature):
    start = source.index(signature)
    brace = source.index("{", start)
    depth = 0
    for i in range(brace, len(source)):
        if source[i] == "{":
            depth += 1
        elif source[i] == "}":
            depth -= 1
            if depth == 0:
                return source[brace:i + 1]
    raise ValueError("unterminated body: " + signature)


def code_only(body):
    body = re.sub(r"/\*.*?\*/", "", body, flags=re.S)
    return "\n".join(line.split("//", 1)[0] for line in body.splitlines())


def main():
    bridge_path = Path(sys.argv[1]) if len(sys.argv) > 1 else BRIDGE
    failures = []
    checks = 0

    body = code_only(function_body(bridge_path.read_text(encoding="utf-8-sig"), "void BridgePhysicsToOutput("))
    order = [
        ("leg 1 VehicleOutputInterface copy", r"\*lpOutputBuffer->GetVehicleOutputInterface\(\)\s*=\s*\*lpPhysicsOutputBuffer->GetVehicleOutputInterface\(\)"),
        ("leg 2 contact spy copy", r"\*lpOutputBuffer->GetContactSpyInterface\(\)\s*=\s*\*lpPhysicsOutputBuffer->GetContactSpyInterface\(\)"),
        ("leg 3 vehicle-manager output copy", r"\*lpOutputBuffer->GetVehicleManagerOutputInterface\(\)\s*=\s*\*lpPhysicsOutputBuffer->GetVehicleManagerOutputInterface\(\)"),
        ("leg 4 SetDeformationOutputInterface (0x827AEBCC)", r"lpOutputBuffer->SetDeformationOutputInterface\(\s*lpPhysicsOutputBuffer->GetDeformationOutputInterface\(\)\s*\)"),
        ("leg 5 AppendGameEventQueue from physics +0x65F0 (0x827AEBE4)", r"lpOutputBuffer->AppendGameEventQueue\(\s*lpPhysicsOutputBuffer->GetVehicleOutputInterface\(\)->GetGameEventQueue\(\)\s*\)"),
        ("leg 6 AppendPropUpdateNotificationQueue (0x827AEC00)", r"lpOutputBuffer->AppendPropUpdateNotificationQueue\(\s*&lpPhysicsOutputBuffer->GetPropManagerOutputInterface\(\)->GetUpdatePropNotifications\(\)\s*\)"),
    ]
    last = -1
    for name, pattern in order:
        checks += 1
        m = re.search(pattern, body)
        if not m:
            failures.append("missing: " + name)
            continue
        checks += 1
        if m.start() < last:
            failures.append("out of console order: " + name)
        last = m.start()

    construct = code_only(function_body(CONSTRUCT.read_text(encoding="utf-8-sig"), "void UpdateOutputBuffer::Construct()"))
    checks += 1
    a = construct.find("mCrashNetworkOutputInterface.Construct()")
    b = construct.find("mDeformationOutputInterface.Construct()")
    c = construct.find("mSoundWorldLoadInterface.Construct()")
    if not (a >= 0 and b > a and c > b):
        failures.append("UpdateOutputBuffer::Construct must call mDeformationOutputInterface.Construct() between the "
                        "crash-network and sound-world-load constructs (X360 0x827CA2C4)")

    for f in failures:
        print("FAIL", f)
    print(f"BridgePhysicsToOutput: {checks} checks, {len(failures)} failures")
    sys.exit(1 if failures else 0)


if __name__ == "__main__":
    main()
