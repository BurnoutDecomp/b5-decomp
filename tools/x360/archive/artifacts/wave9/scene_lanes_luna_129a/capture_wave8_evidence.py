from pathlib import Path
import hashlib
import json
import subprocess

repo = Path(r'D:\Reverse\BP-X360-Experimental-Worktrees\scene_lanes_luna_129a')
base = '129afef95856e383366a55fb8f30a66ceb74de42'
report_path = Path(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave8\repair-r7\full-recovered\compile-results.json')
report = json.loads(report_path.read_text(encoding='utf-8'))
sources = [
    'src/GameShared/GameClasses/SceneManager/CgsEntityManager.cpp',
    'src/GameShared/GameClasses/SceneManager/CgsSceneManagerBridgeFunctions.cpp',
    'src/GameShared/GameClasses/SceneManager/CgsSceneManagerContact.cpp',
    'src/GameShared/GameClasses/Network/Debug Components/CgsNetworkPlayerManagerDebugComponent.cpp',
    'src/GameShared/GameClasses/Graphics/CgsMaterialAssembly.cpp',
]
sha_bytes = lambda b: hashlib.sha256(b).hexdigest()
rows = []
for source in sources:
    row = next(row for row in report['results'] if row['source'] == source)
    log_path = Path(row['log'])
    log_bytes = log_path.read_bytes()
    log = log_bytes.decode('utf-8', errors='replace')
    errors = [line for line in log.splitlines() if 'error ' in line]
    base_bytes = subprocess.check_output(['git', 'show', f'{base}:{source}'], cwd=repo)
    rows.append({
        'source': source,
        'baseline_blob': subprocess.check_output(['git', 'rev-parse', f'{base}:{source}'], cwd=repo, text=True).strip(),
        'baseline_sha256': sha_bytes(base_bytes),
        'reported_passed': row['passed'],
        'reported_returncode': row.get('returncode'),
        'diagnostics_count': len(errors),
        'diagnostics': errors,
        'log': str(log_path),
        'log_sha256': sha_bytes(log_bytes),
    })
assert report['provenance']['head'] == base
assert [r['diagnostics_count'] for r in rows] == [12, 24, 8, 3, 3]
proof = {
    'source': str(report_path),
    'report_sha256': hashlib.sha256(report_path.read_bytes()).hexdigest(),
    'reported_head': report['provenance']['head'],
    'stage': report['stage'],
    'target': report['target'],
    'rows': rows,
}
out = Path(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave9\scene_lanes_luna_129a\wave8-negative-evidence.json')
out.write_text(json.dumps(proof, indent=2) + '\n', encoding='utf-8')
print('report_sha256', proof['report_sha256'])
for row in rows:
    print(row['diagnostics_count'], row['source'], row['log_sha256'])
print('negative_evidence_sha256', hashlib.sha256(out.read_bytes()).hexdigest())
