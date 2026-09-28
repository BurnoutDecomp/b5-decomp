"""Execute the production reverb update and voice writes against controlled inputs."""
import argparse
from pathlib import Path
import sys
sys.dont_write_bytecode = True
from fxgs_common import Tree, definition, compile_and_run, report, code_only

parser = argparse.ArgumentParser()
parser.add_argument('--rev')
parser.add_argument('--source-root', type=Path)
args = parser.parse_args()
tree = Tree(args.rev)
def read(path):
    return (args.source_root / path.removeprefix('src/')).read_text(encoding='utf-8-sig') if args.source_root else tree.read(path)
base = 'src/GameSource/Sound/Vehicles/Environment/BrnReverbEffect'
source = read(base + '.cpp')
header = read(base + '.h')
params = code_only(read('src/GameSource/AttribSys/Generated/classes/reverbparams.h'))
globaldata = code_only(read('src/GameSource/AttribSys/Generated/classes/burnoutglobaldata.h'))
signatures = ('void ReverbEffect::UpdateParams(', 'void ReverbEffect::ProcessUpdate(')
parts = []
for signature in signatures:
    try:
        parts.append(definition(source, signature))
    except ValueError:
        # The old class inherits these two literal no-op base methods.
        parts.append(signature + ('f32 afTimeStep) {}' if 'UpdateParams' in signature else ') {}'))
start = source.find('// Initial values at 82F2CE20')
if start >= 0:
    parts.insert(0, source[start:source.index('// ARTIST 826D1498.', start)])
else:
    parts.insert(0, 'float KF_REVERB_INTERP_TIME_MAX=500, KF_REVERB_INTERP_TIME_MIN=100; bool KB_DEBUG_REVERB_ZONE=false;')
wiring = [
    ('two per-frame overrides are declared', all(s in header for s in ('void UpdateParams(', 'void ProcessUpdate('))),
    ('attribute ctor accepts RefSpec and full ARTIST class key', 'const RefSpec&' in params and '0xA59AD4BD63B62A88' in params),
    ('global preset accessor returns typed RefSpec', 'const RefSpec& burnoutglobaldata::ReverbSettings' in globaldata),
]
try:
    accessor = definition(params, 'u64 GetClass() const')
except ValueError:
    # Before this generated accessor existed, the real shared Instance method
    # returned only the low word. Execute that old implementation for the red run.
    old = tree.read('src/SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribinstance.cpp')
    accessor = definition(old, 'int Instance::GetClass() const').replace('Instance::', '')
attribute_fixture = '''namespace ReverbAttributeTest {
using u64 = uint64_t;
struct Class { u64 mKey; u64 GetKey() const { return mKey; } };
struct Collection { Class* mpClass; };
struct Accessor { Collection* mpCollection; ''' + accessor + ''' };
}
'''
result = compile_and_run(Path(__file__).with_name('FxReverb.cpp'), 'fx_reverb.inc',
                         '\n'.join(parts), 'FxReverb',
                         extra_files={'fx_reverb_attribute.inc': attribute_fixture})
raise SystemExit(report('run_fx_reverb', wiring, result, 31))
