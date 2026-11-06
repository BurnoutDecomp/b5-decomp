import hashlib,json,os,pathlib,re,struct,subprocess,sys

OUT=pathlib.Path(__file__).resolve().parent
WORK=pathlib.Path('D:/Reverse/BP-X360-Experimental-Worktrees/r1_vecfloat_repair_terra')
PARENT='3733740bdfae7307d2159b3277fe01c43b23eb68'
SOURCE='src/SDKs/Packages/ICE/ICEWrapper.cpp'
HDR='src/GameSource/Director/Camera/Utils/BrnDebugController.h'
CONTROLLER=HDR[:-1]+'cpp'
TABLES=[('maPressConverter',0x82F28718),('maPadConverter',0x82F28770)]
sys.path.insert(0,'D:/Reverse/BP-Decomp_Workflow/tools/re')
from x360rd import u32

def git(*args):
    return subprocess.check_output(['git',*args],cwd=WORK).decode().strip()
def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()
def update(path,transform):
    b=path.read_bytes()
    eol='\r\n' if b'\r\n' in b else '\n'
    s=b.decode().replace('\r\n','\n')
    path.write_bytes(transform(s).replace('\n',eol).encode())

assert git('rev-parse','HEAD')==PARENT and not git('status','--porcelain=v1')
assert u32(0x82815570)==0x7c8a2378 and u32(0x82F264A8)==4
assert u32(0x82001C98)==0x3f800000
facts={name:[u32(address+i*4) for i in range(22)] for name,address in TABLES}
assert all(v<64 for row in facts.values() for v in row)
def rewrite_source(s):
    old="""// they can be on the link while this TU cannot -- the two converter tables UpdateAction
// indexes are still unhomed (see the FLAG below)."""
    new="""// they can be on the link independently of the debug-controller input path."""
    assert s.count(old)==1
    s=s.replace(old,new)
    begin=s.index('// ----------------------------------------------------------------------------\n// FLAG (unhomed dev-tools dependency)')
    end=s.index('namespace\n{',begin)
    tables=[]
    for name,address in TABLES:
        data=facts[name]
        values=['        '+', '.join('{'+str(v)+'}' for v in data[i:i+8]) for i in range(0,len(data),8)]
        tables.append(f'    // ARTIST 0x{address:08X}\n    const ICE::EControlToICEAction {name}[] =\n    {{\n'+',\n'.join(values)+'\n    };')
    body='\n\n'.join(tables)
    return s[:begin]+'namespace\n{\n'+body+'\n\n'+s[end+len('namespace\n{\n'):]
def rewrite_header(s):
    old='                f32 GetControlValue(s32 liControl) const;'
    new="""                f32 GetControlValue(s32 liControl) const
                {
                    return mDebugControllerInfo.mafControlValue[liControl];
                }"""
    assert s.count(old)==1
    return s.replace(old,new)
update(WORK/SOURCE,rewrite_source)
update(WORK/HDR,rewrite_header)
assert set(git('diff','--name-only').splitlines())=={SOURCE,HDR}
git('diff','--check','--',SOURCE,HDR)
args=json.loads((OUT.parent/'gui-queue-length-root/command.json').read_text())
args=args[:args.index('--source')]
args[args.index('--out')+1]=str(OUT/'native1')
for source in (SOURCE,CONTROLLER): args += ['--source',source]
temp=OUT/'temp'
temp.mkdir()
(OUT/'command.json').write_text(json.dumps(args,indent=2)+'\n')
subprocess.run(args,cwd=WORK,env=dict(os.environ,TEMP=str(temp),TMP=str(temp)),check=True)
report_path=OUT/'native1/compile-results.json'
report=json.loads(report_path.read_text())
assert report['requested']==report['passed']==2 and not report['failed']
rows={r['source']:r for r in report['results']}
assert all(r['machine']==498 for r in rows.values())
row=rows[SOURCE]
asm=pathlib.Path(row['assembly']).read_text(errors='replace')
assert 'GetControlValue@DebugController' not in asm
assert len(re.findall(r'\blfsx\b',asm))>=2
coff=pathlib.Path(row['object']).read_bytes()
assert all(coff.count(b''.join(struct.pack('>I',v) for v in data))==1 for data in facts.values())
msg=OUT/'COMMIT_MESSAGE.txt'
msg.write_text('Restore native ICE control action tables and value access\n\nCo-Authored-By: GPT-6 <noreply@openai.com>\n')
git('add','--',SOURCE,HDR)
assert set(git('diff','--cached','--name-only').splitlines())=={SOURCE,HDR}
git('commit','--file',str(msg))
assert not git('status','--porcelain=v1')
receipt={'parent':PARENT,'commit':git('rev-parse','HEAD'),'paths':{p:sha(WORK/p) for p in (SOURCE,HDR)},
         'artist_tables':{n:{'address':hex(a),'words':facts[n]} for n,a in TABLES},
         'table_bytes_match_coff':True,'artist_data_reads':['0x8253399C','0x82533A08'],
         'emitted_lfsx':True,'report':str(report_path),'report_sha256':sha(report_path)}
(OUT/'DELIVERY.json').write_text(json.dumps(receipt,indent=2)+'\n')
print(receipt['commit'])
