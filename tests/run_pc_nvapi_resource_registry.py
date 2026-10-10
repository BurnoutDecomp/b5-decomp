"""NVAPI lifetime contract and real 8x MSAA depth pixels across resize/reset."""
from pathlib import Path
import argparse
import os
from fxgs_common import Tree, definition, code_only, compile_and_run, report

os.environ.pop('NoDefaultCurrentDirectoryInExePath', None)
p = argparse.ArgumentParser()
p.add_argument('--untracked-overflow', action='store_true')
a = p.parse_args()
t = Tree()
path = 'src/pc/gcm/renderengine/NvApiResourceRegistry.h'
source = t.read(path)
if a.untracked_overflow:
    source = source.replace('            maResources.push_back(lpResource);',
        '            if (maResources.size() >= 8) return mpRegister(lpResource) == 0;\n'
        '            maResources.push_back(lpResource);')
shim = t.read('src/pc/gcm/renderengine/XenonD3D9Shims.cpp')
start = shim.index('    typedef void* (__cdecl* NvApiQueryInterfaceFn)')
end = shim.index('    // Returns true when the NVAPI path RAN', start)
resolve = shim[start:end] + definition(shim, 'bool TilingResolveDepthViaNvApi(')
pixels = t.read('tests/PCFlipResources.cpp')
sampler = definition(pixels, 'static ID3DBlob* Compile(') + '\n' + definition(pixels, 'struct Sampler') + ';\n'
resize = code_only(definition(t.read('src/pc/gcm/renderengine/PostFxRenderTarget.cpp'),
    'bool PCResizeDisplayTargets('))
flip = code_only(t.read('src/pc/gcm/renderengine/FlipSwapChain.h'))
wiring = [
    ('target replacement retires registrations before releasing owners',
     0 <= resize.find('gNvApiDepthResourcesPC.Clear()') < resize.find('Device::ResizeDisplay(')
     < resize.find('lpTexture->mpD3DTexture->Release()')),
    ('flip reset retires registrations before ResetEx',
     0 <= flip.find('gNvApiDepthResourcesPC.Clear()') < flip.find('mpDevice->ResetEx(')),
]
result = compile_and_run(Path(__file__).with_name('PCNvApiResourceRegistry.cpp'),
    'pc_nvapi_resolve.inc', resolve, 'PCNvApiResourceRegistry',
    shadow={path: source}, extra_files={'pc_nvapi_sampler.inc': sampler},
    extra_flags='d3d9.lib d3dcompiler.lib user32.lib')
raise SystemExit(report('run_pc_nvapi_resource_registry', wiring, result, 18))
