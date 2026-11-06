from pathlib import Path
import hashlib, json, os, subprocess, sys

ROOT = Path('D:/Reverse/BP-X360-Experimental-Worktrees/drive-thru-native-root')
HERE = Path(__file__).resolve().parent
PREVIOUS = HERE.parent / 'native-formatter-root'
SOURCES = [
    'src/GameShared/Jobs/Relocator/Relocator.cpp',
    'src/GameShared/Jobs/RadixSort/RadixSort.cpp',
    'src/GameShared/Jobs/ContactGenerator/ContactGenerator.cpp',
    'src/GameShared/Jobs/PolygonSoupTester/PolygonSoupTester.cpp',
    'src/SDKs/EATech/eajobs/job_thread_id.cpp',
    'src/GameShared/Jobs/Relocator/RelocatorJob.cpp',
    'src/GameShared/Jobs/Relocator/CgsRelocator.cpp',
    'src/GameShared/Jobs/PolygonSoupTester/PolygonSoupTesterJob.cpp',
    'src/GameShared/Jobs/ContactGenerator/ContactGeneratorJob.cpp',
    'src/GameShared/Jobs/RadixSort/RadixSortJob.cpp',
    'src/GameShared/Jobs/DecompressionJob/CgsDecompressor.cpp',
    'src/GameShared/GameClasses/Memory/DataStream/CgsDataStreamCommandReader.cpp',
    'src/GameShared/GameClasses/Memory/DataStream/CgsDataStreamResultPoster.cpp',
    'src/GameShared/GameClasses/Memory/DataStream/CgsSimpleDataStreamConsumer.cpp',
    'src/GameShared/GameClasses/Development/CgsStrStream.cpp',
    'src/SDKs/EATech/eathread/BrnEAThreadX360.cpp',
]

def sha(p):
    return hashlib.sha256(Path(p).read_bytes()).hexdigest()

def write(name, value):
    with (HERE / name).open('x', encoding='utf-8') as f:
        json.dump(value, f, indent=2)

names = subprocess.check_output(['git', '-C', str(ROOT), 'ls-files', '-z']).decode().split('\0')
names = [name for name in names if name.startswith(('src/', 'vendor/'))
         and Path(name).suffix.lower() in ('.cpp', '.c', '.h', '.hpp', '.inl')
         and (ROOT / name).is_file()]
hashes = {name: sha(ROOT / name) for name in names}
assert len(hashes) >= 6281
head = subprocess.check_output(['git', '-C', str(ROOT), 'rev-parse', 'HEAD'], text=True).strip()
write('source-before.json', {'base': head, 'hashes': hashes})
old = json.loads((PREVIOUS / 'command.json').read_text())
args = old['argv'][:]
args[args.index('--out') + 1] = str(HERE / 'native')
args[args.index('--jobs') + 1] = '4'
i = args.index('--source')
del args[i:i + 2]
for name in SOURCES:
    args += ['--source', name]
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
