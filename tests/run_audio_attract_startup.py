"""Execute the production attract event producer; --rev selects a negative control."""
import argparse
import os
from pathlib import Path
from fxgs_common import Tree, definition, compile_and_run

os.environ.pop('NoDefaultCurrentDirectoryInExePath', None)
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--rev')
args = parser.parse_args()
source = Tree(args.rev).read('src/GameSource/Gui/Flow/HUD/States/BrnBootLegal.cpp')
branch = definition(source, 'case E_STAGE_ATTRACT_PREPARE:')
branch = branch[branch.index('{'):]
result = compile_and_run(Path(__file__).with_name('AudioAttractStartup.cpp'),
                         'audio_attract_startup.inc', 'void EmitAttract() ' + branch,
                         'AudioAttractStartup')
raise SystemExit(0 if result and result[1] == 0 else 1)
