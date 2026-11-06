from pathlib import Path
import hashlib, json, os, subprocess, sys

ROOT = Path('D:/Reverse/BP-X360-Experimental-Worktrees/drive-thru-native-root')
HERE = Path(__file__).resolve().parent
PREVIOUS = HERE.parent / 'native-particle-description-root'

def sha(p):
    return hashlib.sha256(Path(p).read_bytes()).hexdigest()

def write(name, value):
    with (HERE / name).open('x', encoding='utf-8') as f:
        json.dump(value, f, indent=2)

names = json.loads((PREVIOUS / 'source-after.json').read_text())['hashes']
hashes = {name: sha(ROOT / name) for name in names}
assert len(hashes) == 6277
head = subprocess.check_output(['git', '-C', str(ROOT), 'rev-parse', 'HEAD'], text=True).strip()
write('source-before.json', {'base': head, 'hashes': hashes})
old = json.loads((PREVIOUS / 'command.json').read_text())
args = old['argv'][:]
args[args.index('--out') + 1] = str(HERE / 'native')
args[args.index('--source') + 1] = 'vendor/renderware/src/rw/OSGlobalManager.cpp'
args += ['--source', 'vendor/renderware/src/rw/core/debug/Manager.cpp', '--source', 'vendor/renderware/src/rw/core/debug/Formatter.cpp', '--source', 'vendor/renderware/src/rw/BaseResourceDescriptor.cpp']
env = os.environ.copy()
temp = HERE / 'temp'
temp.mkdir(exist_ok=True)
env['TEMP'] = env['TMP'] = str(temp)
write('command.json', {'argv': args, 'cwd': str(ROOT), 'private_temp': str(temp),
                       'driver_sha256': sha(ROOT / 'tools/build_x360.py')})
run = subprocess.run([sys.executable, '-X', 'utf8', '-B'] + args, cwd=ROOT, env=env,
                     stdin=subprocess.DEVNULL, creationflags=subprocess.CREATE_NO_WINDOW)
write('exit.json', {'returncode': run.returncode})
after = {name: sha(ROOT / name) for name in names}
write('source-after.json', {'base': head, 'hashes': after})
assert hashes == after, 'source inputs changed during compilation'
print('Stable complete private source inputs:', len(hashes))
raise SystemExit(run.returncode)
