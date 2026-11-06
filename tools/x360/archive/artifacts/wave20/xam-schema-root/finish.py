import hashlib, json, os, pathlib, subprocess, sys

BASE = pathlib.Path('D:/Reverse/BP-X360-Experimental-Artifacts')
OUT = pathlib.Path(__file__).resolve().parent
WORK = pathlib.Path('D:/Reverse/BP-X360-Experimental-Worktrees/r1_vecfloat_repair_terra')
PREFIX = 'src/SDKs/Xam/'
PARENT = '015c30af9d873e85481dba4b2e6176d3b317e73c'

def git(*args):
    return subprocess.check_output(['git', *args], cwd=WORK).decode().strip()

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def edit(name, changes):
    path = WORK / (PREFIX+name)
    actual = path.read_bytes()
    contents = subprocess.check_output(['git','show','HEAD:'+PREFIX+name],cwd=WORK)
    if b'\r\n' in actual and b'\r\n' not in contents:
        contents = contents.replace(b'\n', b'\r\n')
    original = contents
    eol = b'\r\n' if b'\r\n' in contents else b'\n'
    for before,after in changes:
        a,b = [x.encode().replace(b'\n',eol) for x in (before,after)]
        assert contents.count(a) == 1, (name,before)
        contents = contents.replace(a,b)
    assert actual in (original, contents), name
    if actual != contents:
        path.write_bytes(contents)


assert git('rev-parse','HEAD') == PARENT
changed = [PREFIX+x for x in ('CConformanceList.cpp','CSchemaData.h','CSchemaData.cpp','CSchemaAccess.h')]
assert set(git('diff','--name-only').splitlines()) == set(changed)
git('diff','--check','--',*changed)
sources = [PREFIX+x for x in ('CConformanceList.cpp','CSchemaData.cpp','CSchemaAccess.cpp','CMarshaller.cpp')]
report_path = OUT/'native1/compile-results.json'
report = json.loads(report_path.read_text())
assert report['requested'] == report['passed'] == 4 and not report['failed']
rows = {x['source']:x for x in report['results']}
assert set(rows)==set(sources) and all(r['machine']==498 for r in rows.values())
for file,symbol in [('CSchemaData.cpp','?GetSchemaEntry@CSchemaData@@QAAHIPAX@Z'),
                    ('CConformanceList.cpp','?ReleaseConformanceInfo@CConformanceList@@QAAHI@Z')]:
    asm = pathlib.Path(rows[PREFIX+file]['assembly']).read_text(errors='replace')
    assert symbol in asm
    if file=='CSchemaData.cpp':
        body = asm[asm.index('Begin code for function: '+symbol):asm.index('End code for function: '+symbol)]
        for expected in ('a14b0028','a12b002a','814b0030','38a00018'):
            assert expected in body, (expected,body)
message = OUT/'COMMIT_MESSAGE.txt'
message.write_text('Recover native Xam schema entries and slot release\n\nCo-Authored-By: GPT-6 <noreply@openai.com>\n')
git('add','--',*changed)
assert set(git('diff','--cached','--name-only').splitlines()) == set(changed)
git('commit','--file',str(message))
assert not git('status','--porcelain=v1')
receipt = {'parent':PARENT,'commit':git('rev-parse','HEAD'),
           'paths':{p:sha(WORK/p) for p in changed},'report':str(report_path),
           'report_sha256':sha(report_path),
           'artist':{'GetSchemaEntry':'0x8297F228..0x8297F27F',
                     'BindToSchema':'0x8297F380..0x8297F3F7',
                     'ReleaseConformanceInfo':'0x829803A0..0x829803FB'},
           'facts':['Native BindToSchema passes a 24-byte stack record, with its first 12 bytes used for the two sizes and offsets.',
                    'Schema lookup reads count at +0x28, stride at +0x2A, and table at +0x30. Copies 24 bytes.',
                    'Release rejects index >= 64, treats slot 63 as success without mutation, requires both flags for ordinary slots, and clears only acquired.'],
           'outputs':{p:{kind:sha(pathlib.Path(r[kind])) for kind in ('assembly','object','log')} for p,r in rows.items()}}
(OUT/'DELIVERY.json').write_text(json.dumps(receipt,indent=2)+'\n')
print(receipt['commit'])

