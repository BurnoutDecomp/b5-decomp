"""FX-RCEM (crash-parity 2026-09-22): the free-burn rival SHUTDOWN arms of
BrnGameModule::TranslateGameActionsToGuiEvents (ARTIST 0x823E9CE0), replayed from the production
source against a recording GUI queue (tests/RcemShutdownGuiEvents.cpp).

  121 -> GUI 374 (size 1)  must be present.
  120 -> GUI 373 (size 8)  is a LANDING-ORDER GUARD: InGame's case 373 sends "TO_RVL_POST" into
                           BrnGui::OfflineRivalShutdown, so the arm may exist only once that
                           state's Update has a body (tools/re/hasbody.py); then it is checked
                           numerically too.

Run from the workflow checkout:
    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_rcem_shutdown_gui_events.py [snapshot.cpp]
"""
from pathlib import Path
import re
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
from run_showtime_impulse import definition, settings, REPO, WORKFLOW
from run_rcem_takedown_actions import switch_arm

SOURCE = REPO / "src/GameSource/Game/GameBridgeGameStateToX_StuntGuiEvents.cpp"
PUSH_HEADER = REPO / "src/GameSource/Game/GameBridgeGameStateToX.h"


def struct_definition(source, name):
    start = source.index("struct " + name)
    end = source.index("};", start) + 2
    text = source[start:end]
    asserts = re.findall(r"static_assert\(sizeof\(" + name + r"\)[^;]*;", source)
    return text + "\n" + "\n".join(asserts)


def has_body(qualified):
    result = subprocess.run([sys.executable, str(WORKFLOW / "tools/re/hasbody.py"), qualified],
                            capture_output=True, text=True, cwd=WORKFLOW)
    return "HAS BODY" in result.stdout


def main():
    path = Path(sys.argv[1]) if len(sys.argv) > 1 else SOURCE
    source = path.read_text(encoding="utf-8-sig")
    translator = definition(source, "void BrnGameModule::TranslateGameActionsToGuiEvents(")
    arms = {}
    for enumerator in ("E_ACTION_SHUTDOWN_FINISHED", "E_ACTION_SHUTDOWN"):
        arms[enumerator] = switch_arm(translator, enumerator)
        print(("found   " if arms[enumerator] else "MISSING ") + "translator arm " + enumerator)

    structural = []
    if arms["E_ACTION_SHUTDOWN"] and not has_body("BrnGui::OfflineRivalShutdown::Update"):
        structural.append("the 120 -> 373 arm is landed while BrnGui::OfflineRivalShutdown::Update has no "
                          "body: InGame's case 373 would strand the screen flow in POST_RIVAL")
    for failure in structural:
        print("FAIL (landing order):", failure)
    print(f"landing order: 1 check, {len(structural)} failures")

    present = [arm for arm in arms.values() if arm]
    wires = sorted(set(re.findall(r"\b(\w+Wire\d+)\b", "\n".join(present))))
    push = definition(PUSH_HEADER.read_text(encoding="utf-8-sig"), "    template <class GuiEventT>")
    push = push.replace("CgsGui::CgsGuiModuleIO::InputBuffer*", "GuiInput*")
    pieces = [struct_definition(source, wire) for wire in wires] + [push]
    pieces.append("void Translate(s32 liActionType, const CgsModule::Event* lpAction, GuiInput* lpGuiInput)\n{\n"
                  "    (void)lpAction; (void)lpGuiInput;\n    switch (liActionType)\n    {\n"
                  + "\n".join(present) + "\n    default:\n        break;\n    }\n}\n")
    with tempfile.TemporaryDirectory(prefix="brn_rcem_shutdown_gui_") as directory:
        output = Path(directory)
        (output / "rcem_shutdown_gui_events.inc").write_text("\n".join(pieces), encoding="utf-8")
        includes = " ".join(f'/I"{WORKFLOW / entry}"' for entry in settings("msvc_includes.txt"))
        define = " /DRCEM_HAS_120_ARM=1" if arms["E_ACTION_SHUTDOWN"] else ""
        command = ("cl " + " ".join(settings("msvc_flags.txt")) + define + " " + includes
                   + f' /I"{output}" "{Path(__file__).with_name("RcemShutdownGuiEvents.cpp")}"'
                   + f' "{REPO / "src/GameShared/GameClasses/Development/CgsStrStream.cpp"}"'
                   + " /Fe:regression.exe /link /OPT:REF")
        script = output / "run.cmd"
        script.write_text('@echo off\ncall "' + str(WORKFLOW / "tools/build/msvc_env.bat")
                          + '" >nul 2>&1\nif errorlevel 1 exit /b 1\n' + command
                          + '\nif errorlevel 1 exit /b 1\nregression.exe\nexit /b %ERRORLEVEL%\n',
                          encoding="utf-8", newline="\r\n")
        result = subprocess.run(["cmd", "/c", str(script)], cwd=output)
    sys.exit(1 if (structural or result.returncode) else 0)


if __name__ == "__main__":
    main()
