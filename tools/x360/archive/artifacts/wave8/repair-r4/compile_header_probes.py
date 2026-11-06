"""Actual R4 typed-header/ABI gates and the independently retained child-size negative."""
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess

HERE = Path(__file__).resolve().parent
WORK = Path('C:/Users/adri1/.codex/worktrees/bp360/wave4-intake')
HEAD = json.loads((HERE / 'repair-r4-receipts.json').read_text())['head']
report = json.loads((HERE / 'selected-stage1/compile-results.json').read_text())
metadata = json.loads((HERE / 'native-parallel-wave8-r4-selected-stage1-patch.json').read_text())
assert report['state'] == 'complete' and metadata['head'] == HEAD
OUT = HERE / 'header-probes'
assert not OUT.exists()
OUT.mkdir()
TEMP = OUT / 'compiler-temp'
TEMP.mkdir()


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def check_source():
    assert subprocess.check_output(['git', '--no-optional-locks', '-C', str(WORK), 'rev-parse', 'HEAD']).decode().strip() == HEAD
    assert not subprocess.check_output(['git', '--no-optional-locks', '-C', str(WORK), 'status', '--porcelain=v1']).strip()
    for path, digest in metadata['source_snapshot']['hashes'].items():
        assert sha(WORK / path) == digest, path


def symbol_words(path, prefix, count):
    data = Path(path).read_bytes()
    machine, sections, _, symbols, nsyms, optional, _ = struct.unpack_from('<HHLLLHH', data)
    assert machine == 0x1f2
    strings = symbols + nsyms * 18
    index = 0
    while index < nsyms:
        entry = symbols + index * 18
        name = data[entry:entry + 8]
        if name[:4] == bytes(4):
            start = strings + struct.unpack_from('<I', name, 4)[0]
            name = data[start:data.index(0, start)]
        else:
            name = name.split(bytes(1), 1)[0]
        value, section = struct.unpack_from('<Ih', data, entry + 8)
        if prefix.encode() in name and section > 0:
            header = 20 + optional + (section - 1) * 40
            size, offset = struct.unpack_from('<II', data, header + 16)
            assert value + count * 4 <= size
            return list(struct.unpack_from('>' + 'I' * count, data, offset + value))
        index += 1 + data[entry + 17]
    raise AssertionError(('Missing actual data symbol', str(path), prefix))


check_source()
row = report['results'][0]
flags = [arg for arg in row['command'][1:-1] if not arg.startswith(('/Fo', '/Fa'))]
flags += ['/FAcs', '/showIncludes']
layout = '''#include "GameSource/World/BrnWorldModule.h"
#include "GameShared/GameClasses/Graphics/CgsShaderConstants.h"
#include "GameSource/Graphics/BrnCoronaManager.h"
#include "GameSource/Effects/Curves.h"
extern "C" const unsigned int R4LayoutFacts[] = {
 sizeof(BrnWorld::World), __alignof(BrnWorld::World),
 sizeof(BrnWorld::RaceCarEntityModule), __alignof(BrnWorld::RaceCarEntityModule),
 sizeof(BrnReplays::RaceCarEntitySerialiser), __alignof(BrnReplays::RaceCarEntitySerialiser),
 sizeof(BrnWorld::WorldEntityModule), __alignof(BrnWorld::WorldEntityModule),
 sizeof(BrnTraffic::TrafficEntityModule), __alignof(BrnTraffic::TrafficEntityModule),
 sizeof(BrnTraffic::VehicleTypeRuntime),
 sizeof(CgsGraphics::ShaderConstantsExternal), sizeof(CgsGraphics::ShaderConstantsInternal), sizeof(CgsGraphics::ShaderConstantsCPU),
 sizeof(rw::math::fpu::Vector2Template<float>), sizeof(rw::math::fpu::Vector3Template<float>),
 sizeof(Vector2), sizeof(rw::RGBA), sizeof(BrnEffects::Curves::SmoothStep),
 sizeof(renderengine::Corona), sizeof(renderengine::CoronaBuffer), sizeof(renderengine::CoronaBuffer::Iterator),
 sizeof(BrnCoronaTypeParams), sizeof(BrnCoronaManager::BrnSubmissionInterface), sizeof(BrnCoronaManager)
};
CGS_STATIC_ASSERT((sizeof(BrnWorld::World)==0x5e3c80), "R4 measured World");
CGS_STATIC_ASSERT((sizeof(BrnWorld::WorldEntityModule)==0x79e0), "R4 retained child footprint, not native match");
'''
negative = '''#include "GameSource/World/EntityModules/WorldEntityModule/BrnWorldEntityModule.h"
CGS_STATIC_ASSERT((sizeof(BrnWorld::WorldEntityModule)==0x79f0), "Independent ARTIST WorldEntity79F0 remains negative");
'''
sources = {'CombinedLayouts': layout,
           'MatrixForward': Path('D:/Reverse/BP-X360-Experimental-Artifacts/matrix-home-r3-terra/probe_forward.cpp').read_text(),
           'MatrixReverse': Path('D:/Reverse/BP-X360-Experimental-Artifacts/matrix-home-r3-terra/probe_reverse.cpp').read_text(),
           'Traffic51': Path('D:/codex-native/blobby-shadow-terra/native_traffic_layout_probe.cpp').read_text(),
           'CoreABI': Path('D:/Reverse/BP-X360-Experimental-Worktrees/rendercore-corona-r1-a26/scratch/corona-r1/core_probe.cpp').read_text(),
           'WorldEntityNativeNegative': negative}
