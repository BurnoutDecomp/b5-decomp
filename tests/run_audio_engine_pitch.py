"""Run production Ginsu parameter updates at RPM boundaries (ARTIST 826B3908/826B3B68).
Use --rev 27fdb66ff as the negative control before the pitch-floor repair.
"""
import argparse
import os
from pathlib import Path
from fxgs_common import Tree, compile_and_run, definition
os.environ.pop('NoDefaultCurrentDirectoryInExePath', None)
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--rev')
args = parser.parse_args()
source = Tree(args.rev).read('src/GameSource/Sound/Vehicles/Engines/BrnDualGinsuEffect.cpp')
text = 'namespace BrnSound { namespace Vehicles { namespace Engines {\n'
for method in ['void DualGinsuEffect::UpdateAccelGinsu()', 'void DualGinsuEffect::UpdateDecelGinsu()']:
    text += definition(source, '\n' + method) + '\n'
text += '} } }\n'
result = compile_and_run(Path(__file__).with_name('AudioEnginePitch.cpp'),
                         'audio_engine_pitch.inc', text, 'AudioEnginePitch')
raise SystemExit(0 if result and result[1] == 0 else 1)
