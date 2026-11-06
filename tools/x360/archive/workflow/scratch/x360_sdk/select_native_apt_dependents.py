"""Select a conservative native compile check for the changed Apt headers."""
import importlib.util
import argparse
import json
import os
from pathlib import Path
import re
import subprocess
from collections import defaultdict, deque

ROOT = Path(__file__).resolve().parents[2] / 'b5-decomp'
SCRATCH = Path(__file__).parent
parser=argparse.ArgumentParser()
parser.add_argument('--batch',default='native-apt-record')
parser.add_argument('--baseline',default='build/x360/full-native-network-apt/compile-results.json')
parser.add_argument('--no-apt-expansion',action='store_true',help='Use just the include closure for a batch outside Apt')
args=parser.parse_args()
meta = json.loads((SCRATCH/(args.batch+'-patch.json')).read_text())
baseline = json.loads((ROOT/args.baseline).read_text())
assert baseline['state'] == 'complete'
added_paths=set(subprocess.check_output(['git','diff','--name-only','--diff-filter=A',meta['base']],cwd=ROOT,text=True).splitlines())
native = {r['source'] for r in baseline['results']}
names = subprocess.check_output(['git','ls-files','--recurse-submodules'], cwd=ROOT).decode().splitlines()
names = [n for n in names if Path(n).suffix.lower() in
         ('.h','.hpp','.hh','.hxx','.inl','.inc','.ipp','.cpp','.c','.cxx')]
known = {os.path.normcase(os.path.abspath(ROOT/n)):n for n in names if (ROOT/n).is_file()}
spec = importlib.util.spec_from_file_location('native_build', ROOT/'tools/build_x360.py')
build = importlib.util.module_from_spec(spec);spec.loader.exec_module(build)
includes = [ROOT/n for n in build.INCLUDES]
reverse = defaultdict(set)
macro = []
pattern = re.compile(r'^\s*#\s*include\s*[<"]([^">]+)[">]', re.M)
for source in known.values():
    content = (ROOT/source).read_text(encoding='utf-8', errors='replace')
    contents = [content]
    if source in meta['paths']:
        if source not in added_paths:
            contents.append(subprocess.check_output(['git','show',meta['base']+':'+source],cwd=ROOT).decode('utf-8'))
    for body in contents:
        for target in pattern.findall(body):
            for directory in [(ROOT/source).parent]+includes:
                key = os.path.normcase(os.path.abspath(directory/target))
                if key in known:
                    reverse[known[key]].add(source)
                    break
        for line in body.splitlines():
            if re.match(r'^\s*#\s*include\s+[^<"\s]',line):
                macro.append({'source':source,'line':line.strip()})
# The build forces types.hpp into every translation unit.
reverse['src/types.hpp'].update(native)
reasons = defaultdict(set)
for changed in meta['paths']:
    visited, pending = set(), deque([changed])
    while pending:
        name = pending.popleft()
        if name in visited:continue
        visited.add(name)
        if name in native:reasons[name].add(changed)
        pending.extend(reverse.get(name, ()))
# Also compile every Apt runtime TU, including unchanged callers, independently
# of the include graph. All current Apt include directives are literal filenames.
for source in native:
    if not args.no_apt_expansion and (source.startswith(('src/SDKs/EATech/include/Apt/','src/SDKs/EATech/Apt/')) or source=='src/SDKs/EATech/AptGlobals.cpp'):
        reasons[source].add('whole Apt runtime')
apt_macros = [x for x in macro if x['source'].startswith(('src/SDKs/EATech/include/Apt/','src/SDKs/EATech/Apt/'))]
assert not apt_macros, apt_macros
selected = sorted(reasons)
report = {'base':meta['base'],'patch_sha256':meta['sha256'],
          'baseline':args.baseline,
          'selection':'reverse literal include closure over tracked files/submodules; union old/new includes on changed paths'+('' if args.no_apt_expansion else '; all Apt runtime TUs additionally included'),
          'sources':selected,'reasons':{s:sorted(reasons[s]) for s in selected},
          'macro_includes_outside_apt':macro,'apt_macro_includes':apt_macros}
(SCRATCH/(args.batch+'-selection.json')).write_text(json.dumps(report,indent=2)+'\n')
print('Selected',len(selected),'of',len(native),'native TUs; changed-header dependents'+('' if args.no_apt_expansion else ' plus all Apt runtime'))
print('Prior passes included:',sum(r['passed'] for r in baseline['results'] if r['source'] in reasons))
