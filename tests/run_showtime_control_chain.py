"""Compile the complete production aftertouch, bounce and cap bodies against ARTIST gold.

Run from the workflow checkout. Optional source argument supplies pre-fix RaceCarPhysics.cpp.
The committed gold records synthetic inputs interpreted through ARTIST raw instruction words.
Includes actual target assistance, ideal intercept and camera-relative aftertouch.
External force application into body integration remains a separate boundary.
"""
from pathlib import Path
import os
import re
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
from run_render_part_interpolation import REPO, WORKFLOW, settings
from fxdeformlat_common import definition


def main():
    path = Path(sys.argv[1]) if len(sys.argv)>1 else REPO / "src/GameSource/Physics/VehicleManager/VehiclePhysics/RaceCarPhysics.cpp"
    source = path.read_text(encoding="utf-8-sig")
    constants = re.findall(r"^\s*static const (?:f32|Vector3) K[A-Z0-9_]+\s*=[^;]+;", source, re.M)
    methods = constants + [definition(source,("Vector3*" if name=="ComputeIdealVelocity" else "void")+" RaceCarPhysics::"+name+"(").replace("RaceCarPhysics::","ShowtimeFixture::")
                          for name in ("ComputeIdealVelocity","UpdateTargetAssist","UpdateAftertouch","UpdateShowtimePhysics","CapShowtimeVelocities")]
    # Include production helper when the per-component impulse predicate has been recovered.
    if "bool HasAftertouchImpulse(" in source:
        methods.insert(len(constants),definition(source,"bool HasAftertouchImpulse("))
    if "bool ShowtimeControlProbeChanged(" in source:
        methods.insert(len(constants),definition(source,"bool ShowtimeControlProbeChanged("))
    with tempfile.TemporaryDirectory(prefix="brn_showtime_chain_") as directory:
        output = Path(directory)
        (output/"showtime_chain_methods.inc").write_text("\n".join(methods),encoding="utf-8")
        math_source=(REPO/"src/GameSource/Math/BrnMathUtils.cpp").read_text(encoding="utf-8-sig")
        math_methods="\n".join(definition(math_source,"f32 "+name+"(") for name in ("MagnitudeSquared2D","Magnitude2D"))
        (output/"showtime_chain_math.cpp").write_text('#include "GameSource/Math/BrnMathUtils.h"\n#include "rw/math/vpu/vector3_operation.h"\n#include <cmath>\nnamespace BrnMath {\n'+math_methods+'\n}\n',encoding="utf-8")
        sources = [Path(__file__).with_name("ShowtimeControlChain.cpp"),
                   REPO / "src/GameSource/Physics/VehicleManager/SharedIO/BrnPlayerDriverControls.cpp",
                   REPO / "src/GameShared/GameClasses/Development/CgsStrStream.cpp"]
        sources.append(output/"showtime_chain_math.cpp")
        includes = " ".join(f'/I"{WORKFLOW/path}"' for path in settings("msvc_includes.txt"))
        command = ("cl "+" ".join(settings("msvc_flags.txt"))+" "+includes+f' /I"{output}" '
                   +" ".join(f'"{path}"' for path in sources)+" /Fe:regression.exe /link /OPT:REF")
        script=output/"run.cmd"
        script.write_text('@echo off\ncall "'+str(WORKFLOW/"tools/build/msvc_env.bat")
                          +'" >nul 2>&1\nif errorlevel 1 exit /b 1\n'+command
                          +'\nif errorlevel 1 exit /b 1\nregression.exe\nexit /b %ERRORLEVEL%\n',
                          encoding="utf-8",newline="\r\n")
        environment=dict(os.environ)
        environment.pop("NoDefaultCurrentDirectoryInExePath",None)
        subprocess.run(["cmd","/c",str(script)],cwd=output,env=environment,check=True)


if __name__=="__main__":main()
