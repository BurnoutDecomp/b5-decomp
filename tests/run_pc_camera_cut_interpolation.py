"""Compile actual latch/render methods; --rev aaa5ccc8 reproduces cross-shot blending."""
import argparse
import os
from pathlib import Path
from fxgs_common import REPO, Tree, definition, compile_and_run, report

os.environ.pop('NoDefaultCurrentDirectoryInExePath', None)
parser = argparse.ArgumentParser()
parser.add_argument('--rev')
args = parser.parse_args()
source = Tree(args.rev).read('src/GameSource/Game/BrnGameModule.cpp')
methods = '\n'.join(definition(source, signature) for signature in (
    'void BrnGameModule::LatchDispatchCamera(',
    'const BrnDirector::Camera::Camera* BrnGameModule::GetInterpolatedDispatchCamera('))
methods = methods.replace('BrnGameModule::', 'TestModule::')
camera_source = Tree().read('src/GameSource/Director/Camera/Camera.cpp')
camera_members = '\n'.join(definition(camera_source, signature) for signature in (
    'void Camera::SetFOV(', 'f32 Camera::GetFOV(',
    'const rw::math::vpu::Matrix44Affine& Camera::GetTransform(', 'void Camera::SetTransform('))
copy_constructor = 'Camera::Camera(const Camera& lrOther) = default;'
assert camera_source.count(copy_constructor) == 1
state_member = definition(Tree().read('src/GameSource/Director/Camera/BrnCameraState.cpp'),
                          'void CameraState::SetFlag(')
methods = ('namespace BrnDirector { namespace Camera {\n' + copy_constructor + '\n'
           + camera_members + '\n' + state_member + '\n} }\n' + methods)
result = compile_and_run(Path(__file__).with_name('PCCameraCutInterpolation.cpp'),
    'pc_camera_cut_interpolation.inc', methods, 'PCCameraCutInterpolation', extra_sources=[
        REPO / 'src/GameShared/GameClasses/System/Timer/CgsFrameInterpolation.cpp'])
raise SystemExit(report('run_pc_camera_cut_interpolation', [], result, 17))
