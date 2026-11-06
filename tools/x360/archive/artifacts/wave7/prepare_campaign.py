"""Pin the explicitly authorized new Wave7 cutoff without touching source or refs."""
import hashlib
import json
from pathlib import Path
import subprocess
from datetime import datetime, timezone

HERE = Path(__file__).resolve().parent
WORK = Path('C:/Users/adri1/.codex/worktrees/bp360/wave4-intake')
PARENT = Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow')
BASE = '9388e2a58178bd4403a2b34353105b608a8feb08'


def git(*args):
    return subprocess.check_output(['git', '--no-optional-locks', *args], cwd=WORK).decode().strip()


def read(path):
    return json.loads(Path(path).read_text(encoding='utf-8-sig'))


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def save(name, value):
    with (HERE / name).open('x', encoding='utf-8') as stream:
        json.dump(value, stream, indent=2)


assert git('rev-parse', 'HEAD') == BASE and not git('status', '--porcelain=v1')
gui_path = HERE.parent / 'gui-526-combined-final/HANDOFF.json'
gui = read(gui_path)
assert len(gui['ordered_commits']) == 6
groups = {
    'root_limits': ['22758494dfdb2ba0d8f46c013e7c36d136fa6921', '89ff029c24c991d199dd3e5392b7663676c31ceb', '3a0702f25723d94059fa7cd542d2c9fff69566e1'],
    'scalar_fma': ['c21c82b203466f67b39f2f1e8f3cd47204ea5fb9', 'a76269d542b8e7fa9c5efc63124b6f093d4bd352', 'cf56bb9767037fe2bc3cd5e7fdea6b10ee288288', '0f247eb66e7fe5991b5fc282c87c85856d916fae', 'cbf7571b8ba2966f0943fc198c8a3fed937e9ec3'],
    'static_callback': ['eaf285897b9601571b5a02d2b2502aa12cc7765a'],
    'audio_fma': ['d1ea71ca0811777554b60a80a185f01c5505f80d', 'dc4e372b74f5a0e832050bd2a58d17e19c6aebd0', 'f01e1a5e49d369b26ce5b35d5e3d4bb34a670be5'],
    'ai_accessors': ['1c6d90e1553f53c17aa25c05da20810d07236451'],
    'sound_yaw': ['6dc1583dbdf42202e4d1f1a9ac836899ce4187a4'],
    'boot': ['021de1a12bcbaf26138b3c380e765b6fa3a37b01', '3e2e8d6e6d00740e33d9b09d76b1df3bfb4ff635'],
    'factory_vertex': ['a4491a2a22f8a8e19053b924f63234413bdf7f0d', '790d9599074979c76a68a1f5030935567ddbe319'],
    'gui': gui['ordered_commits'] + ['2c5e48a6d7a7922dd4d5d78c1c075b8ce2fbfda1', 'cf6868a0dbff45972f4f82d1b511ac7efa3495db'],
    'early_director': ['be1641cea515bb330129888d271947ef6c4a6b9c', 'fe9ae66eebf3e10e6552df2499cf5472fc29703f'],
    'early_loose': ['1e9481630da05477f8d72a3967cbc1bb4d4d543d', '0b13f762947f6155e84e9b729d620f5473e4a29d']}
commits = [value for group in groups.values() for value in group]
assert len(commits) == len(set(commits)) == 30
rows, patches = [], []
all_paths = set()
for group, values in groups.items():
    for commit in values:
        assert git('cat-file', '-t', commit) == 'commit'
        paths = git('diff-tree', '--no-commit-id', '--name-only', '-r', commit).splitlines()
        assert paths and all(p.startswith(('src/', 'vendor/')) and 'pc' not in Path(p).parts for p in paths)
        all_paths.update(paths)
        rows.append({'group': group, 'commit': commit, 'parent': git('rev-parse', commit + '^'),
                     'paths': paths, 'message': git('show', '-s', '--format=%B', commit),
                     'numstat': git('diff-tree', '--no-commit-id', '--numstat', '-r', commit)})
        patches.append(git('show', '--format=full', '--no-ext-diff', '--binary', commit))
