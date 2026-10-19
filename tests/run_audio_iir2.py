"""Compare the production biquad kernel with ARTIST's frozen scalar instruction trace."""
import argparse
import os
from pathlib import Path
from fxgs_common import Tree, compile_and_run

parser = argparse.ArgumentParser()
parser.add_argument('--rev')
args = parser.parse_args()
os.environ.pop('NoDefaultCurrentDirectoryInExePath', None)
source = Tree(args.rev).read('vendor/renderware/src/rw/audio/core/Iir2.cpp')
result = compile_and_run(Path(__file__).with_name('AudioIir2.cpp'),
                         'audio_iir2.inc', source, 'AudioIir2')
raise SystemExit(0 if result and result[1] == 0 else 1)
