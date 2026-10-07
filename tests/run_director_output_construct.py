"""Run real OutputBuffer construction on poisoned/reused typed storage (ARTIST 0x8225C960).

--omit-camera is a one-call negative control. --pre-fix REV substitutes only that
revision's constructor into the current production TU and header, so the regression
can detect the original omissions without relying on obsolete raw member storage.
"""
from pathlib import Path
import argparse
import os
import sys

sys.dont_write_bytecode = True
from fxgs_common import REPO, STRSTREAM_CPP, Tree, compile_and_run, definition

os.environ.pop("NoDefaultCurrentDirectoryInExePath", None)

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--omit-camera", action="store_true")
parser.add_argument("--pre-fix", metavar="REV")
args = parser.parse_args()

relative = "src/GameSource/Director/DirectorModule/BrnDirectorModuleIOOutputBuffer.cpp"
source = Tree().read(relative)
signature = "void OutputBuffer::Construct()"
constructor = definition(source, signature)
if args.pre_fix:
    source = source.replace(constructor, definition(Tree(args.pre_fix).read(relative), signature), 1)
elif args.omit_camera:
    if constructor.count("mCameraOutput.Construct();") != 1:
        raise SystemExit("negative control requires exactly one production camera Construct")
    source = source.replace(constructor, constructor.replace("mCameraOutput.Construct();", ""), 1)

camera = REPO / "src/GameSource/Director/Camera"
# Keep real constructor callees, but leave unrelated unmounted renderer/debug methods
# outside this CPU fixture. The complete OutputBuffer TU is also checked by compile_gate.
output_bodies = "\n".join(definition(source, signature) for signature in (
    "void OutputBuffer::Construct()", "u8* OutputBuffer::GetDirectorOu()",
    "u8* OutputBuffer::GetReplayRe()", "u8* OutputBuffer::GetDirectorOutputIn()",
    "u8* OutputBuffer::GetReplayRequestI()",
    "const BrnDirector::Camera::Camera* OutputBuffer::GetCameraOutput() const",
))
camera_source = (camera / "Camera.cpp").read_text(encoding="utf-8-sig")
camera_bodies = "\n".join(definition(camera_source, signature) for signature in (
    "void Camera::Construct()", "void Camera::Clear()",
))
validity_source = (camera / "BrnCameraValidityAccount.cpp").read_text(encoding="utf-8-sig")
validity_setup = definition(validity_source, "void ValidityAccount::SetupFailFlagMask()")
validity_prefix = validity_source[:validity_source.index(validity_setup) + len(validity_setup)]
compiled_bodies = ("namespace BrnDirector { namespace DirectorIO {\n" + output_bodies
                   + "\n} namespace Camera {\n" + camera_bodies + "\n} }\n"
                   + validity_prefix + "\n} }\n")
numeric = compile_and_run(Path(__file__).with_name("DirectorOutputConstruct.cpp"),
    "director_output_buffer.inc", compiled_bodies, "DirectorOutputConstruct", extra_sources=[
        REPO / "src/GameShared/GameClasses/Module/CgsIOBuffer.cpp",
        camera / "BrnCameraState.cpp",
        camera / "BrnCameraEffects.cpp", camera / "BrnDepthOfField.cpp",
        STRSTREAM_CPP,
    ])
if numeric is None:
    raise SystemExit(1)
checks, failures = numeric
print(f"run_director_output_construct: {checks - failures}/{checks} pass ({failures} fail)")
raise SystemExit(bool(failures))
