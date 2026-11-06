import json,hashlib,sys
from pathlib import Path
inv=Path(r'D:/Reverse/BP-X360-Experimental-Artifacts/wave6/current-failure-triage-luna/triage_inventory.json')
md=inv.parent/'TRIAGE.md'
d=json.loads(inv.read_text(encoding='utf-8'))
r=Path(d['current_report']['path'])
assert hashlib.sha256(r.read_bytes()).hexdigest()==d['current_report']['sha256']
assert (d['current_report']['requested'],d['current_report']['passed'],d['current_report']['failed'])==(3613,3371,242)
assert d['primary_diagnostic_partition']['sum']==242
assert sum(x['failed_tu_count'] for x in d['primary_diagnostic_partition']['buckets'])==242
assert len(d['regressions'])==2 and len(d['true_layout_gates'])==3
assert all(Path(x['log']).is_file() for x in d['unowned_cpp_only_opportunities'])
assert all(Path(x['log']).is_file() for x in d['regressions'].values())
s=md.read_text(encoding='utf-8')
assert '{opportunities' not in s and "{w6/" not in s
print('PASS: report hash/counts, 242-row bucket sum, regressions/gates, opportunity and regression log paths, note placeholders')
print('inventory_bytes',inv.stat().st_size,'note_bytes',md.stat().st_size)
