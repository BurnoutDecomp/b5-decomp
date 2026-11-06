"""Read actual terminal PPC COFF symbol tables for three repaired provider families."""
import hashlib
import json
from pathlib import Path
import struct

HERE = Path(__file__).resolve().parent
audit_path = HERE / 'stage1-terminal-audit.json'
audit = json.loads(audit_path.read_text(encoding='utf-8'))
report_path = Path(audit['report'])
assert hashlib.sha256(report_path.read_bytes()).hexdigest() == audit['report_sha256']
report = json.loads(report_path.read_text(encoding='utf-8'))
providers = {}
scanned = 0
for row in report['results']:
    if not row['passed']:
        continue
    path = Path(row['object'])
    with path.open('rb') as stream:
        machine, sections, _, symbols_at, count, optional, _ = struct.unpack('<HHIIIHH', stream.read(20))
        assert machine == 0x1F2 and optional == 0
        stream.seek(symbols_at)
        symbols = stream.read(count * 18)
        raw_size = stream.read(4)
        assert len(raw_size) == 4
        size = struct.unpack('<I', raw_size)[0]
        strings = raw_size + stream.read(size - 4)
    index = 0
    while index < count:
        name, value, section, typ, storage, auxiliary = struct.unpack_from('<8sIhHBB', symbols, index * 18)
        if name[:4] == b'\0\0\0\0':
            offset = struct.unpack_from('<I', name, 4)[0]
            name = strings[offset:strings.index(b'\0', offset)]
        else:
            name = name.rstrip(b'\0')
        if storage == 2 and section > 0:
            wanted = (name.startswith(b'?MemoryUsage@BrnRendererModule@') or
                      name.startswith(b'??0OverlapGenerationModule@CgsSceneManager@') or
                      name.startswith(b'?GetPoolInputQueue@InputBuffer@PoolIO@'))
            if wanted:
                text = name.decode('ascii')
                providers.setdefault(text, []).append({
                    'source': row['source'], 'object': str(path),
                    'object_sha256': audit['artifact_manifest'][row['source']]['object_sha256'],
                    'section': section, 'symbol_value': value,
                })
        index += 1 + auxiliary
    scanned += 1
assert scanned == 3335
assert providers and all(len(rows) == 1 for rows in providers.values()), providers
assert sum(name.startswith('?MemoryUsage@') for name in providers) == 1, providers
assert sum(name.startswith('??0OverlapGenerationModule@') for name in providers) == 1, providers
assert sum(name.startswith('?GetPoolInputQueue@') for name in providers) == 2, providers
result = {
    'head': audit['head'], 'report': str(report_path), 'report_sha256': audit['report_sha256'],
    'terminal_audit': str(audit_path),
    'terminal_audit_sha256': hashlib.sha256(audit_path.read_bytes()).hexdigest(),
    'native_objects_scanned': scanned, 'providers': providers,
    'scope': 'Exactly one external provider per actual decorated signature in the successful '
             'full-sweep objects: Renderer MemoryUsage, Overlap constructor, const and mutable '
             'Pool input-queue getters. Failed TUs and all other duplicate/unresolved symbols '
             'remain outside this bounded symbol audit. This is not a complete or successful link.'}
with (HERE / 'three-native-provider-proof.json').open('x', encoding='utf-8') as stream:
    json.dump(result, stream, indent=2)
print(json.dumps(result, indent=2))
