"""Compile the restored production math/state methods against their real headers."""
from pathlib import Path
import subprocess
import tempfile
from run_rival_impacts import WORKFLOW, REPO, settings


def definition(source, signature):
    start = source.index(signature)
    opening = source.index("{", start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


def main():
    chunks = []
    groups = [
        ("GameSource/World/AI/ResetOnTrack/BrnResetOnTrackManager.cpp", "BrnAI", [
            "Vector3 ResetOnTrackManager::ComputeNearestPositionInSegment",
            "f32 ResetOnTrackManager::ComputeAISectionWidth",
            "AICar* ResetOnTrackManager::GetAICar", "bool ResetOnTrackManager::ComputeInitialCoordinatesStandard",
            "bool ResetOnTrackManager::UpdateResetOnTrackSectionUsingRoute",
            "void ResetOnTrackManager::UpdateResetOnTrackSectionUsingCurrentSection"]),
        ("GameSource/World/AI/BrnAICar_Update.cpp", "BrnAI", ["Vector3 AICar::GetVelocityDirection", "void AICar::UpdateResetOnTrackSection"]),
        ("GameSource/Director/Camera/Behaviours/BrnBehaviourGyroCam.cpp", "BrnDirector::Camera", [
            "void AttachmentTruck::Set", "Vector3 AttachmentTruck::GetVelocity", "void AttachmentTruck::Update"]),
        ("GameSource/Director/Camera/Utils/BrnPositionLag.cpp", "BrnDirector::Camera::Utils", [
            "void PositionLag::Parameters::Construct"]),
        ("GameSource/Director/Camera/Utils/CameraUtils.cpp", "BrnDirector::Camera::Utils", [
            "Vector3 FindNonParallelNormalisedVectorTo", "Vector3 SphericalBlend", "Vector3 SafeSLerp",
            "bool PointWillLeaveFrustrum", "f32 SineLerp"]),
    ]
    for path, namespace, methods in groups:
        source = (REPO / "src" / path).read_text(encoding="utf-8-sig")
        chunks += [f"namespace {namespace} {{\nnamespace vpu = rw::math::vpu;\n"
                   + "\n".join(definition(source, method) for method in methods) + "\n}"]
    with tempfile.TemporaryDirectory(prefix="brn_recovery_camera_") as directory:
        output = Path(directory)
        (output / "restored_methods.inc").write_text("\n".join(chunks), encoding="utf-8")
        includes = " ".join(f'/I"{WORKFLOW / path}"' for path in settings("msvc_includes.txt"))
        command = ("cl " + " ".join(settings("msvc_flags.txt")) + " " + includes
                   + f' /I"{output}" "{Path(__file__).with_name("RivalRecoveryCamera.cpp")}"'
                   + " /Fe:regression.exe /link /OPT:REF")
        script = output / "run.cmd"
        script.write_text('@echo off\ncall "' + str(WORKFLOW / "tools/build/msvc_env.bat")
                          + '" >nul 2>&1\nif errorlevel 1 exit /b 1\n' + command
                          + '\nif errorlevel 1 exit /b 1\nregression.exe\nexit /b %ERRORLEVEL%\n',
                          encoding="utf-8", newline="\r\n")
        subprocess.run(["cmd", "/c", str(script)], cwd=output, check=True)


if __name__ == "__main__":
    main()
