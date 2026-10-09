"""Native grazing texture footprints through the production world sampler setting."""
from pathlib import Path
import os
import struct
import subprocess
import sys
import tempfile
from fxgs_common import compile_and_run, report, WORKFLOW

os.environ.pop('NoDefaultCurrentDirectoryInExePath', None)
sys.path.insert(0, str(WORKFLOW / 'tools/assets/shaders'))
from convert_shaders_bundle import find_fxc

with tempfile.TemporaryDirectory(prefix='world_filter_shader_') as directory:
    source = Path(directory) / 'filter.fx'
    output = source.with_suffix('.fxo')
    source.write_text('sampler2D Source : register(s0);\n'
        'float4 Main(float2 uv:TEXCOORD0):COLOR0 { '
        'return tex2Dgrad(Source,uv,float2(0.25,0),float2(0,1.0/256.0)); }\n')
    subprocess.run([find_fxc(), '/nologo', '/T', 'ps_3_0', '/E', 'Main', '/O2', '/Fo', str(output), str(source)],
                   check=True, capture_output=True)
    data = output.read_bytes()
    words = struct.unpack('<%dI' % (len(data)//4), data)
    include = 'const DWORD kauWorldFilterProgram[] = {' + ','.join('0x%08X' % word for word in words) + '};\n'
    numeric = compile_and_run(Path(__file__).with_name('PCWorldTextureFiltering.cpp'),
        'world_texture_filtering.inc', include, 'PCWorldTextureFiltering', extra_flags='d3d9.lib user32.lib')
raise SystemExit(report('run_pc_world_texture_filtering', [], numeric, 1))
