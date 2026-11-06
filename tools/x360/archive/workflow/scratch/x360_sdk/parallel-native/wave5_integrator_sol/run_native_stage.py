"""Freeze one Wave5 stage and run actual native selected/consumer compilation."""
import argparse
from collections import defaultdict, deque
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import sys
from datetime import datetime, timezone

HERE = Path(__file__).resolve().parent
PARENT = HERE.parents[3]
SCRATCH = PARENT/'scratch/x360_sdk'
CONFIG = json.loads((HERE/'campaign.json').read_text(encoding='utf-8'))
WORK = Path(CONFIG['workspace'])
ARTIFACT = Path(CONFIG['artifact_root'])
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--name',required=True)
parser.add_argument('--mode',choices=('selected','closure','full'),default='selected')
parser.add_argument('--since-head')
parser.add_argument('--out',type=Path)
args = parser.parse_args()


def git(*options):
    return subprocess.check_output(['git','-C',str(WORK),*options])


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def save_new(path, value):
    with path.open('x',encoding='utf-8') as stream:
        json.dump(value,stream,indent=2)


assert not git('status','--porcelain=v1').strip()
head = git('rev-parse','HEAD').decode().strip()
paths = git('diff','--name-only',CONFIG['base']).decode().splitlines()
patch = git('diff','--binary',CONFIG['base'])
tracked = git('ls-files','src','vendor').decode().splitlines()
native = {p for p in tracked if p.startswith(('src/','vendor/'))
          and Path(p).suffix.lower() in('.cpp','.c')
          and 'pc' not in [x.lower() for x in Path(p).parts]}
snapshot = None
if args.mode == 'selected':
    changed = git('diff','--name-only',args.since_head or CONFIG['base']).decode().splitlines()
    sources = sorted(native & set(changed))
else:
    spec = importlib.util.spec_from_file_location('wave4_readonly_evidence',
        SCRATCH/'parallel-native/wave4_integrator_sol/wave4_evidence.py')
    common = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(common)
    snapshot = common.source_snapshot()
    prior = json.loads(Path(CONFIG['baseline_metadata']).read_text(encoding='utf-8'))['source_snapshot']
    changed = {p for p in set(prior['hashes'])|set(snapshot['hashes'])
               if prior['hashes'].get(p) != snapshot['hashes'].get(p)}
    reverse = defaultdict(set)
    for graph in (prior['dependencies'],snapshot['dependencies']):
        for parent,children in graph.items():
            for child in children:
                reverse[child].add(parent)
    closure = set(changed)
    queue = deque(changed)
    while queue:
        for parent in reverse[queue.popleft()]:
            if parent not in closure:
                closure.add(parent)
                queue.append(parent)
    sources = sorted(native if args.mode=='full' else native & closure)
assert sources
batch = 'native-parallel-wave5-'+args.name
out = args.out.resolve() if args.out else ARTIFACT/('build/x360/'+batch)
assert not out.exists(), 'Immutable output must not be overwritten'
meta = {'base':CONFIG['base'],'head':head,'workspace':str(WORK),'sha256':hashlib.sha256(patch).hexdigest(),
        'paths':paths,'source_sha256':{p:sha(WORK/p) for p in paths if (WORK/p).is_file()},
        'selected_sources':sources,'source_snapshot':snapshot,'selection':args.mode,
        'native_inventory':sorted(native),'created_utc':datetime.now(timezone.utc).isoformat()}
save_new(SCRATCH/(batch+'-patch.json'),meta)
with (SCRATCH/(batch+'.diff')).open('xb') as stream:
    stream.write(patch)
baseline = json.loads(Path(CONFIG['baseline_report']).read_text(encoding='utf-8'))
extra = [p for p in baseline['results'][0]['command']
         if p.replace('\\','/').lower().startswith('/i'+str(ARTIFACT).replace('\\','/').lower()+'/vendor/')]
temp = (out.parent/(out.name+'-compiler-temp')) if args.out else HERE/(args.name+'-temp')
temp.mkdir(parents=True,exist_ok=True)
os.environ['TEMP']=os.environ['TMP']=str(temp)
driver = WORK/'tools/build_x360.py'
argv = [str(driver),'--sdk',baseline['sdk'],'--out',str(out),'--jobs','8','--timeout','180','--quiet']
argv += ['--compiler-arg='+p for p in extra]
for source in sources:
    argv += ['--source',source]
save_new(HERE/(args.name+'-command.json'),{'head':head,'argv':argv,'private_temp':str(temp)})
owner_path = SCRATCH/'parallel-native/INTEGRATION_OWNER.json'
owner = json.loads(owner_path.read_text(encoding='utf-8'))
owner.update(phase='wave5_'+args.name+'_native_build',frozen_head=head,live_session=None,
             live_process_id=os.getpid(),live_report=str(out/'compile-results.json'),
             instruction='Source frozen during this actual native build. No root checkout mutation until terminal; Wave4 evidence immutable.')
owner_path.write_text(json.dumps(owner,indent=2)+'\n',encoding='utf-8')
print('FROZEN',head,'mode',args.mode,'sources',len(sources),'paths',len(paths),'PID',os.getpid(),flush=True)
spec = importlib.util.spec_from_file_location('wave5_native_driver',driver)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
sys.argv=argv
result = module.main()
save_new(HERE/(args.name+'-exit.json'),{'head':head,'exit_code':result,
                                     'report':str(out/'compile-results.json')})
raise SystemExit(result)
