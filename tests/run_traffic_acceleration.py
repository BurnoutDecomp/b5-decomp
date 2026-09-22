"""Exercise production acceleration math with real pool types and an isolated receiver.

Only the receiver's name changes to avoid constructing world-module services.
Pass an optional source snapshot to reproduce a pre-fix failure.
"""
from pathlib import Path
import re
import subprocess
import sys
import tempfile

REPO = Path(__file__).resolve().parents[1]
WORKFLOW = REPO.parent
TRAFFIC = REPO / "src/GameSource/World/EntityModules/TrafficEntityModule"


def definition(source, signature):
    start = source.index(signature)
    depth = 0
    for token in re.finditer(r'//[^\n]*|/\*[\s\S]*?\*/|"(?:\\.|[^"\\])*"|[{}]', source[start:]):
        if token[0] == "{":
            depth += 1
        elif token[0] == "}":
            depth -= 1
            if depth == 0:
                return source[start:start + token.end()]
    raise ValueError(signature)


def settings(name):
    return [line for line in (WORKFLOW / "tools/build" / name).read_text().splitlines()
            if line and not line.startswith("#")]


def main():
    current = (TRAFFIC / "BrnTrafficEntityModule.cpp").read_text(encoding="utf-8-sig")
    source = Path(sys.argv[1]).read_text(encoding="utf-8-sig") if len(sys.argv) > 1 else current
    chunks = ["namespace BrnTraffic {"]
    for constant in ("KF_PARAM_DEFAULT_MAX_SPEED", "KF_CRASH_SLOW_TARGET_SPEED", "KF_CRASH_SLOW_MAX_ACCEL"):
        chunks.append(re.search(r"const f32 " + constant + r"\s*=[^;]+;", source)[0])
    for signature in ("Vehicle* TrafficEntityModule::GetVehicle(u32", "const Vehicle* TrafficEntityModule::GetVehicle(u32",
                      "const ParamTransform* TrafficEntityModule::GetParamTransform(u32",
                      "Matrix44Affine TrafficEntityModule::GetVehicleTransform(u32"):
        chunks.append(definition(current, signature).replace("TrafficEntityModule", "AccelerationFixture"))
    chunks.append(definition(source, "f32 TrafficEntityModule::UpdateParams_CalcAcceleration(")
                  .replace("TrafficEntityModule::", "AccelerationFixture::"))
    for file, signatures in (
        ("BrnTrafficVehicle.cpp", ("bool Vehicle::IsRecoveringFromSlam()", "bool Vehicle::IsExtremeSwerving()")),
        ("BrnTrafficParam.cpp", ("Vector3 ParamTransform::GetLerpedPos()", "Vector3 ParamTransform::GetDirection()"))):
        text = (TRAFFIC / file).read_text(encoding="utf-8-sig")
        chunks.extend(definition(text, signature) for signature in signatures)
    chunks.append("}")
    with tempfile.TemporaryDirectory(prefix="brn_traffic_acceleration_") as directory:
        output = Path(directory)
        (output / "traffic_acceleration_methods.inc").write_text("\n".join(chunks), encoding="utf-8")
        includes = " ".join(f'/I"{WORKFLOW / path}"' for path in settings("msvc_includes.txt"))
        command = ("cl " + " ".join(settings("msvc_flags.txt")) + " " + includes
                   + f' /I"{output}" "{Path(__file__).with_name("TrafficAcceleration.cpp")}"'
                   + ' /Fe:regression.exe /link /OPT:REF')
        script = output / "run.cmd"
        script.write_text('@echo off\ncall "' + str(WORKFLOW / "tools/build/msvc_env.bat")
                          + '" >nul 2>&1\nif errorlevel 1 exit /b 1\n' + command
                          + '\nif errorlevel 1 exit /b 1\nregression.exe\nexit /b %ERRORLEVEL%\n',
                          encoding="utf-8", newline="\r\n")
        subprocess.run(["cmd", "/c", str(script)], cwd=output, check=True)


if __name__ == "__main__":
    main()
