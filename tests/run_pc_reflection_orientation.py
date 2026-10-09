"""Render and sample a continuous world field through the real cube-face cameras.

--rev selects previous production camera/setup bodies for the regression control.
The scene and D3D9 device are fixture boundaries; cube lookup is performed by the GPU.
"""
import argparse
import os
import re
from pathlib import Path
from fxgs_common import REPO, Tree, definition, compile_and_run, report

os.environ.pop('NoDefaultCurrentDirectoryInExePath', None)
parser = argparse.ArgumentParser()
parser.add_argument('--rev')
args = parser.parse_args()
tree = Tree(args.rev)
camera = tree.read('src/GameShared/GameClasses/Graphics/CgsCamera.cpp')
envmap = tree.read('src/GameSource/World/EnvironmentMap/BrnEnvironmentMap.cpp')
world = tree.read('src/GameSource/World/BrnWorldModule.cpp')
renderer = tree.read('src/GameSource/Graphics/BrnRendererModule.cpp')
shims = tree.read('src/pc/gcm/renderengine/XenonD3D9Shims.cpp')
methods = 'namespace CgsGraphics {\n' + definition(camera, 'namespace\n    {') + '\n'
methods += '\n'.join(definition(camera, sig) for sig in (
    'void Camera::LookAt(', 'void Camera::SetFovHorizontal(',
    'void Camera::UpdatePerspectiveProjectionMatrix(', 'void Camera::UpdateViewProjectionMatrix(',
    'void Camera::Release()', 'Camera::Camera(const Camera&', 'Camera& Camera::operator=(',
    'void Camera::GetFrustumPerspective(CameraRwFrustum&')) + '\n}\n'
tables = '\n'.join(re.search(r'static const rw::math::vpu::Vector3 ' + name + r'\[E_FACE_NUM\].*?;',
                            envmap, re.S).group() for name in (
    'KAV_ENV_MAP_LOOK_DIRECTIONS', 'KAV_ENV_MAP_UP_DIRECTIONS'))
constants = '\n'.join(re.findall(r'^\s*const f32 KF_ENVMAP_\w+\s*=.*?;', envmap, re.M))
methods += 'namespace BrnGraphics {\n' + tables + '\n' + constants + '\n'
methods += '\n'.join(definition(envmap, sig) for sig in (
    'bool EnvironmentMap::Prepare(', 'void EnvironmentMap::Update(')) + '\n}\n'
sky_setups = re.findall(r'lFaceCamera\.SetFarClipPlane\( 10000\.0f \);[^\n]*\n'
                       r'(?:\s*renderengine::SetEnvironmentMapProjectionPC\(lFaceCamera\);\n)?', world)
assert sky_setups, 'The production sky projection rebuild must be covered'
methods += '\nconstexpr u32 KU_SKY_PROJECTIONS = %du;\n' % len(sky_setups)
methods += '\nvoid SkyProjection(u32 luIndex, CgsGraphics::Camera& lFaceCamera) {\nswitch (luIndex) {\n'
for index, setup in enumerate(sky_setups):
    methods += 'case %d: {\n%s\nbreak;\n}\n' % (index, setup)
methods += '}\n}\n'
begin = definition(renderer, 'void BrnRendererModule::BeginRenderEnvironmentMapFace(')
cull = re.search(r'E_FACTORY_RASTERIZER_STATE_SCISSOR_CULL_MODE_\w+', begin).group()
methods += '\nconstexpr auto KE_REFLECTION_CULL = ' + cull + ';\n'
methods += '\n' + '\n'.join(definition(shims, sig) for sig in (
    'inline DWORD XenonCullToD3D9(', 'void D3DDevice_SetRenderState_CullMode('))
leaf = 'src/pc/gcm/renderengine/EnvironmentMapPCLeaf.h'
try:
    leaf_source = tree.read(leaf)
except FileNotFoundError:
    leaf_source = ''
result = compile_and_run(Path(__file__).with_name('PCReflectionOrientation.cpp'),
    'reflection_orientation.inc', methods, 'PCReflectionOrientation',
    shadow={leaf: leaf_source or '#pragma once\n',
        'src/GameShared/GameClasses/Core/CgsAssert.h':
            '#pragma once\n#include <cstdlib>\n#define CGS_ASSERT(ok, ...) do { if (!(ok)) std::abort(); } while (0)\n',
        'src/GameShared/GameClasses/Graphics/CgsResourceAllocatorCreate.h':
            '#pragma once\nvoid* AllocateTestState(void*, const void*);\nnamespace CgsGraphics {\n'
            'inline void* ResourceAllocatorCreate(void*, void* out, const void* descriptor) '
            '{ return AllocateTestState(out, descriptor); }\n}\n'},
    extra_sources=[REPO / 'src/GameShared/GameClasses/Graphics/CgsRasterizerStateFactory.cpp',
                   REPO / 'src/pc/gcm/renderengine/RasterizerState.cpp'],
    extra_flags='d3d9.lib d3dcompiler.lib user32.lib')
raise SystemExit(report('run_pc_reflection_orientation', [], result, 1))
