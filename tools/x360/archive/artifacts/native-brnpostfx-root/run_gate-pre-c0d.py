from pathlib import Path
import hashlib, json, os, subprocess, sys

HERE = Path(__file__).resolve().parent
ROOT = Path('D:/Reverse/BP-X360-Experimental-Worktrees/drive-thru-native-root')
stage = sys.argv[1] if len(sys.argv) > 1 else 'native'
prefix = stage + '-' if stage != 'native' else ''
assert not (HERE / stage / 'compile-results.json').exists(), 'preserve prior compiler report'
config = json.loads(Path('D:/Reverse/BP-X360-Experimental-Artifacts/native-tintblend-root/command.json').read_text())
keys = list(json.loads(Path('D:/Reverse/BP-X360-Experimental-Artifacts/native-tintblend-root/source-final-after.json').read_text())['hashes'])

def snapshot():
    return {p: hashlib.sha256((ROOT / p).read_bytes()).hexdigest() for p in keys}

before = snapshot()
(HERE / (prefix + 'source-before.json')).write_text(json.dumps({'hashes': before}, indent=2))
args = config['argv'][:]
args[args.index('--out')+1] = str(HERE / stage)
args[args.index('--source')+1] = 'src/GameSource/Graphics/PostFx/BrnPostFx.cpp'
for source in [
    'src/SDKs/RenderEngineClub/MAIN/components/src/postfx/src/rwgpfxcolourcube.cpp',
    'src/SDKs/RenderEngineClub/MAIN/components/src/postfx/src/rwgpfxtint.cpp',
    'src/GameShared/Jobs/TintBlend/TintBlend.cpp',
    'src/GameShared/GameClasses/RenderWare/CgsRwColourCubeResourceType.cpp',
    'src/GameShared/GameClasses/System/Resource/CgsResourcePtr_rw_graphics_postfx_ColourCube.cpp',
    'src/GameSource/Gui/PFX/BrnPfxHookBlender_Fader.cpp',
    'src/GameSource/Gui/PFX/BrnPfxHookBlender.cpp',
    'src/GameSource/Graphics/BrnEffectsArbitrator.cpp',
    'src/GameSource/World/EnvironmentManager/BrnEnvironmentManager.cpp',
    'src/SDKs/RenderEngineClub/MAIN/components/src/postfx/src/rwgpfxrendertarget.cpp',
    'src/GameShared/GameClasses/Graphics/CgsRenderTarget.cpp',
]:
    args.extend(['--source', source])
(HERE / (prefix + 'command.json')).write_text(json.dumps({'argv': args, 'cwd': str(ROOT), 'private_temp': config['private_temp']}, indent=2))
env = os.environ.copy()
env['TEMP'] = config['private_temp']
env['TMP'] = config['private_temp']
result = subprocess.run([sys.executable, '-X', 'utf8', '-B', *args], cwd=ROOT, env=env,
                        stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
(HERE / (prefix + 'console.log')).write_bytes(result.stdout)
(HERE / (prefix + 'exit.json')).write_text(json.dumps({'returncode': result.returncode}, indent=2))
after = snapshot()
(HERE / (prefix + 'source-after.json')).write_text(json.dumps({'hashes': after}, indent=2))
assert before == after
print(result.stdout.decode(errors='replace'))
print('stable sources', len(before), 'compiler return', result.returncode)
