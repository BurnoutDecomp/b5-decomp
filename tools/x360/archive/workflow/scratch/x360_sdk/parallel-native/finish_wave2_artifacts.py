"""Compose frozen reports without altering raw compiler evidence."""
from pathlib import Path
import hashlib
import json
import struct
import subprocess

HERE = Path(__file__).resolve().parent
SCRATCH = HERE.parent
ROOT = HERE.parents[2] / 'b5-decomp'
meta_path = SCRATCH / 'native-parallel-wave2-fixed-patch.json'
metadata = json.loads(meta_path.read_text())
base_path = ROOT / 'build/x360/native-parallel-wave2-infra-effective.json'
fixed_path = ROOT / 'build/x360/parallel-wave2-fixed/compile-results.json'
base = json.loads(base_path.read_text())
fixed = json.loads(fixed_path.read_text())
assert base['state'] == fixed['state'] == 'complete'
assert fixed['passed'] == 4 and not fixed['failed']
rows = {r['source']: r for r in base['results']}
provenance = base['provenance'].copy()
for row in fixed['results']:
    assert row['source'] in rows and row['passed']
    assert row['returncode'] == 0 and row['machine'] == 0x1f2
    with Path(row['object']).open('rb') as stream:
        assert struct.unpack('<H', stream.read(2))[0] == 0x1f2
    assert Path(row['assembly']).is_file()
    rows[row['source']] = row
    provenance[row['source']] = str(fixed_path)
result = {k: v for k, v in base.items() if k not in ('results', 'passed', 'failed', 'provenance', 'source_snapshot')}
result.update({
    'results': [rows[r['source']] for r in base['results']],
    'passed': sum(r['passed'] for r in rows.values()),
    'failed': sum(not r['passed'] for r in rows.values()),
    'provenance': provenance,
    'source_snapshot': {'head': metadata['head'], 'patch_sha256': metadata['sha256']},
})
assert result['passed'] == 3043 and result['failed'] == 554
out = ROOT / 'build/x360/native-parallel-wave2-fixed-effective.json'
out.write_text(json.dumps(result, indent=2) + '\n')


def external_definitions(path):
    data = path.read_bytes()
    machine, _, _, pointer, count, _, _ = struct.unpack_from('<HHIIIHH', data)
    assert machine == 0x1f2
    strings_at = pointer + count * 18
    strings = data[strings_at:]
    names = set()
    index = 0
    while index < count:
        at = pointer + index * 18
        raw, value, section, typ, storage, auxiliaries = struct.unpack_from('<8sIhHBB', data, at)
        if raw[:4] == bytes(4):
            offset = struct.unpack_from('<I', raw, 4)[0]
            name = strings[offset:].split(bytes(1), 1)[0].decode('ascii')
        else:
            name = raw.split(bytes(1), 1)[0].decode('ascii')
        if storage == 2 and section > 0:
            names.add(name)
        index += 1 + auxiliaries
    return names


removed = 'src/SDKs/EATech/Apt/AptSpriteMembersIndex.cpp'
provider = 'src/SDKs/Packages/Apt/2.00.00/source/AptValue/AptSprite.cpp'
prior_path = ROOT / 'build/x360/full-native-parallel-wave1/compile-results.json'
prior = {r['source']: r for r in json.loads(prior_path.read_text())['results']}
assert prior[removed]['passed'] and rows[provider]['passed']
old_object, retained_object = Path(prior[removed]['object']), Path(rows[provider]['object'])
old_definitions = external_definitions(old_object)
retained_definitions = external_definitions(retained_object)
assert old_definitions
missing = sorted(old_definitions - retained_definitions)
assert not missing, missing
proof = {
    'removed_source': removed, 'provider': provider,
    'removed_object': str(old_object),
    'removed_object_sha256': hashlib.sha256(old_object.read_bytes()).hexdigest(),
    'retained_object': str(retained_object),
    'retained_object_sha256': hashlib.sha256(retained_object.read_bytes()).hexdigest(),
    'removed_external_definitions': sorted(old_definitions),
    'missing_external_definitions': missing,
    'scope': 'Every removed external COFF definition is supplied by the retained native CPP; source-token and one-provider evidence is separately reviewed.',
}
proof_path = HERE / 'wave2-removed-source-proof.json'
proof_path.write_text(json.dumps(proof, indent=2) + '\n')
metadata['removed_source_evidence'] = {removed: {
    'kind': 'duplicate-native-definitions', 'providers': [provider],
    'proof': 'parallel-native/' + proof_path.name,
    'proof_sha256': hashlib.sha256(proof_path.read_bytes()).hexdigest(),
}}
metadata['report'] = str(out.relative_to(ROOT)).replace('\\', '/')
meta_path.write_text(json.dumps(metadata, indent=2) + '\n')
print('Frozen native effective report:', result['passed'], '/', result['requested'])
print('Deleted duplicate:', len(old_definitions), 'external definitions retained in original native owner.')
