"""Check detached-part extra-gravity mass scaling.
Optional first argument is a pre-fix BrnPhysicalBodyPartPool.cpp snapshot.
"""
from pathlib import Path
import subprocess
import sys
import tempfile
sys.dont_write_bytecode = True
from run_showtime_impulse import definition, settings, REPO, WORKFLOW


def main():
    path = Path(sys.argv[1]) if len(sys.argv) > 1 else REPO / "src/GameSource/Physics/DeformationManager/DeformationPhysics/BrnPhysicalBodyPartPool.cpp"
    source = path.read_text(encoding="utf-8-sig")
    methods = "namespace BrnPhysics { namespace Deformation {\nf32 kfPartExtraGravity = 0.0f;\n"
    methods += definition(source, "    void PhysicalBodyPartPool::UpdateRWBodies(") + "\n}}"
    body = (REPO / "src/GameSource/Physics/PhysicsUtilities/ExternalPhysicsBody.cpp").read_text(encoding="utf-8-sig")
    methods += "\nnamespace BrnPhysics {\n" + definition(body, "    void ExternalPhysicsBody::AddLocalSpaceForce(") + "\n}"
    with tempfile.TemporaryDirectory(prefix="brn_part_gravity_") as directory:
        output = Path(directory)
        (output / "part_gravity_methods.inc").write_text(methods, encoding="utf-8")
        includes = " ".join(f'/I"{WORKFLOW / path}"' for path in settings("msvc_includes.txt"))
        command = ("cl " + " ".join(settings("msvc_flags.txt")) + " " + includes
                   + f' /I"{output}" "{Path(__file__).with_name("PartGravity.cpp")}"'
                   + ' /Fe:regression.exe /link /OPT:REF')
        script = output / "run.cmd"
        script.write_text('@echo off\ncall "' + str(WORKFLOW / "tools/build/msvc_env.bat")
                          + '" >nul 2>&1\nif errorlevel 1 exit /b 1\n' + command
                          + '\nif errorlevel 1 exit /b 1\nregression.exe\nexit /b %ERRORLEVEL%\n',
                          encoding="utf-8", newline="\r\n")
        subprocess.run(["cmd", "/c", str(script)], cwd=output, check=True)


if __name__ == "__main__":
    main()
