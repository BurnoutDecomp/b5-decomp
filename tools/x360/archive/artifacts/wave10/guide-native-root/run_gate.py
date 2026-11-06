from pathlib import Path
import hashlib, importlib.util, json, os, subprocess, sys

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
WORK = Path('D:/Reverse/BP-X360-Experimental-Worktrees/external-body-native-root')
SOURCE = 'src/GameShared/GameClasses/Gui/CgsGuideIntegration.cpp'
BASE = 'e572f7db4c533546832e42c6f3f73149071b5f82'
CANON = Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/b5-decomp')
R11 = Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave8/repair-r11/full-stage1/compile-results.json')

def sha(p): return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def save(p, obj):
    with Path(p).open('x', encoding='utf-8', newline='\n') as f:
        json.dump(obj, f, indent=2)
        f.write('\n')

assert subprocess.check_output(['git', '-C', str(WORK), 'rev-parse', 'HEAD'], text=True).strip() == BASE
assert subprocess.check_output(['git', '-C', str(WORK), 'diff', '--name-only'], text=True).splitlines() == [SOURCE]
baseline = json.loads(R11.read_text())
row = next(x for x in baseline['results'] if x['source'] == SOURCE)
assert not row['passed']
extra = [x for x in row['command'] if x.replace('\\', '/').lower().startswith('/i' + CANON.as_posix().lower() + '/vendor/')]
assert len(extra) == 9
out = HERE / sys.argv[1]
temp = HERE / (sys.argv[1] + '-temp')
assert not out.exists() and not temp.exists()
temp.mkdir()
os.environ['TEMP'] = os.environ['TMP'] = str(temp)
before = sha(WORK / SOURCE)
args = [str(WORK / 'tools/build_x360.py'), '--sdk', baseline['sdk'], '--out', str(out), '--jobs', '1', '--timeout', '180', '--source', SOURCE, '--compiler-arg=/showIncludes']
args += ['--compiler-arg=' + x for x in extra]
save(HERE / (sys.argv[1] + '-command.json'), {'base': BASE, 'source': SOURCE, 'source_sha256': before, 'argv': args, 'driver_sha256': sha(args[0])})
spec = importlib.util.spec_from_file_location('guide_native_driver', args[0])
driver = importlib.util.module_from_spec(spec)
spec.loader.exec_module(driver)
sys.argv = args
code = driver.main()
report = json.loads((out / 'compile-results.json').read_text())
assert sha(WORK / SOURCE) == before
includes = {}
for result in report['results']:
    for line in Path(result['log']).read_text(errors='replace').splitlines():
        if 'including file:' in line:
            p = Path(line.split('including file:', 1)[1].strip()).resolve()
            includes[str(p)] = sha(p)
save(out / 'bindings.json', {'source_sha256': before, 'headers': includes, 'results': report['results'], 'report_sha256': sha(out / 'compile-results.json'), 'exit_code': code})
raise SystemExit(code)
