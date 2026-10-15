"""Verify the production final sum stays transparent below full scale; uses a muted endpoint."""
import argparse
import os
from pathlib import Path
from fxgs_common import Tree, STRSTREAM_CPP, compile_and_run

parser = argparse.ArgumentParser()
parser.add_argument('--rev')
args = parser.parse_args()
os.environ.pop('NoDefaultCurrentDirectoryInExePath', None)
source = Tree(args.rev).read('src/GameShared/GameClasses/System/PC/CgsAudioOutputPC.cpp')
result = compile_and_run(Path(__file__).with_name('AudioOutputTransparency.cpp'),
                         'audio_output_transparency.inc', source, 'AudioOutputTransparency',
                         extra_sources=[STRSTREAM_CPP], extra_flags='ole32.lib uuid.lib')
raise SystemExit(0 if result and result[0] == 15 and result[1] == 0 else 1)
