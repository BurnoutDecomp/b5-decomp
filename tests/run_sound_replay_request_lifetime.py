"""Exercise native replay slots in real sound IO headers and accessor bodies.

--rev compiles the same fixture against that revision's headers and accessors.
The adjacent-allocation guard detects the original 32-bit storage on x64.
"""
import argparse
import os
import re
from pathlib import Path
from fxgs_common import REPO, STRSTREAM_CPP, Tree, compile_and_run, definition

os.environ.pop('NoDefaultCurrentDirectoryInExePath', None)
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--rev')
args = parser.parse_args()
tree = Tree(args.rev)
root = 'src/GameSource/Sound/Module/BrnRootSoundModule'
logic = 'src/GameSource/Sound/Module/LogicModule/BrnSoundLogicModuleIo'
root_source = tree.read(root + 'IO.cpp')
logic_source = tree.read(logic + '.cpp')
bodies = []
for owner, source in [('RootOutputBuffer', root_source), ('LogicOutputBuffer', logic_source)]:
    for const in (True, False):
        pattern = (r'^' + ('const ' if const else '')
                   + r'RootOutputBuffer::ReplayRequestInterface\*\s+'
                   + owner + r'::GetReplayRequestInterface\(\)' + (' const' if const else ''))
        match = re.search(pattern, source, re.M)
        if match is None:
            raise SystemExit(f'Missing production accessor: {owner}, const={const}')
        bodies.append(definition(source, '\n' + match.group()))
replay = definition(tree.read('src/GameSource/Replays/BrnReplayRequestInterface.cpp'),
                    'void RequestInterface::Append(')
text = ('namespace BrnSound { namespace Module { namespace Io {\n'
        + '\n'.join(bodies) + '\n} } }\nnamespace BrnReplays { namespace ReplayIO {\n'
        + replay + '\n} }\n')
shadow = {relative: tree.read(relative) for relative in (root + 'Io.h', logic + '.h')} if args.rev else None
numeric = compile_and_run(Path(__file__).with_name('SoundReplayRequestLifetime.cpp'),
                          'sound_replay_request_lifetime.inc', text, 'SoundReplayRequestLifetime',
                          shadow=shadow, extra_sources=[
                              REPO / 'src/GameShared/GameClasses/Module/CgsIOBuffer.cpp', STRSTREAM_CPP])
if numeric is None:
    raise SystemExit(1)
checks, failures = numeric
print(f'run_sound_replay_request_lifetime: {checks-failures}/{checks} pass ({failures} fail)')
raise SystemExit(bool(failures))
