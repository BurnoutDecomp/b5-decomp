"""FX-GATE: CgsInput::InputPads::UpdateJoltEnvelope (X360 0x828E7618) against the console's arms.

The body is EXTRACTED from src/GameShared/GameClasses/System/Input/CgsInputPads.cpp and compiled with
the canonical build flags into FxGateJoltEnvelope.cpp, which models the console instructions: the
`t >= 0` assert is `fcmpu ; bge -> past` (0x828E7640/0x828E7644, a NaN time does not fire it), the
final test is `fcmpu t, end ; bge -> 0.0` (0x828E76D4/0x828E76D8, a NaN time returns 0.0), and both
lerps are fmadds (0x828E76A0, 0x828E76EC: one rounding).

usage: run_fxgate_jolt_envelope.py [--rev <b5-decomp git revision>]
    default   the working tree's CgsInputPads.cpp
    --rev R   the body as of revision R (e.g. the fix commit's parent, to show it fails)
"""
from pathlib import Path
import argparse
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
from run_showtime_impulse import settings, REPO, WORKFLOW

SOURCE = "src/GameShared/GameClasses/System/Input/CgsInputPads.cpp"
SIGNATURE = "f32 InputPads::UpdateJoltEnvelope(const InputIO::JoltEnvelope& lEnvelope, f32 lfTime)"


def read(rev):
    if rev:
        return subprocess.run(["git", "-C", str(REPO), "show", f"{rev}:{SOURCE}"], check=True,
                              capture_output=True, text=True, encoding="utf-8").stdout
    return (REPO / SOURCE).read_text(encoding="utf-8-sig")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="b5-decomp revision to take CgsInputPads.cpp from")
    args = parser.parse_args()
    text = read(args.rev).replace("\r\n", "\n")
    start = text.index(SIGNATURE)
    end = text.index("\n    }\n", start) + len("\n    }\n")
    body = text[start:end]
    with tempfile.TemporaryDirectory(prefix="brn_fxgate_jolt_") as directory:
        output = Path(directory)
        (output / "extracted.inc").write_text(body, encoding="utf-8")
        command = ("cl " + " ".join(settings("msvc_flags.txt")) + f' /I"{output}" '
                   + f'"{Path(__file__).with_name("FxGateJoltEnvelope.cpp")}"'
                   + " /Fe:regression.exe /link /OPT:REF")
        script = output / "run.cmd"
        script.write_text('@echo off\ncall "' + str(WORKFLOW / "tools/build/msvc_env.bat")
                          + '" >nul 2>&1\nif errorlevel 1 exit /b 1\n' + command
                          + ' >build.log 2>&1\nif errorlevel 1 (type build.log & exit /b 1)\n'
                          + 'regression.exe\nexit /b %ERRORLEVEL%\n',
                          encoding="utf-8", newline="\r\n")
        sys.exit(subprocess.run(["cmd", "/c", str(script)], cwd=output).returncode)


if __name__ == "__main__":
    main()
