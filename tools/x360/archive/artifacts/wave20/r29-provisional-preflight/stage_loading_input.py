"""Reproduce and bind the private generated X360 loading textures."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--worktree', type=Path, required=True)
parser.add_argument('--head', required=True)
parser.add_argument('--receipt', type=Path, required=True)
args = parser.parse_args()
worktree = args.worktree.resolve()
packet = Path(__file__).resolve().parent.parent / 'r27-loading-terra-relative/NATIVE_INTAKE_INPUT.json'

def sha(data):
    return hashlib.sha256(data).hexdigest()

def git(*argv):
    return subprocess.check_output(['git', '--no-optional-locks', *argv], cwd=worktree)

assert sha(packet.read_bytes()) == '5595446155f387ebc482046edd99c457c70afbff2dc13267637277ce189406cf'
binding = json.loads(packet.read_text())
assert git('rev-parse', 'HEAD').decode().strip() == args.head
assert not git('status', '--porcelain=v1').strip()
for path, entry in binding['source_git_objects'].items():
    assert sha(git('show', args.head + ':' + path)) == entry['sha256']
    raw = git('hash-object', '--no-filters', '--', path).decode().strip()
    filtered = git('hash-object', '--path=' + path, '--', path).decode().strip()
    assert entry['blob'] in (raw, filtered)

reader = Path(binding['reader']['path'])
assert sha(reader.read_bytes()) == binding['reader']['sha256']
generated = {path: (worktree / path).resolve() for path in binding['generated_paths']}
for path, resolved in generated.items():
    assert resolved.relative_to(worktree).as_posix() == path
    assert resolved.parent == worktree / 'build/x360/generated'
    assert git('check-ignore', '--', path).decode().strip() == path
presence = {path: p.exists() for path, p in generated.items()}
assert not any(presence.values()) or all(presence.values()), presence
command = [sys.executable, '-X', 'utf8', '-B', 'tools/extract_x360_loading_textures.py',
           *binding['generator_arguments']]
if not any(presence.values()):
    subprocess.run(command, cwd=worktree, check=True)
pins = {str(p): sha(p.read_bytes()) for p in generated.values()}
assert all(pins[str(p)] == binding['generated_paths'][rel]['sha256'] for rel,p in generated.items())
assert not git('status', '--porcelain=v1').strip()
with args.receipt.open('x', encoding='utf-8') as stream:
    json.dump({'head':args.head,'input_contract_sha256':sha(packet.read_bytes()),
               'generator_command':command,'generated':pins},stream,indent=2)
    stream.write('\n')