env = dict(os.environ, TEMP=str(TEMP), TMP=str(TEMP))
rows = []
for name, source in sources.items():
    cpp, obj, asm, log = [OUT / (name + suffix) for suffix in ('.cpp', '.obj', '.asm', '.log')]
    cpp.write_text(source, encoding='utf-8')
    command = [row['command'][0], *flags, '/Fo' + str(obj), '/Fa' + str(asm), str(cpp)]
    run = subprocess.run(command, cwd=WORK, env=env, capture_output=True, timeout=180)
    log.write_bytes(run.stdout + run.stderr)
    actual_pass = run.returncode == 0 and obj.exists() and struct.unpack_from('<H', obj.read_bytes())[0] == 0x1f2
    expected_negative = name == 'WorldEntityNativeNegative'
    satisfied = (run.returncode != 0 and b'C2118' in log.read_bytes() and not obj.exists()) if expected_negative else actual_pass
    inputs = {}
    for line in log.read_text(errors='replace').splitlines():
        if 'including file:' in line:
            path = Path(line.split('including file:', 1)[1].strip())
            if path.is_file():
                inputs[str(path.resolve())] = sha(path)
    rows.append({'name': name, 'head': HEAD, 'source': str(cpp), 'source_sha256': sha(cpp),
                 'command': command, 'returncode': run.returncode, 'actual_passed': actual_pass,
                 'expected_negative': expected_negative, 'gate_satisfied': satisfied,
                 'actual_included_headers': inputs, 'object': str(obj) if obj.exists() else None,
                 'artifact_sha256': {str(path): sha(path) for path in (log, obj, asm) if path.exists()}})
    print(name, run.returncode, 'satisfied', satisfied, flush=True)

expected = [0x5e3c80, 0x80, 0x18800, 0x80, 0x80, 0x80, 0x79e0, 0x10, 0x79800, 0x80, 0x80,
            16, 20, 16, 8, 12, 16, 4, 20, 64, 8, 12, 48, 112, 320]
values = symbol_words(OUT / 'CombinedLayouts.obj', 'R4LayoutFacts', len(expected))
assert values == expected, (values, expected)
traffic_original = Path('D:/Reverse/BP-X360-Experimental-Artifacts/traffic-align128-final/native_traffic_layout_probe-4863347a.obj')
traffic_values = symbol_words(OUT / 'Traffic51.obj', 'TrafficLayout', 104)
assert traffic_values == symbol_words(traffic_original, 'TrafficLayout', 104)
check_source()
result = {'head': HEAD, 'positive_requested': 5, 'positive_passed': sum(r['actual_passed'] for r in rows),
          'expected_native_child_negative_retained': rows[-1]['gate_satisfied'], 'rows': rows,
          'layout_values': values, 'layout_expected': expected, 'Traffic51_words': traffic_values,
          'Traffic51_reference_object': str(traffic_original), 'Traffic51_reference_sha256': sha(traffic_original),
          'source_input_hashes': 6299, 'selected_metadata_sha256': sha(HERE / 'native-parallel-wave8-r4-selected-stage1-patch.json'),
          'scope': 'Actual typed-header/ABI compile and emitted data; WorldEntity79E0!=79F0 remains an independent failure. No whole-contained ABI/runtime/hardware claim.',
          'created_utc': datetime.now(timezone.utc).isoformat()}
with (OUT / 'probe-results.json').open('x', encoding='utf-8') as stream:
    json.dump(result, stream, indent=2)
assert all(r['gate_satisfied'] for r in rows)
print(json.dumps({'head': HEAD, 'positive_passed': result['positive_passed'], 'positive_requested': 5,
                  'independent_child_negative_retained': True, 'report_sha256': sha(OUT / 'probe-results.json')}), flush=True)
