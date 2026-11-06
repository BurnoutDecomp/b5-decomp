import hashlib, json, pathlib, re, struct
root = pathlib.Path(r'D:/Reverse/BP-X360-Experimental-Artifacts/wave30/protossl-native/entropy-final')
report = json.loads((root / 'compile-results.json').read_text())
assert report['passed'] == 3 and report['failed'] == 0
row = next(x for x in report['results'] if x['source'].endswith('protossl_random_x360.cpp'))
assert row['passed'] and row['machine'] == 498
obj = pathlib.Path(row['object'])
assert struct.unpack('<H', obj.read_bytes()[:2])[0] == 498
listing = pathlib.Path(row['assembly']).read_text()
def section(name):
    return re.search(r'(?ms)^' + re.escape(name) + r'\s+PROC\b.*?^' + re.escape(name) + r'\s+ENDP', listing).group()
def insns(section):
    return [(int(m.group(1),16), m.group(2).lower(), m.group(3).strip()) for m in re.finditer(r'(?m)^\s*([0-9a-f]+)\s+([0-9a-f]{8})\s+([^\r\n;]+)',section,re.I)]
leaf = insns(section('ProtoSSLSampleStack'))
expected = ['7c6a1b78','39600000','91610050','5569103a','39010050','396b0001','2f0b0020','7d29402e','91610050','7d495214','4198ffe4','7d435378','4e800020']
assert [x[1] for x in leaf] == expected, leaf
owner = insns(section('ProtoSSLGenerateRandom'))
assert [x[1] for x in owner[:3]] == ['7d8802a6','48000001','9421ff80']
assert '__savegprlr_28' in owner[1][2]
assert '.pdata\tSEGMENT' in listing and re.search(r'DD\s+ProtoSSLGenerateRandom', listing)
assert re.search(r'gProtoSSLRandomSeed\s+DD\s+04H DUP \(\?\)',listing)
pre = owner[:next(i for i,x in enumerate(owner) if 'ProtoSSLSampleStack' in x[2])]
assert not any(re.search(r'\b(stw|std|stb|sth)\b.*\(r1\)',x[2]) for x in pre), pre
calls = [x[2].split()[-1] for x in owner if re.search(r'\bbl\s+',x[2])]
assert calls == ['__savegprlr_28','NetTick','NetTick','ProtoSSLSampleStack','CryptArc4Init','CryptArc4Apply'], calls
receipt={'status':'pass','source':row['source'],'machine':row['machine'],'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'listing_sha256':hashlib.sha256(pathlib.Path(row['assembly']).read_bytes()).hexdigest(),'leaf_opcodes':expected,'owner_first_opcodes':[x[1] for x in owner[:3]],'calls':calls,'sample_range':'owner r1+0x50..r1+0xcc inclusive','caveat':'No integrated link or runtime/asynchronous unwind test'}
(root / 'frame-verification.json').write_text(json.dumps(receipt,indent=2)+'\n')
print(json.dumps(receipt,indent=2))
