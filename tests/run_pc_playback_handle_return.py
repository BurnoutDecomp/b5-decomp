"""Returned sound handles must not depend on optional named-return elision."""
from pathlib import Path
import argparse
import os
from fxgs_common import Tree, definition, compile_and_run, report

os.environ.pop('NoDefaultCurrentDirectoryInExePath', None)
parser = argparse.ArgumentParser()
parser.add_argument('--rev', help='source revision for the missing-copy-definition control')
parser.add_argument('--drop-acquire', action='store_true')
args = parser.parse_args()
source=Tree(args.rev).read('src/GameShared/GameClasses/Sound/Playback/CgsEnvironment.cpp')
body='\n'.join(definition(source, signature) for signature in (
    'Handle<Factory> Environment::GetFactory(', 'Handle<Voice> Environment::GetVoice(',
    'Handle<Voice> Environment::GetRwacVoiceByPlugin(', 'Handle<Environment> Environment::Create('))
if args.drop_acquire:
    body=body.replace('lpFactory->Acquire();','').replace('lpVoice->Acquire();','').replace('lpEnvironment->Acquire();','(void)lpEnvironment;')
result=compile_and_run(Path(__file__).with_name('PCPlaybackHandleReturn.cpp'), 'playback_handle_return.inc', body,
    'PCPlaybackHandleReturn', extra_flags='/Zc:nrvo-')
raise SystemExit(report('run_pc_playback_handle_return',[],result,10))
