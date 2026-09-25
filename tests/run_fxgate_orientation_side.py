"""FX-GATE: BrnSound::Logic::Collision MapPositionToOrientationUsingBox (X360 0x8269ED18) -- the side
distance is `vminfp v10, v7(left), v8(right)` at 0x8269EF10, which returns a NaN when either distance is
NaN, and the Roof / Bottom tests after it are all-lanes vcmpgtfp. (0x8269EF18, 0x8269EF38) that a NaN
fails. KV_DIRECTION_BIAS and the function body are EXTRACTED from
src/GameSource/Sound/Collision/BrnCollisionStateManager.cpp and compiled into FxGateOrientationSide.cpp.

usage: run_fxgate_orientation_side.py [--rev <b5-decomp git revision>]
    default   the working tree's BrnCollisionStateManager.cpp
    --rev R   the body as of revision R (e.g. the fix commit's parent, to show it fails)
"""
from pathlib import Path
import argparse
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
from run_showtime_impulse import settings, REPO, WORKFLOW

SOURCE = "src/GameSource/Sound/Collision/BrnCollisionStateManager.cpp"
BIAS = "Vector3 KV_DIRECTION_BIAS = "
FUNCTION = "AttribSys::Enums::eOrientation::eOrientation MapPositionToOrientationUsingBox("


def read(rev):
    if rev:
        return subprocess.run(["git", "-C", str(REPO), "show", f"{rev}:{SOURCE}"], check=True,
                              capture_output=True, text=True, encoding="utf-8").stdout
    return (REPO / SOURCE).read_text(encoding="utf-8-sig")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="b5-decomp revision to take BrnCollisionStateManager.cpp from")
    args = parser.parse_args()
    text = read(args.rev).replace("\r\n", "\n")
    bias_start = text.index(BIAS)
    bias = text[bias_start:text.index("\n", bias_start) + 1]
    start = text.index(FUNCTION)
    end = text.index("\n}\n", start) + len("\n}\n")
    # Any file-local helper the body calls sits between the bias constant and the function; take the
    # whole span so the body compiles as it does in the tree.
    helpers = text[text.index("\n", bias_start) + 1:start]
    with tempfile.TemporaryDirectory(prefix="brn_fxgate_orient_") as directory:
        output = Path(directory)
        (output / "extracted.inc").write_text(bias + helpers + text[start:end], encoding="utf-8")
        includes = [WORKFLOW / path for path in settings("msvc_includes.txt")]
        command = ("cl " + " ".join(settings("msvc_flags.txt")) + " "
                   + " ".join(f'/I"{path}"' for path in includes) + f' /I"{output}" '
                   + f'"{Path(__file__).with_name("FxGateOrientationSide.cpp")}"'
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
