"""Installed wheel VS through the real native per-instance constant consumer.

Actual wheel positions/declaration and installed programs are read from the
staged assets. The observation PS exposes world XYZ and wheel blur; the CPU
reference projects those same world points without using the wheel program.
Native geometry submission/cache entry is the fixture boundary, not a claim
about the complete live dispatch-bin/terrain producer.
"""
import argparse
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
from fxgs_common import Tree, definition, compile_and_run, report, WORKFLOW

os.environ.pop('NoDefaultCurrentDirectoryInExePath', None)
parser = argparse.ArgumentParser()
parser.add_argument('--matrix-zero', action='store_true')
args = parser.parse_args()
sys.path.insert(0, str(WORKFLOW / 'tools/assets/bundles'))
import vehicle_skin_audit as skin
tree = Tree()
device = tree.read('src/GameShared/GameClasses/Graphics/Dispatch/shadowingdevice.cpp')
native = tree.read('src/pc/gcm/renderengine/XenonD3D9Shims.cpp')
methods = 'namespace renderengine {\n' + definition(native, 'void WorldShaderConstants_Set(') + '\n}\n'
methods += 'namespace shadow {\n' + definition(device, 'void Device::DrawInstancedMeshPC(') + '\n}\n'
shadow = {}
if args.matrix_zero:
    path = 'src/pc/gcm/renderengine/InstancedDraw.h'
    header = tree.read(path)
    before = 'mpMatrices + static_cast<u32>(selected) * 16'
    assert header.count(before) == 1
    shadow[path] = header.replace(before, 'mpMatrices')

with tempfile.TemporaryDirectory(prefix='brn_wheel_shader_') as folder:
    folder = Path(folder)
    for bundle, output in [(WORKFLOW/'build/game/SHADERS.BNDL', 'shaders'),
                           (WORKFLOW/'build/game/WHEELS/WHE_51916650_GR.BNDL', 'wheel')]:
        subprocess.run([str(WORKFLOW/'build/tools/yap/YAP.exe'), 'e', str(bundle), str(folder/output)],
                       stdout=subprocess.DEVNULL, check=True)
    header = folder/'wheel/Renderable/0395BA0E_header.dat'
    mesh = skin.meshes_pc(header.read_bytes(), skin.read_imports(str(header)+'_imports.yaml'))[0]
    vds = skin.load_vds(str(folder/'wheel'), '<')
    declaration = skin.parse_vd_elements(*vds[mesh['vd'][0]])
    assert declaration == (24, [(0,0,0,0x2A23B9), (3,0,12,0x2A2187),
                               (6,0,16,0x2A2187), (5,0,20,0x2C235F)])
    body = header.with_name('0395BA0E_body.dat').read_bytes()
    base = mesh['vb'][0][6] & ~3
    # Three real wheel points spanning its authored radial plane.
    packed = b''.join(body[base+i*24:base+(i+1)*24] for i in [0,8,9])
    methods += 'static const unsigned char wheelVertices[] = {' + ','.join(map(str,packed)) + '};\n'
    # All four colour wheel variants carry the same XYZ/blur vertex program.
    codes = []
    for rid in ['AB4DE44C','9CFA74B1','EB89C988','DC3E5975','30C0F622']:
        blob = (folder/'shaders/ShaderProgramBuffer'/f'{rid}_header.dat').read_bytes()
        code = blob[20:20+struct.unpack_from('<I',blob,8)[0]]
        assert struct.unpack_from('<I',code)[0] == 0xfffe0300
        codes.append(code)
    assert len(set(codes[:4])) == 1, 'new colour wheel vertex variants need separate checks'
    for name, code in [('wheelVS',codes[0]), ('wheelZVS',codes[4])]:
        methods += f'static const unsigned char {name}[] = {{' + ','.join(map(str,code)) + '};\n'
    result = compile_and_run(Path(__file__).with_name('PCWheelShader.cpp'),
        'wheel_shader.inc', methods, 'PCWheelShader', shadow=shadow,
        extra_flags='d3d9.lib d3dcompiler.lib user32.lib')
raise SystemExit(report('run_pc_wheel_shader', [], result, 1))
