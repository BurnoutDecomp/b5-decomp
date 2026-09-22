"""FX-RCEM (crash-parity 2026-09-22): replay the production takedown-flow arms of
RaceCarEntityModule::HandleGameActions (ARTIST 0x8230BE08 cases 3 / 111 / 120 / 121) against
fixture race cars (tests/RcemTakedownActions.cpp).

Run from the workflow checkout:
    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_rcem_takedown_actions.py [snapshot.cpp]
The optional argument is a snapshot of BrnRaceCarEntityModule.cpp (e.g. the pre-fix
`git show HEAD~1:...`). An arm the snapshot does not have is replayed as the console's
`default: break;`, so the old source reports per-check failures instead of failing to extract.
"""
from pathlib import Path
import re
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
from run_showtime_impulse import definition, settings, REPO, WORKFLOW

SOURCE = REPO / "src/GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModule.cpp"
ARMS = ("E_ACTION_RESET_PLAYER_CAR_ON_TRACK", "E_ACTION_PLAYER_INVULNERABLE",
        "E_ACTION_SHUTDOWN", "E_ACTION_SHUTDOWN_FINISHED")


def code_mask(text):
    """Blank comments and literals (same length), so token scans ignore them."""
    def blank(match):
        return re.sub(r"[^\n]", " ", match.group(0))
    return re.sub(r'//[^\n]*|/\*[\s\S]*?\*/|"(?:\\.|[^"\\\n])*"|\'(?:\\.|[^\'\\\n])*\'', blank, text)


def switch_arm(body, enumerator):
    """The verbatim text of `case ...::<enumerator>:` up to the next label of the same switch."""
    masked = code_mask(body)
    label = re.search(r"case\s+BrnGameState::GameStateModuleIO::" + enumerator + r"\s*:", masked)
    if not label:
        return None
    depth = 0
    position = label.end()
    token = re.compile(r"[{}]|\bcase\b|\bdefault\s*:")
    while True:
        match = token.search(masked, position)
        if match is None:
            raise ValueError("unterminated arm " + enumerator)
        text = match.group(0)
        if text == "{":
            depth += 1
        elif text == "}":
            depth -= 1
            if depth < 0:
                return body[label.start():match.start()]
        elif depth == 0:
            return body[label.start():match.start()]
        position = match.end()


def main():
    path = Path(sys.argv[1]) if len(sys.argv) > 1 else SOURCE
    source = path.read_text(encoding="utf-8-sig")
    handler = definition(source, "void RaceCarEntityModule::HandleGameActions(")
    pieces = []
    constant = re.search(r"^\s*const f32 KF_RESET_ON_TRACK_SPEED\s*=[^;]+;", source, re.M)
    if constant:
        pieces.append(constant.group(0).strip())
    if "bool TakedownActionDiagEnabled()" in source:
        pieces.append(definition(source, "bool TakedownActionDiagEnabled()"))
    arms = []
    for enumerator in ARMS:
        arm = switch_arm(handler, enumerator)
        print(("found   " if arm else "MISSING ") + "arm " + enumerator)
        if arm:
            arms.append(arm)
    pieces.append("void RaceCarEntityModule::Dispatch(s32 liType, const CgsModule::Event* lpEvent, "
                  "OutputFixture* lpOutput)\n{\n    (void)lpEvent; (void)lpOutput;\n    switch (liType)\n    {\n"
                  + "\n".join(arms) + "\n    default:\n        break;\n    }\n}\n")
    with tempfile.TemporaryDirectory(prefix="brn_rcem_takedown_actions_") as directory:
        output = Path(directory)
        (output / "rcem_takedown_actions.inc").write_text("\n".join(pieces), encoding="utf-8")
        includes = " ".join(f'/I"{WORKFLOW / entry}"' for entry in settings("msvc_includes.txt"))
        command = ("cl " + " ".join(settings("msvc_flags.txt")) + " " + includes
                   + f' /I"{output}" "{Path(__file__).with_name("RcemTakedownActions.cpp")}"'
                   + f' "{REPO / "src/GameShared/GameClasses/Development/CgsStrStream.cpp"}"'
                   + " /Fe:regression.exe /link /OPT:REF")
        script = output / "run.cmd"
        script.write_text('@echo off\ncall "' + str(WORKFLOW / "tools/build/msvc_env.bat")
                          + '" >nul 2>&1\nif errorlevel 1 exit /b 1\n' + command
                          + '\nif errorlevel 1 exit /b 1\nregression.exe\nexit /b %ERRORLEVEL%\n',
                          encoding="utf-8", newline="\r\n")
        result = subprocess.run(["cmd", "/c", str(script)], cwd=output)
        sys.exit(result.returncode)


if __name__ == "__main__":
    main()
