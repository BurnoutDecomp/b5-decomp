from pathlib import Path
import hashlib, json, os, subprocess, sys

HERE = Path(__file__).resolve().parent
config = json.loads((HERE / 'command.json').read_text())
ROOT = Path(config['cwd'])

def sha(p):
    return hashlib.sha256(Path(p).read_bytes()).hexdigest()

paths = list(json.loads((HERE / 'source-before.json').read_text())['hashes'])
def snapshot():
    return {p: sha(ROOT / p) for p in paths}

before = snapshot()
(HERE / 'source-final-before.json').write_text(json.dumps({'hashes': before}, indent=2))
args = config['argv'][:]
args[args.index('--out')+1] = str(HERE / 'native-final')
args.extend(['--source', 'src/SDKs/RenderEngineClub/MAIN/components/src/postfx/src/rwgpfxtint.cpp',
             '--source', 'src/GameSource/Graphics/PostFx/BrnPostFx.cpp'])
(HERE / 'final-command.json').write_text(json.dumps({'argv': args, 'cwd': str(ROOT),
    'private_temp': config['private_temp'], 'driver_sha256': sha(args[0])}, indent=2))
env = os.environ.copy()
env['TEMP'] = config['private_temp']
env['TMP'] = config['private_temp']
result = subprocess.run([sys.executable, '-X', 'utf8', '-B', *args], cwd=ROOT, env=env,
                        stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
(HERE / 'final-console.log').write_bytes(result.stdout)
(HERE / 'final-exit.json').write_text(json.dumps({'returncode': result.returncode}, indent=2))
after = snapshot()
(HERE / 'source-final-after.json').write_text(json.dumps({'hashes': after}, indent=2))
assert before == after, 'source drift'
print(result.stdout.decode(errors='replace'))
print('Final source snapshot stable:', len(before), 'inputs; compiler return:', result.returncode)
