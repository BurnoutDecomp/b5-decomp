"""Production config/global/LOD checks plus real D3D9 AA creation, drawing and resolve."""
from pathlib import Path
from fxgs_common import Tree, definition, compile_and_run, report

HERE = Path(__file__).resolve().parent
tree = Tree()
main = tree.read('src/GameSource/Main/BrnMain.cpp')
leaf = tree.read('src/pc/gcm/renderengine/PostFxRenderTargetPCLeaf.cpp')
world = tree.read('src/GameSource/World/BrnWorldModule.cpp')
code = '\n'.join(definition(main, signature) for signature in (
    'void LoadConfig(', 'void SaveFullscreenConfigPC(', 'void SaveConfig('))
code += '\n' + definition(leaf, 'struct MultisampleChoice\n') + ';\n'
code += 'const u32 KU_MULTISAMPLE_MAX_SAMPLES = 8u;\n'
code += 'const D3DFORMAT KAE_MSAA_DEPTH_FORMATS[3] = {D3DFMT_D24S8,D3DFMT_D24X8,D3DFMT_D16};\n'
code += '\n'.join(definition(leaf, signature) for signature in (
    'u32 MultisampleSampleCountForConsoleFormat(', 'u32 MultisampleOverrideSampleCount(',
    'MultisampleChoice ChooseMultisampleType(', 'IDirect3DSurface9* CreateMultisampledColourSurface(',
    'IDirect3DSurface9* CreateMultisampledDepthSurface('))
code += '\nCgsGraphics::Model::State ' + definition(world, 'ClassifyVehicleLOD(')
wiring = []
for path, anchor, members in (
    ('src/GameSource/World/EntityModules/WorldEntityModule/BrnWorldEntityModule.cpp',
     'WorldEntityModule::Construct(', ('miEnvironmentMapLOD = lrGraphics.miEnvironmentMapLod',
     'lrGraphics.ApplyLodOverride(lrGraphics.miWorldLodOverrideDistance')),
    ('src/GameSource/World/EntityModules/PropEntityModule/BrnPropEntityModule.cpp',
     'PropEntityModule::Construct(', ('lrGraphics.ApplyLodOverride(lrGraphics.miPropLodOverrideDistance',)),
    ('src/GameSource/World/ShadowMap/BrnShadowMap.cpp',
     'ShadowMap::Construct(', ('mbRenderTrafficIntoShadowMap = renderengine::GetGraphicsSettingsPC().mbTrafficShadows',))):
    body = definition(tree.read(path), anchor)
    wiring.append((path + ' settings seed original members in Construct', all(member in body for member in members)))
numeric = compile_and_run(HERE / 'PCGraphicsSettings.cpp', 'pc_graphics_settings.inc', code,
    'PCGraphicsSettings', extra_flags='d3d9.lib user32.lib shell32.lib')
raise SystemExit(report('run_pc_graphics_settings', wiring, numeric, 1))
