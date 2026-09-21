"""Compile real locator bodies and dependencies, with doubles only for unused physics methods."""
from pathlib import Path
import subprocess
import tempfile

REPO = Path(__file__).resolve().parents[1]
WORKFLOW = REPO.parent
DEFORMATION = REPO / "src/GameSource/Physics/DeformationManager/DeformationPhysics"


def definition(source, signature):
    start = source.index(signature)
    return source[start:source.index("\n    }", start) + 6]


def settings(name):
    return [line for line in (WORKFLOW / "tools/build" / name).read_text().splitlines()
            if line and not line.startswith("#")]


def main():
    update = (DEFORMATION / "BrnDeformableObject_Update.cpp").read_text(encoding="utf-8")
    simple = (REPO / "src/GameSource/Physics/VehicleManager/VehiclePhysics/BrnSimpleVehiclePhysics.cpp").read_text(encoding="utf-8")
    accessor = (DEFORMATION / "BrnDeformableObject_Accessors.cpp").read_text(encoding="utf-8")
    methods = ['#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnDeformableObject.h"',
               '#include "rw/math/vpu/matrix44affine_operation.h"',
               'namespace BrnPhysics { namespace Deformation {',
               definition(update, "    void DeformableObject::UpdateLocators("),
               definition(accessor, "    Vehicle::VehiclePhysics* DeformableObject::GetVehiclePhysics()"),
               '} namespace Vehicle { namespace vpu = rw::math::vpu;',
               definition(simple, "    static Vector3 RotateCOMOffsetToWorld("),
               definition(simple, "    static bool IsTransformValid("),
               definition(simple, "    Matrix44Affine SimpleVehiclePhysics::GetGraphicsVehicleTransform()"),
               '} }']
    with tempfile.TemporaryDirectory(prefix="brn_deformation_locators_") as directory:
        output = Path(directory)
        extracted = output / "methods.cpp"
        extracted.write_text("\n".join(methods), encoding="utf-8")
        sources = [Path(__file__).with_name("DeformationLocators.cpp"), extracted,
                   DEFORMATION / "BrnDeformableObject_IKSkinning.cpp",
                   DEFORMATION / "BrnPhysicalBodyPartPool_Accessors.cpp"]
        includes = " ".join(f'/I"{WORKFLOW / path}"' for path in settings("msvc_includes.txt"))
        # MSVC encodes member access in symbols; expose private members consistently
        # in every fixture translation unit, including the unmodified production bodies.
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
