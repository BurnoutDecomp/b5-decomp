"""Check the PC hardware-init helpers: accessibility/screensaver save, apply and restore, and
the device-notification registration's recipient, handle and result. The production bodies are
compiled against fakes (HardwareInitHelpersRegression.cpp); the wiring checks read the callers.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_hardware_init_helpers.py [--rev <rev>]
"""
import argparse
import os
import re
from pathlib import Path
from fxgs_common import Tree, definition, body_or_empty, compile_and_run, report

os.environ.pop("NoDefaultCurrentDirectoryInExePath", None)
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--rev", help="read the production source from a b5 revision")
args = parser.parse_args()

SOURCE = "src/GameShared/GameClasses/System/PC/CgsHardwareInitPC.cpp"
source = Tree(args.rev).read(SOURCE)

create_window = body_or_empty(source, "static HWND CreateGameWindow(")
initialize = body_or_empty(source, "void CgsSystem::HardwareInit::InitializeHardware(")
release = body_or_empty(source, "void CgsSystem::HardwareInit::ReleaseHardware()")
watchdog = body_or_empty(source, "static DWORD WINAPI ShutdownWatchdogProc(")
window_proc = body_or_empty(source, "static LRESULT CALLBACK windowProc(")


def ordered(text, first, second):
    a, b = text.find(first), text.find(second)
    return a >= 0 and b > a


wiring = [
    ("CreateGameWindow registers the new window and fails without the registration",
     re.search(r"return\s+RegisterDeviceNotif\(\s*window\s*,\s*&notify\s*\)\s*\?\s*window\s*:\s*nullptr\s*;",
               create_window) is not None),
    ("InitializeHardware applies the in-game settings after creating the window",
     ordered(initialize, "CreateGameWindow(", "SetSystemParameters(false)")),
    ("ReleaseHardware restores the saved settings",
     "SetSystemParameters(true)" in release),
    ("the shutdown watchdog restores the saved settings before TerminateProcess",
     ordered(watchdog, "SetSystemParameters(true)", "TerminateProcess(")),
    ("windowProc swallows a lone Alt or F10 system key",
     re.search(r"uMsg\s*==\s*WM_SYSKEYDOWN\s*&&\s*\(\s*wParam\s*==\s*VK_MENU\s*\|\|\s*wParam\s*==\s*VK_F10\s*\)\s*\)\s*return\s+0\s*;",
               window_proc) is not None),
]

bodies = []
for signature in ("static void SetSystemParameters(bool reset)", "static bool RegisterDeviceNotif("):
    try:
        bodies.append(definition(source, signature))
    except ValueError:
        print("MISSING body: " + signature)
NUMERIC_TOTAL = 19
numeric = None
if len(bodies) == 2:
    numeric = compile_and_run(Path(__file__).with_name("HardwareInitHelpersRegression.cpp"),
                              "hardware_init_helpers.inc", "\n\n".join(bodies) + "\n",
                              "HardwareInitHelpers")
raise SystemExit(report("run_hardware_init_helpers", wiring, numeric, NUMERIC_TOTAL))
