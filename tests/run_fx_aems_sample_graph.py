"""Execute the production AEMS factory and controls against RWAC graph spies."""
import argparse
from pathlib import Path
import sys
sys.dont_write_bytecode = True
from fxgs_common import Tree, definition, compile_and_run, report

parser = argparse.ArgumentParser()
parser.add_argument('--source', type=Path)
parser.add_argument('--rev')
args = parser.parse_args()
path = 'src/GameShared/GameClasses/Sound/Playback/AEMS/CgsAemsInterfaceImplementation.cpp'
source = args.source.read_text(encoding='utf-8-sig') if args.source else Tree(args.rev).read(path)
code = '\n'.join(definition(source, signature) for signature in (
    'Snd9::IAemsSamplePlayer* AemsRWSampleFactory::CreateInstance(',
    'void AemsRWSamplePlayer::SetInput(',
    'void AemsRWSamplePlayer::Pause()',
    'void AemsRWSamplePlayer::Unpause()'))
numeric = compile_and_run(Path(__file__).with_name('FxAemsSampleGraph.cpp'),
                          'fx_aems_sample_graph.inc', code, 'FxAemsSampleGraph')
raise SystemExit(report('run_fx_aems_sample_graph', [], numeric, 36))
