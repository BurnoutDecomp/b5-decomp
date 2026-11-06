from pathlib import Path
import hashlib, json, struct

HERE = Path(__file__).resolve().parent

def sha(p):
    return hashlib.sha256(Path(p).read_bytes()).hexdigest()

def text_sections(path):
    b = Path(path).read_bytes()
    machine, count, _, offset, nsym, optsize, _ = struct.unpack_from('<HHIIIHH', b)
    assert machine == 0x1F2
    strings = b[offset + nsym * 18:]
    symbols, index = {}, 0
    while index < nsym:
        raw, value, sec, typ, storage, aux = struct.unpack_from('<8sIhHBB', b, offset + index * 18)
        name = (strings[struct.unpack_from('<I', raw, 4)[0]:].split(b'\0', 1)[0]
                if raw[:4] == b'\0' * 4 else raw.rstrip(b'\0')).decode()
        symbols[index] = (name, value, sec, typ, storage)
        index += 1 + aux
    result = {}
    for sec in range(1, count + 1):
        s = struct.unpack_from('<8sIIIIIIHHI', b, 20 + optsize + (sec - 1) * 40)
        if not s[-1] & 0x20 or not s[3]:
            continue
        names = sorted(name for name, value, section, typ, storage in symbols.values()
                       if section == sec and value == 0 and (typ & 0x20 or name.startswith('?')))
        assert names
        relocs = []
        for i in range(s[7]):
            at, sym, kind = struct.unpack_from('<IIH', b, s[5] + i * 10)
            relocs.append((at, symbols[sym][0] if kind != 0x12 else None, kind))
        key = '|'.join(names)
        assert key not in result
        result[key] = {'code_sha256': hashlib.sha256(b[s[4]:s[4] + s[3]]).hexdigest(),
                       'size': s[3], 'relocations': relocs}
    return result

old = json.loads((HERE.parent / 'native-formatter-root/native/compile-results.json').read_text())
new = json.loads((HERE / 'native/compile-results.json').read_text())
retained = []
for row in old['results']:
    other = next(r for r in new['results'] if r['source'] == row['source'])
    a, b = text_sections(row['object']), text_sections(other['object'])
    assert a == b, row['source']
    retained.append({'source': row['source'], 'before_object': row['object'],
                     'before_sha256': sha(row['object']), 'after_object': other['object'],
                     'after_sha256': sha(other['object']), 'functions': len(a),
                     'bytes': sum(s['size'] for s in a.values()), 'text': a})
result = {'retained': retained, 'verifier_sha256': sha(__file__),
          'scope': 'Every emitted executable section and its actual symbol relocations in the unchanged Formatter and Manager CPPs. Not a whole-header/all-program regression claim.'}
with (HERE / 'retained-consumer-text.json').open('x', encoding='utf-8') as f:
    json.dump(result, f, indent=2)
print(json.dumps([{'source': x['source'], 'functions': x['functions'], 'bytes': x['bytes']}
                  for x in retained], indent=2))