manifest_paths = [
    HERE.parent / 'native-limit-tables-root/DELIVERY.json',
    HERE.parent / 'drive-thru-native-root/DELIVERY.json',
    HERE.parent / 'scoring-float-limits-root/DELIVERY.json',
    HERE.parent / 'wave6/fma_native_luna/delivery.json',
    HERE.parent / 'wave6/fma_native_luna/cpp_scalar_fma_validation.json',
    Path('D:/Reverse/BP-Decomp_Workflow/scratch/parallel-native/scoring_static_callback_luna/DELIVERY.json'),
    HERE.parent / 'wave6/audio_fma_luna/delivery.json',
    HERE.parent / 'parallel/ai_accessors_terra/delivery.json',
    PARENT / 'scratch/parallel-native/sound_layout_native/YAW_NATIVE_DELIVERY.md',
    HERE.parent / 'boot-callers-526adc/DELIVERY.json',
    HERE.parent / 'job-provider-526adc/DELIVERY.json',
    gui_path, HERE.parent / 'gui-consumers-closure-final/HANDOFF.json']
assert sha(manifest_paths[0]) == '958202c72ff81ece9077f3409c24bd37ddad4e5afbb9aca7bcba41e1716e22b2'
baseline_meta = HERE.parent / 'wave6/native-parallel-wave6-final2-patch.json'
baseline_report = HERE.parent / 'wave6/native-parallel-wave6-final2-effective.json'
baseline_manifest = HERE.parent / 'wave6/native-parallel-wave6-final2-artifact-manifest.json'
plan = {'base': BASE, 'workspace': str(WORK), 'collect_now': commits, 'groups': groups,
        'candidates': rows, 'candidate_paths': sorted(all_paths),
        'manifest_bindings': {str(p): sha(p) for p in manifest_paths},
        'expected_count_note': 'Root enumerated set resolves to30 unique originals, not31; no extra node inferred.',
        'held': ['all later Director/Replay/CTOR/ICE', 'all later Frustum worker/data/callback',
                 'private native V3/V4/VecFloat/Affine/V2', 'AboveCar/bf3/RaceCar/Curves/Corona/model-private',
                 'GPU storagefa5/MSAA/uncommittedSun', 'all other later registry candidates'],
        'scope': 'Explicit closed cutoff only; no private foreign ancestry or W6 queues replayed.',
        'created_utc': datetime.now(timezone.utc).isoformat()}
save('intake-plan-01.json', plan)
with (HERE / 'preflight-candidate-patches.txt').open('x', encoding='utf-8') as stream:
    stream.write('\n\n'.join(patches))
save('campaign.json', {'campaign': 'wave7', 'base': BASE, 'workspace': str(WORK),
    'canonical': str(PARENT / 'b5-decomp'), 'artifact_root': str(HERE),
    'baseline_metadata': str(baseline_meta), 'baseline_metadata_sha256': sha(baseline_meta),
    'baseline_report': str(baseline_report), 'baseline_report_sha256': sha(baseline_report),
    'baseline_artifact_manifest': str(baseline_manifest), 'baseline_artifact_manifest_sha256': sha(baseline_manifest),
    'baseline_passed': 3374, 'baseline_failed': 239, 'baseline_requested': 3613,
    'current_plan': str(HERE / 'intake-plan-01.json')})
save('wave7-receipts.json', {'workspace': str(WORK), 'base': BASE, 'head': BASE,
    'state': 'complete', 'queue': [], 'todo': [], 'receipts': []})
print('Preflight pinned', len(commits), 'new commits /', len(all_paths), 'candidate paths. No source changed.')
for row in rows:
    print(row['group'], row['commit'][:10], ', '.join(row['paths']))
