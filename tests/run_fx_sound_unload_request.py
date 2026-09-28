"""Production unload constructor: bundle-only requests have no resource handle."""
import argparse
from pathlib import Path
from fxgs_common import Tree, definition, compile_and_run, report

HERE = Path(__file__).resolve().parent
parser = argparse.ArgumentParser()
parser.add_argument('--rev')
parser.add_argument('--source-root', type=Path)
args = parser.parse_args()
path = 'src/GameSource/Sound/BrnResourceRegistrar.cpp'
source = ((args.source_root / path).read_text(encoding='utf-8')
          if args.source_root else Tree(args.rev).read(path))
code = definition(source, 'ResourceRegistrar::QueuedResource::QueuedResource(const RequestedResource& lrSource)')
numeric = compile_and_run(HERE / 'FxSoundUnloadRequest.cpp', 'fx_sound_unload_request.inc', code, 'FxSoundUnloadRequest')
raise SystemExit(report('run_fx_sound_unload_request', [], numeric, 16))
