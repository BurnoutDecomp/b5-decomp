"""Run the production Showtime impulse routine against ARTIST-derived numeric cases."""
from pathlib import Path
import re
import subprocess
import tempfile

REPO = Path(__file__).resolve().parents[1]
WORKFLOW = REPO.parent


def definition(source, signature):
    start = source.index(signature)
    depth = 0
    # Skip comments and literals when balancing braces; retain the original source verbatim.
    tokens = re.finditer(r'//[^\n]*|/\*[\s\S]*?\*/|"(?:\\.|[^"\\])*"|[{}]', source[start:])
    for token in tokens:
        if token[0] == "{":
            depth += 1
        elif token[0] == "}":
            depth -= 1
            if depth == 0:
                return source[start:start + token.end()]
    raise ValueError(f"Unclosed function: {signature}")


def settings(name):
    return [line for line in (WORKFLOW / "tools/build" / name).read_text().splitlines()
            if line and not line.startswith("#")]


def main():
    vehicle = (REPO / "src/GameSource/Physics/VehicleManager/VehiclePhysics/VehiclePhysics.cpp").read_text(encoding="utf-8")
    body = (REPO / "src/GameSource/Physics/PhysicsUtilities/ExternalPhysicsBody.cpp").read_text(encoding="utf-8")
    methods = ['#include "GameSource/Physics/VehicleManager/VehiclePhysics/VehiclePhysics.h"',
               '#include "rw/math/vpu/vector3_operation.h"',
               'namespace BrnPhysics { namespace vpu = rw::math::vpu;',
               definition(body, "        inline Vector3 RotateToWorld("),
               definition(body, "    void ExternalPhysicsBody::GetImpulsesFromLocalImpulse("),
               'namespace Vehicle {']
    constant = re.search(r'^    VecFloat KF_SHOWTIME_PLAYER_ANGULAR_IMPULSE_SCALE[^;]+;', vehicle, re.M)
    if constant:
        methods.append(constant[0])
    methods += [definition(vehicle, "    void VehiclePhysics::ApplyShowtimeContactImpulse("), '} }']
    with tempfile.TemporaryDirectory(prefix="brn_showtime_impulse_") as directory:
        output = Path(directory)
        extracted = output / "methods.cpp"
        extracted.write_text("\n".join(methods), encoding="utf-8")
        sources = [Path(__file__).with_name("ShowtimeImpulse.cpp"), extracted]
        includes = " ".join(f'/I"{WORKFLOW / path}"' for path in settings("msvc_includes.txt"))
        command = ("cl " + " ".join(settings("msvc_flags.txt"))
                   + " /D_ALLOW_KEYWORD_MACROS=1 /Dprivate=public /Dprotected=public " + includes + " "
                   + " ".join(f'"{path}"' for path in sources)
                   + ' /Fe:regression.exe /link /OPT:REF')
        script = output / "run.cmd"
        script.write_text('@echo off\ncall "' + str(WORKFLOW / "tools/build/msvc_env.bat")
                          + '" >nul 2>&1\nif errorlevel 1 exit /b 1\n' + command
                          + '\nif errorlevel 1 exit /b 1\nregression.exe\nexit /b %ERRORLEVEL%\n',
                          encoding="utf-8", newline="\r\n")
        subprocess.run(["cmd", "/c", str(script)], cwd=output, check=True)


if __name__ == "__main__":
    main()
