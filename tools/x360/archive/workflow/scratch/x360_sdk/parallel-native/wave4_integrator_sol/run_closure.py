"""Run the unchanged native build driver in-process to avoid Windows argv limits."""
import importlib.util
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys
from datetime import datetime, timezone

PARENT = Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow')
HERE = Path(__file__).resolve().parent
WORK = Path('C:/Users/adri1/.codex/worktrees/bp360/wave4-intake')
ARTIFACT = PARENT / 'b5-decomp'
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--batch',default='native-parallel-wave4-final')
parser.add_argument('--output',default='native-parallel-wave4-repair-closure')
parser.add_argument('--tag',default='closure')
args = parser.parse_args()
meta = json.loads((PARENT / ('scratch/x360_sdk/'+args.batch+'-patch.json')).read_text())
raw = json.loads((ARTIFACT / 'build/x360/full-native-parallel-wave4-stage1/compile-results.json').read_text())
assert subprocess.check_output(['git','-C',str(WORK),'rev-parse','HEAD'],text=True).strip() == meta['head']
assert not subprocess.check_output(['git','-C',str(WORK),'status','--porcelain=v1']).strip()
out = ARTIFACT / ('build/x360/'+args.output)
assert not out.exists(), 'Never overwrite a retained report/artifact directory'
private = HERE / ('private-temp-'+args.tag)
private.mkdir(exist_ok=True)
os.environ['TEMP'] = os.environ['TMP'] = str(private)
extra = [arg for arg in raw['results'][0]['command']
         if arg.replace('\\','/').lower().startswith('/i' + str(ARTIFACT).replace('\\','/').lower() + '/vendor/')]
driver = WORK / 'tools/build_x360.py'
argv = [str(driver), '--sdk',raw['sdk'],'--out',str(out),'--jobs','8','--timeout','180','--quiet']
for arg in extra:
    argv.append('--compiler-arg=' + arg)
for source in meta['affected_sources']:
    argv.extend(['--source',source])
with (HERE / (args.tag+'-command.json')).open('x',encoding='utf-8') as stream:
    json.dump({'head':meta['head'],'patch_sha256':meta['sha256'],'argv':argv,
               'private_temp':str(private),'started_utc':datetime.now(timezone.utc).isoformat()},stream,indent=2)
spec = importlib.util.spec_from_file_location('wave4_native_driver',driver)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
sys.argv = argv
code = module.main()
with (HERE / (args.tag+'-exit.json')).open('x',encoding='utf-8') as stream:
    json.dump({'head':meta['head'],'exit_code':code,'report':str(out/'compile-results.json'),
               'completed_utc':datetime.now(timezone.utc).isoformat()},stream,indent=2)
raise SystemExit(code)
