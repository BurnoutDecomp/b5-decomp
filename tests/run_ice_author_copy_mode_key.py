"""Check ICEAuthor::SetState's key-edge choice against ARTIST (0x8253CEE4: entering copy mode
asks IsEndKeyState(miPrevState)). SetState, ResolveEndKey and IsEndKeyState are extracted
verbatim from production; only the receiver is renamed.
Optional arguments: snapshots of ICEAuthorKeyElementOps.cpp and ICEAuthor.cpp (pre-fix files).
"""
from pathlib import Path
import re
import subprocess
import sys
import tempfile
sys.dont_write_bytecode = True
from run_showtime_impulse import definition, settings, REPO, WORKFLOW

ICE = REPO / "src/SDKs/Packages/ICE"


def main():
    ops_path = Path(sys.argv[1]) if len(sys.argv) > 1 else ICE / "ICEAuthorKeyElementOps.cpp"
    author_path = Path(sys.argv[2]) if len(sys.argv) > 2 else ICE / "ICEAuthor.cpp"
    ops = ops_path.read_text(encoding="utf-8-sig")
    author = author_path.read_text(encoding="utf-8-sig")
    set_state = definition(ops, "void ICEAuthor::SetState(")
    resolve = definition(ops, "static bool ResolveEndKey(")
    is_end = definition(author, "bool ICEAuthor::IsEndKeyState(")
    params = re.match(r"void ICEAuthor::SetState\(([^)]*)\)", set_state).group(1)
    with tempfile.TemporaryDirectory(prefix="brn_ice_copy_mode_key_") as directory:
        output = Path(directory)
        (output / "ice_set_state_decl.inc").write_text(f"void SetState({params});\n", encoding="utf-8")
        (output / "ice_set_state.inc").write_text(
            set_state.replace("ICEAuthor::SetState", "SetStateFixture::SetState"), encoding="utf-8")
        (output / "ice_resolve_end_key.inc").write_text(
            resolve.replace("const ICEAuthor&", "const SetStateFixture&"), encoding="utf-8")
        (output / "ice_is_end_key_state.inc").write_text(
            is_end.replace("ICEAuthor::IsEndKeyState", "SetStateFixture::IsEndKeyState"), encoding="utf-8")
        includes = " ".join(f'/I"{WORKFLOW / path}"' for path in settings("msvc_includes.txt"))
        command = ("cl " + " ".join(settings("msvc_flags.txt")) + " " + includes
                   + f' /I"{output}" "{Path(__file__).with_name("IceAuthorCopyModeKey.cpp")}"'
                   + ' /Fe:regression.exe /link /OPT:REF')
        script = output / "run.cmd"
        script.write_text('@echo off\ncall "' + str(WORKFLOW / "tools/build/msvc_env.bat")
                          + '" >nul 2>&1\nif errorlevel 1 exit /b 1\n' + command
                          + '\nif errorlevel 1 exit /b 1\nregression.exe\nexit /b %ERRORLEVEL%\n',
                          encoding="utf-8", newline="\r\n")
        sys.exit(subprocess.run(["cmd", "/c", str(script)], cwd=output).returncode)


if __name__ == "__main__":
    main()
