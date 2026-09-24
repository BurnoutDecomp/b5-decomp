"""FX-RUMBLE3 (crash parity 2026-09-24, G10-D4): the input module plays the rumble requests on the pads.

The console's consumer of the pre-world rumble buffer is CgsInput::InputModule::PreWorldUpdate @0x82903328 ->
ProcessRumbleRequests @0x828FFE50 -> InputPads (PlayJoltEvent / StopRumbleEvent / PlayRumbleEvent /
ChangeVolumeRumbleEvent, then UpdateRumble -> UpdatePadRumble @0x828EFC58 per port) -> DeviceX360Pad::SetRumble
@0x828E78D0 -> XInputSetState. On PC none of the module half ran (BrnGameModule embedded an empty InputModule
stub), SetRumble had no body, UpdateRumble / BindPlayerToPort did not exist, ChangeVolumeRumbleEvent dropped the
event's envelope, and UpdatePadRumble's connected gate read an invented "force rumble" global instead of
DeviceX360Pad::IsConnected() (whose OR is the automated-testing byte, HardwareInit @0x83085F80).

  1. WIRING -- InputModule::PreWorldUpdate / ProcessRumbleRequests / Construct in the console's order, and the
     PC seats (the pad scan, the motor leaf) where the console's calls are.
  2. NUMERIC, two programs:
     tests/FxRumble3PadEvents.cpp -- the PRODUCTION InputPads event handlers + envelope walk with a recording
         SetRumble; builds on the old bodies too, so it shows what they got wrong (the envelope copy, the
         automated-testing gate).
     tests/FxRumble3PadChain.cpp -- the whole chain on the production bodies (module, pads, device, IO) down to
         recording XInputSetState / XInputFFSetRumble leaves: jolt to motor speeds, pause / enable / !lbUpdatePads,
         the event order, SetRumble's failure restore, the wheel arm, BindPlayerToPort, the bind-result hand-over.
     The motor speeds are float32 arithmetic on binary fractions:
         pad   (0.75, 0.625) -> (42566, 40959)    (0.5, 0) -> (23170, 0)    (0.25, 0.125) -> (8191, 8191)
               (0.5, 0.25) -> (23170, 16383)       left = (f32)pow(l, 1.5) * 65535, right = r * 65535, truncated
         wheel (0.0625, 0.03125) -> (40959, 20479)  (0.01, 0) -> (6553, 0)   x 655350, Clamp [0, 65535], truncated

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxrumble3_pad_rumble.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import REPO, STRSTREAM_CPP, Tree, body_or_empty, code_only, compile_and_run, definition, report

PADS_H = "src/GameShared/GameClasses/System/Input/CgsInputPads.h"
PADS_CPP = "src/GameShared/GameClasses/System/Input/CgsInputPads.cpp"
DEVICE_H = "src/GameShared/GameClasses/System/Input/Devices/X360/CgsInputDeviceX360Pad.h"
DEVICE_CPP = "src/GameShared/GameClasses/System/Input/Devices/X360/CgsInputDeviceX360Pad.cpp"
TYPES_H = "src/GameShared/GameClasses/System/Input/CgsInputTypes.h"
IO_H = "src/GameShared/GameClasses/System/Input/CgsInputModuleIO.h"
IO_CPP = "src/GameShared/GameClasses/System/Input/CgsInputModuleIO.cpp"
QUEUES_CPP = "src/GameShared/GameClasses/System/Input/CgsInputProcessRumbleQueues.cpp"
MODULE_H = "src/GameShared/GameClasses/Input/CgsInputModule.h"
MODULE_CPP = "src/GameShared/GameClasses/Input/CgsInputModule.cpp"
PADSPC_H = "src/GameShared/GameClasses/System/Input/PC/CgsInputPadsPC.h"
PADSPC_CPP = "src/GameShared/GameClasses/System/Input/PC/CgsInputPadsPC.cpp"
IOBUFFER_CPP = REPO / "src/GameShared/GameClasses/Module/CgsIOBuffer.cpp"

EVENTS_CHECKS = 35
CHAIN_CHECKS = 36

EVENT_BODIES = [
    "    void InputPads::PlayJoltEvent(",
    "    void InputPads::PlayRumbleEvent(",
    "    void InputPads::ChangeVolumeRumbleEvent(",
    "    void InputPads::StopRumbleEvent(",
    "    f32 InputPads::UpdateJoltEnvelope(",
    "    void InputPads::UpdatePadRumble(",
]
# Present in one revision or the other (the helpers' two homes).
OPTIONAL_PAD_BODIES = [
    "    static f32 GetTotalJoltEnvelopeDuration(",
    "    f32 InputPads::GetTotalJoltEnvelopeDuration(",
    "    f32 InputPads::GetTotalJoltDuration(",
]
MODULE_BODIES = [
    "void InputModule::PreWorldUpdate(",
    "void InputModule::ProcessRumbleRequests(",
]
CHAIN_PAD_BODIES = ["    EBindResult InputPads::BindPlayerToPort(", "    void InputPads::UpdateRumble("]
CHAIN_DEVICE_BODIES = ["    void DeviceX360Pad::SetRumble(", "    bool DeviceX360Pad::IsConnected() const"]
CHAIN_IO_BODIES = ["void PreWorldInputBuffer::Construct()", "void PreWorldInputBuffer::PostPlayJoltEffectByPlayer(",
                   "OutputBuffer::BindResultQueue* OutputBuffer::GetBindResultQueue()\n{"]


def normalised(tree, relative):
    return tree.read(relative).replace("\r\n", "\n")


def shadows(tree):
    return {path: tree.read(path) for path in (PADS_H, DEVICE_H, TYPES_H, IO_H, MODULE_H, PADSPC_H)}


def wiring(tree):
    module = normalised(tree, MODULE_CPP)
    update = body_or_empty(module, "void InputModule::PreWorldUpdate(")
    marks = [update.find(s) for s in ("lpOutputBuffer->LockForWrite()", "lpPreWorldInputBuffer->LockForRead()",
                                      "ProcessRumbleRequests(lpPreWorldInputBuffer, lbUpdatePads)",
                                      "lpPreWorldInputBuffer->UnlockForRead()", "if (lbUpdatePads)",
                                      "GetBindResultQueue()->Append(mOutputBindResultQueue)",
                                      "GetUnbindResultQueue()->Append(mOutputUnbindResultQueue)",
                                      "mOutputBindResultQueue.Clear()", "lpOutputBuffer->UnlockForWrite()")]
    yield ("InputModule::PreWorldUpdate (0x82903328): W(output), R(pre-world), ProcessRumbleRequests, unlock R, "
           "[lbUpdatePads] pads, Append bind / unbind results, clear, unlock W",
           all(m >= 0 for m in marks) and marks == sorted(marks))
    yield ("...its pad-update seat is the PC device scan InputPadsPC::UpdatePadDevices(&mControllers)",
           re.search(r"if\s*\(\s*lbUpdatePads\s*\)\s*\{[^}]*InputPadsPC::UpdatePadDevices\(\s*&mControllers\s*\)", update) is not None)

    process = body_or_empty(module, "void InputModule::ProcessRumbleRequests(")
    order = [process.find(s) for s in ("mControllers.PlayJoltEvent(", "mControllers.StopRumbleEvent(",
                                       "mControllers.PlayRumbleEvent(", "mControllers.ChangeVolumeRumbleEvent(",
                                       "mControllers.UpdateRumble(")]
    yield ("ProcessRumbleRequests (0x828FFE50) drains jolt, stop, play, volume, then UpdateRumble",
           all(m >= 0 for m in order) and order == sorted(order))
    yield ("...the step is the GAME timer's (GetTimerStatusInt()->GetGameTimerStatus()->GetCurrentTimeStep())",
           "GetTimerStatusInt()->GetGameTimerStatus()->GetCurrentTimeStep()" in process)
    yield ("...pause = GetRumblePaused() || !lbUpdatePads, then the enable and force-feedback flags",
           re.search(r"GetRumblePaused\(\)\s*\|\|\s*!\s*lbUpdatePads\s*,\s*lpPreWorldInputBuffer->GetRumbleEnabled\(\)\s*,"
                     r"\s*lpPreWorldInputBuffer->GetWheelForceFeedbackEnabled\(\)", process) is not None)

    construct = body_or_empty(module, "void InputModule::Construct()")
    steps = [construct.find(s) for s in ("ModuleSingleBuffered::Construct()", "mbIsNewModule = true",
                                         "mControllers.Construct()", "mOutputBindResultQueue.Construct()",
                                         "mOutputUnbindResultQueue.Construct()", "E_INPUTPREPARESTAGE_START",
                                         "E_INPUTRELEASESTAGE_DONE", "mpAllocator")]
    yield ("InputModule::Construct (0x828F83D0): base, new-module byte, pads, the two result queues, stages, allocator",
           all(m >= 0 for m in steps) and steps == sorted(steps))

    pads = normalised(tree, PADS_CPP)
    upr = body_or_empty(pads, "    void InputPads::UpdatePadRumble(")
    yield ("UpdatePadRumble gates on DeviceX360Pad::IsConnected() (0x828EFEB8), not an invented force-rumble global",
           "gbForceRumbleOnDisconnectedPad" not in code_only(pads) and "if (!maPads[liPort].IsConnected())" in upr)

    device = normalised(tree, DEVICE_CPP)
    connected = body_or_empty(device, "    bool DeviceX360Pad::IsConnected() const")
    yield ("DeviceX360Pad::IsConnected (0x828DC7E0) = the automated-testing byte || mbConnected",
           "HardwareInit::HasDetectedAutomaticTestingFile()" in connected and "mbConnected" in connected)

    pc = normalised(tree, PADSPC_CPP)
    set_state = body_or_empty(pc, 'extern "C" u32 XInputSetState(u32 dwUserIndex, void* pVibration)')
    yield ("PC motor leaf: XInputSetState forwards to the system XInput DLL (same API, same XINPUT_VIBRATION)",
           "ResolveXInputSetState()" in set_state and "lpfSetState(dwUserIndex, pVibration)" in set_state)
    scan = body_or_empty(pc, "    void InputPadsPC::UpdatePadDevices(InputPads* lpPads)")
    yield ("PC pad scan: binds port 0's DeviceX360Pad when XInput user 0 is present, and player 0 to port 0",
           "BindToPort(KU_PC_PAD_PORT, Device::E_PAD_DEVICE_TYPE)" in scan and "BindPlayerToPort(0," in scan)


def events_numeric(tree):
    pads = normalised(tree, PADS_CPP)
    missing = [s.strip() for s in EVENT_BODIES if s not in pads]
    if missing:
        print("NUMERIC (events): cannot build -- production bodies absent: " + "; ".join(missing))
        return None
    includes = re.findall(r"^#include [^\n]+", pads, re.M)
    parts = includes + ["namespace CgsInput {"]
    force = re.search(r"^\s*bool gbForceRumbleOnDisconnectedPad[^;]*;", pads, re.M)
    if force:
        parts.append(force.group(0))
    parts += [definition(pads, s) for s in OPTIONAL_PAD_BODIES if s in pads]
    parts += [definition(pads, s) for s in EVENT_BODIES]
    device = normalised(tree, DEVICE_CPP)
    if "    bool DeviceX360Pad::IsConnected() const" in device:
        parts += ['}', '#include "GameShared/GameClasses/System/CgsHardwareInit.h"', "namespace CgsInput {",
                  definition(device, "    bool DeviceX360Pad::IsConnected() const")]
    parts.append("}")
    return compile_and_run(Path(__file__).with_name("FxRumble3PadEvents.cpp"), "fxrumble3_pad_events.inc",
                           "\n".join(parts), "FxRumble3PadEvents", shadow=shadows(tree), extra_sources=(STRSTREAM_CPP,))


def chain_numeric(tree):
    module = normalised(tree, MODULE_CPP)
    pads = normalised(tree, PADS_CPP)
    device = normalised(tree, DEVICE_CPP)
    io = normalised(tree, IO_CPP)
    missing = [s.strip() for s in MODULE_BODIES if s not in module]
    missing += [s.strip() for s in CHAIN_PAD_BODIES if s not in pads]
    missing += [s.strip() for s in CHAIN_DEVICE_BODIES if s not in device]
    missing += [s.strip() for s in CHAIN_IO_BODIES if s not in io]
    if missing:
        print("NUMERIC (chain): cannot build -- production bodies absent: " + "; ".join(m.replace("\n", " ") for m in missing))
        return None
    text = "\n".join(["namespace CgsInput {"] + [definition(module, s) for s in MODULE_BODIES] + ["}"])
    # The revision's whole pad / device / IO translation units compile beside the fixture (written next to
    # the .inc; cl runs in that directory, so the relative names resolve there).
    units = {"fx3_CgsInputPads.cpp": tree.read(PADS_CPP), "fx3_CgsInputDeviceX360Pad.cpp": tree.read(DEVICE_CPP),
             "fx3_CgsInputModuleIO.cpp": tree.read(IO_CPP), "fx3_CgsInputProcessRumbleQueues.cpp": tree.read(QUEUES_CPP)}
    return compile_and_run(Path(__file__).with_name("FxRumble3PadChain.cpp"), "fxrumble3_pad_chain.inc", text,
                           "FxRumble3PadChain", shadow=shadows(tree), extra_files=units,
                           extra_sources=tuple(units) + (IOBUFFER_CPP, STRSTREAM_CPP))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    args = parser.parse_args()
    tree = Tree(args.rev)
    events = events_numeric(tree) or (EVENTS_CHECKS, EVENTS_CHECKS)
    chain = chain_numeric(tree) or (CHAIN_CHECKS, CHAIN_CHECKS)
    print(f"numeric: events {events[0] - events[1]}/{events[0]}, chain {chain[0] - chain[1]}/{chain[0]}")
    numeric = (events[0] + chain[0], events[1] + chain[1])
    return report("run_fxrumble3_pad_rumble", list(wiring(tree)), numeric, EVENTS_CHECKS + CHAIN_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
