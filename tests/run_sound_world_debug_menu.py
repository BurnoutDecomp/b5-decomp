"""Check the recovered static-sound debug draw seam and production RGBA packing."""
import argparse
import os
from pathlib import Path

from fxgs_common import REPO, Tree, compile_and_run, definition, report

os.environ.pop('NoDefaultCurrentDirectoryInExePath', None)
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--rev')
args = parser.parse_args()
tree = Tree(args.rev)
source = tree.read('src/SharedClasses/Sound/World/BrnSoundWorldScene.cpp')
body = definition(source, 'void SoundWorldScene::QuerySoundMap(')
draw = body.find('lDebugInterface.GetRender().DrawSphere(')
capacity = body.find('lrQuery.miEntitiesWritten >= lrQuery.miMaxEntities')
checks = [
    ('mbDrawDebug gates the scene debug draw', 'if (lrQuery.mbDrawDebug &&' in body),
    ('draw uses the query radius predicate',
     'lrQuery.mfRadius * lrQuery.mfRadius' in body and
     body.find('lrQuery.mfRadius * lrQuery.mfRadius') < draw),
    ('draw uses the entity position and radius',
     'DrawSphere(\n                        lPos, lfRadius,' in body),
    ('debug draw precedes the output-capacity exit', draw >= 0 and capacity > draw),
    ('entity-radius output predicate remains', 'lfRadius * lfRadius' in body),
    ('color uses the production rw::RGBA constructor',
     'rw::RGBA lColour(0xA0, 0xFF, 0x00, 0xFF)' in body and
     'lColour.m_rgba' in body),
]
for label, passed in checks:
    print(('PASS  ' if passed else 'FAIL  ') + label)

result = compile_and_run(
    Path(__file__).with_name('SoundWorldDebugMenu.cpp'),
    'sound_world_debug_menu.inc', '', 'SoundWorldDebugMenu',
    extra_sources=(str(REPO / 'src/GameShared/GameClasses/RenderWare/RwRGBA.cpp'),))
raise SystemExit(report('run_sound_world_debug_menu', checks, result, 1))
