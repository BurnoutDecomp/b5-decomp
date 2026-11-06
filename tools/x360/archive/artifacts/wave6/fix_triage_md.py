import json
from pathlib import Path
p=Path(r'D:/Reverse/BP-X360-Experimental-Artifacts/wave6/current-failure-triage-luna/triage_inventory.json')
d=json.loads(p.read_text(encoding='utf-8'))
md=p.parent/'TRIAGE.md'; s=md.read_text(encoding='utf-8')
repls={
 "{opportunities[0]['log']}":d['unowned_cpp_only_opportunities'][0]['log'],
 "{opportunities[1]['log']}":d['unowned_cpp_only_opportunities'][1]['log'],
 "{w6/'triage_summary.json'}":d['raw_diagnostics_artifact'],
 "{w6/'triage_inventory.json'}":str(p),
 'Five additional `sizeof` diagnostics are on unknown/incomplete types; they are not evidence that the layout values themselves are wrong.':'Five TUs carry six `sizeof` errors on unknown/incomplete types; additionally, `VolumeQuery.cpp` has two unknown-size pointer-arithmetic errors on the incomplete result type. None is evidence by itself that a measured layout value is wrong.',
 'RenderEngine provider headers are already active-owner territory; do not duplicate those repairs.':'These provider families overlap current RenderCore provider ownership; confirm with that owner before routing any consumer change.'
}
for a,b in repls.items():
 if a not in s: print('MISSING_TOKEN',a)
 s=s.replace(a,b)
md.write_text(s,encoding='utf-8')
print('updated',md)
