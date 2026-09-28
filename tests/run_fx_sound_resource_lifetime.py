"""Run original effect-ready/detach bodies; --rev HEAD is the car-swap RED."""
import argparse
from pathlib import Path
from fxgs_common import Tree, definition, compile_and_run, report
HERE = Path(__file__).resolve().parent
parser = argparse.ArgumentParser()
parser.add_argument('--rev')
parser.add_argument('--source-root', type=Path)
args = parser.parse_args()
def read(name):
    path = 'src/GameSource/Sound/Module/LogicModule/' + name
    return (args.source_root / path).read_text(encoding='utf-8') if args.source_root else Tree(args.rev).read(path)
obj = read('BrnEffectObject.h')
control = read('BrnEffectControl.cpp')
code = definition(obj, 'virtual void ResourcesAreReady()').replace('virtual void ResourcesAreReady()', 'void BrnEffectObject::ResourcesAreReady()')
code += '\n' + definition(obj, 'virtual bool Detach()').replace('virtual bool Detach()', 'bool BrnEffectObject::Detach()')
code += '\n' + definition(control, 'void BrnEffectControl::ResourcesAreReady()')
code += '\n' + definition(control, 'bool BrnEffectControl::Detach()')
numeric = compile_and_run(HERE / 'FxSoundResourceLifetime.cpp', 'fx_sound_resource_lifetime.inc', code, 'FxSoundResourceLifetime')
raise SystemExit(report('run_fx_sound_resource_lifetime', [], numeric, 30))
