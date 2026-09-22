"""Check detached-part velocity caps and real simulation queue publication.
Optional first argument is a pre-fix BrnPhysicalBodyPart.cpp snapshot.
"""
from pathlib import Path
import subprocess
import sys
import tempfile
sys.dont_write_bytecode = True
from run_showtime_impulse import definition, settings, REPO, WORKFLOW


def main():
    path = Path(sys.argv[1]) if len(sys.argv) > 1 else REPO / "src/GameSource/Physics/DeformationManager/DeformationPhysics/BrnPhysicalBodyPart.cpp"
    source = path.read_text(encoding="utf-8-sig")
    import re
    constants = "\n".join(re.findall(r"        const f32 KF_(?:MAX_(?:LIN|ANG)_VEL_AT_(?:60|INF)|PROPORTION_OF_MAX_TO_CAP_TO)[^;]+;", source))
    methods = "namespace BrnPhysics { namespace Deformation {\n" + constants + "\n"
    methods += definition(source, "    bool PhysicalBodyPart::LimitVelocities(")
    methods += definition(source, "    void PhysicalBodyPart::UpdateRW(") + "\n}}"
    io = (REPO / "src/GameShared/GameClasses/Physics/CgsPhysicsSimulationModuleIO_InputBuffer.cpp").read_text(encoding="utf-8-sig")
    accessor = definition(io, "    InputBuffer::InUpdateExternalBodyQueue* InputBuffer::GetUpdateExternalBodyQueue()")
    methods += "\nnamespace CgsPhysics { namespace PhysicsSimulationIO {\n" + accessor + "\n}}"
    with tempfile.TemporaryDirectory(prefix="brn_part_velocity_") as directory:
        output = Path(directory)
        (output / "part_velocity_methods.inc").write_text(methods, encoding="utf-8")
        includes = " ".join(f'/I"{WORKFLOW / path}"' for path in settings("msvc_includes.txt"))
        command = ("cl " + " ".join(settings("msvc_flags.txt")) + " " + includes
                   + f' /I"{output}" "{Path(__file__).with_name("PartVelocity.cpp")}"'
                   + ' /Fe:regression.exe /link /OPT:REF')
        script = output / "run.cmd"
        script.write_text('@echo off\ncall "' + str(WORKFLOW / "tools/build/msvc_env.bat")
                          + '" >nul 2>&1\nif errorlevel 1 exit /b 1\n' + command
                          + '\nif errorlevel 1 exit /b 1\nregression.exe\nexit /b %ERRORLEVEL%\n',
                          encoding="utf-8", newline="\r\n")
        subprocess.run(["cmd", "/c", str(script)], cwd=output, check=True)


if __name__ == "__main__":
    main()
