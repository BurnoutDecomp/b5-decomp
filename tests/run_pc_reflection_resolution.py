"""Production reflection allocation, INI/menu edits and native cube/MSAA resources."""
import argparse
import os
import re
from pathlib import Path
from fxgs_common import Tree, definition, compile_and_run, report, REPO

os.environ.pop('NoDefaultCurrentDirectoryInExePath', None)
parser = argparse.ArgumentParser()
parser.add_argument('--rev')
args = parser.parse_args()
tree = Tree(args.rev)
memory = tree.read('src/GameSource/Graphics/BrnRendererMemory.cpp')
constants = re.findall(r'^\s*const [us]32 (?:KU_ENV_MAP\w*|KI_ENV_MAP\w*|KU_COLOUR_SURFACE\w*)[^;]+;', memory, re.M)
code = '\n'.join(constants) + '\n' + definition(memory, 'void BrnRendererMemory::CreateEnvmapBuffer(') + '\n'
camera = tree.read('src/GameShared/GameClasses/Graphics/CgsCamera.cpp')
code += 'namespace CgsGraphics {\n' + definition(camera, 'namespace\n    {') + '\n'
for sig in ('void Camera::UpdatePerspectiveProjectionMatrix(', 'void Camera::UpdateViewProjectionMatrix('):
    code += definition(camera, sig) + '\n'
code += '}\n'
variable = tree.read('src/GameShared/GameClasses/Development/DebugSystem/Core/UI/Variables/CgsVariable.cpp')
manager = tree.read('src/GameShared/GameClasses/Development/DebugSystem/Core/UI/Variables/CgsVariableManager.cpp')
code += 'namespace CgsDev::DebugUI {\n'
for sig in ('bool VariableMetadata::Prepare(', 'bool Variable::Prepare(const Variant&',
            'Variant&    Variable::GetValue(', 'const char* Variable::GetName(',
            'VariableMetadata* Variable::FindMetadata(', 'bool Variable::IsReadOnly(',
            'const StringList* Variable::GetStringList(', 'void Variable::AddMetadata(',
            'void Variable::Increment(', 'void Variable::Decrement(',
            'void Variable::OnChange(', 'void Variable::SetValueFromString('):
    code += definition(variable, sig) + '\n'
code += definition(manager, 'void VariableManager::ApplyIniOverridesPC(') + '\n}\n'
leaf = tree.read('src/pc/gcm/renderengine/PostFxRenderTarget.cpp')
code += definition(leaf, 'struct MultisampleChoice\n') + ';\n'
code += 'const D3DFORMAT KAE_MSAA_DEPTH_FORMATS[3] = {D3DFMT_D24S8,D3DFMT_D24X8,D3DFMT_D16};\n'
for sig in ('MultisampleChoice ChooseMultisampleType(', 'IDirect3DSurface9* CreateMultisampledColourSurface(',
            'IDirect3DSurface9* CreateMultisampledDepthSurface('):
    code += definition(leaf, sig) + '\n'
result = compile_and_run(Path(__file__).with_name('PCReflectionResolution.cpp'),
    'pc_reflection_resolution.inc', code, 'PCReflectionResolution',
    extra_sources=(REPO / 'src/pc/gcm/renderengine/reflections/Resolution.cpp',
        REPO / 'src/GameShared/GameClasses/Development/DebugSystem/Core/UI/CgsTypes.cpp'),
    extra_flags='d3d9.lib user32.lib')
raise SystemExit(report('run_pc_reflection_resolution', [], result, 1))
