"""FX-RIGIDBODY (H2-D1): run the shipped rw::physics::Quaternion::UnitQuaternionToMatrix @0x82BC3EC0
and rw::physics::RigidBody::DynamicUpdate @0x82BC2B78 against oracles from the console's own
instruction sequences (FxRigidBodyQuaternion.cpp).

usage: run_fxrigidbody_quaternion.py [--rev <b5-decomp git revision>]
    default   the working tree's Quaternion.cpp / quaternion.h / RigidBody.cpp
    --rev R   those three files as of revision R (e.g. the fix commit's parent, to show the
              pre-fix bodies fail); every other header still comes from the working tree.
"""
from pathlib import Path
import argparse
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
from run_showtime_impulse import settings, REPO, WORKFLOW

FILES = {
    "header": "vendor/renderware/include/rw/physics/quaternion.h",
    "quaternion": "vendor/renderware/src/rw/physics/Quaternion.cpp",
    "rigidbody": "src/vendor/renderware/physics/RigidBody.cpp",
}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="b5-decomp revision to take the three production files from")
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="brn_fxrigidbody_") as directory:
        output = Path(directory)
        includes = []
        if args.rev:
            def show(path):
                return subprocess.run(["git", "-C", str(REPO), "show", f"{args.rev}:{path}"],
                                      check=True, capture_output=True).stdout
            header = output / "inc" / "rw" / "physics" / "quaternion.h"
            header.parent.mkdir(parents=True)
            header.write_bytes(show(FILES["header"]))
            (output / "Quaternion.cpp").write_bytes(show(FILES["quaternion"]))
            (output / "RigidBody.cpp").write_bytes(show(FILES["rigidbody"]))
            includes.append(output / "inc")   # first, so the revision's quaternion.h wins
            sources = [output / "Quaternion.cpp", output / "RigidBody.cpp"]
        else:
            sources = [REPO / FILES["quaternion"], REPO / FILES["rigidbody"]]
        includes += [WORKFLOW / path for path in settings("msvc_includes.txt")]
        includes.append(REPO / "src/vendor/renderware/physics")   # RigidBody.cpp's "JointFrames.hpp"
        command = ("cl " + " ".join(settings("msvc_flags.txt"))
                   + " /D_ALLOW_KEYWORD_MACROS=1 /Dprivate=public /Dprotected=public "
                   + " ".join(f'/I"{path}"' for path in includes) + " "
                   + " ".join(f'"{path}"' for path in [Path(__file__).with_name("FxRigidBodyQuaternion.cpp")] + sources)
                   + " /Fe:regression.exe /link /OPT:REF")
        script = output / "run.cmd"
        script.write_text('@echo off\ncall "' + str(WORKFLOW / "tools/build/msvc_env.bat")
                          + '" >nul 2>&1\nif errorlevel 1 exit /b 1\n' + command
                          + '\nif errorlevel 1 exit /b 1\nregression.exe\nexit /b %ERRORLEVEL%\n',
                          encoding="utf-8", newline="\r\n")
        subprocess.run(["cmd", "/c", str(script)], cwd=output, check=True)


if __name__ == "__main__":
    main()
