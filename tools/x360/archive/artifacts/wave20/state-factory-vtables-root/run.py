import hashlib, json, os, pathlib, re, subprocess, sys

OUT = pathlib.Path(__file__).resolve().parent
WORK = pathlib.Path('D:/Reverse/BP-X360-Experimental-Worktrees/r1_vecfloat_repair_terra')
PARENT = '7af1b43257cb7dc9187440cc66db3e003c28f8c6'
PREFIX = 'src/GameShared/GameClasses/Graphics/'
FACTORIES = [('BlendState',0x820CDC1C,0x827EB2D8),
             ('RasterizerState',0x820CDC28,0x827EBF30),
             ('DepthStencilState',0x820CDC34,0x827EBBA0)]
sys.path.insert(0,'D:/Reverse/BP-Decomp_Workflow/tools/re')
from x360rd import u32

def git(*args):
    return subprocess.check_output(['git', *args], cwd=WORK).decode().strip()

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def read(path):
    raw=path.read_bytes()
    return raw.decode().replace('\r\n','\n'), '\r\n' if b'\r\n' in raw else '\n'

def write(path,body,eol):
    path.write_bytes(body.replace('\n',eol).encode())

assert git('rev-parse','HEAD')==PARENT and not git('status','--porcelain=v1')
assert u32(0x82815570)==0x7c8a2378 and u32(0x82F264A8)==4
assert u32(0x8284CB38)==0x4e800020
assert [u32(0x82C296C8),u32(0x82C296CC)]==[0x38600001,0x4e800020]
for name,table,construct in FACTORIES:
    assert [u32(table+i*4) for i in range(3)]==[construct,0x8284CB38,0x82C296C8]
sources=[]
paths=[]
for name,table,construct in FACTORIES:
    factory='Cgs'+name+'Factory'
    source=PREFIX+factory+'.cpp'
    header=PREFIX+factory+'.h'
    sources.append(source)
    paths += [source,header]
    p=WORK/source
    body,eol=read(p)
    if name=='BlendState':
        old="""// dispatch interpreter select between. Only Construct is console-attested for this TU;
// Destruct / Prepare have no body anywhere in the image and GetState is inlined into
// its readers (see the header)."""
        new="""// dispatch interpreter select between. GetState is inlined into its readers."""
    elif name=='DepthStencilState':
        old="""// between. Only Construct is console-attested for this TU (Destruct / Prepare / GetState
// are declaration-only -- see the header)."""
        new="""// between. GetState is inlined into its readers."""
    else:
        old="""// post-fx chain and the dispatch interpreter select between. Only Construct is
// console-attested for this TU (Destruct / Prepare / GetState are declaration-only --
// see the header, which is NEW in this wave: this class used to be declared TU-locally
// right here, non-polymorphically and with a decompiler-artefact return type)."""
        new="""// post-fx chain and the dispatch interpreter select between."""
    assert body.count(old)==1
    body=body.replace(old,new).rstrip()
    body+=f"""

// ARTIST vtable 0x{table:08X}, slot +0x04 targets the shared return at 0x8284CB38.
void {factory}::Destruct()
{{
}}

// ARTIST vtable slot +0x08 targets the shared true return at 0x82C296C8.
bool {factory}::Prepare()
{{
    return true;
}}
"""
    write(p,body,eol)
    p=WORK/header
    body,eol=read(p)
    if name=='DepthStencilState':
        start=body.index('// header and defined in the .cpp. Destruct')
        end=body.index('//   * GetState IS NOW BODIED',start)
        body=body[:start]+"""// header and defined in the .cpp. The ARTIST vtable at 0x820CDC34 names
// Construct and the shared Destruct and Prepare bodies.
//
// Accessor details:
// 
"""+body[end:]
        body=body.replace('// \n','//\n')
        body=body.replace('virtual void Destruct();   // declared in the original header; declared-only here',
                          'virtual void Destruct();')
        body=body.replace('virtual bool Prepare();    // declared in the original header; declared-only here',
                          'virtual bool Prepare();')
    else:
        start=body.index('    // Both are declared in the original header.')
        end=body.index('    virtual void Destruct();',start)
        body=body[:start]+f'    // Shared console bodies appear in the ARTIST vtable at 0x{table:08X}.\n'+body[end:]
    write(p,body,eol)
git('diff','--check','--',*paths)
assert set(git('diff','--name-only').splitlines())==set(paths)
args=json.loads((OUT.parent/'gui-queue-length-root/command.json').read_text())
args=args[:args.index('--source')]
args[args.index('--out')+1]=str(OUT/'native1')
for source in sources: args += ['--source',source]
temp=OUT/'temp'
temp.mkdir()
(OUT/'command.json').write_text(json.dumps(args,indent=2)+'\n')
subprocess.run(args,cwd=WORK,env=dict(os.environ,TEMP=str(temp),TMP=str(temp)),check=True)
report_path=OUT/'native1/compile-results.json'
report=json.loads(report_path.read_text())
assert report['requested']==report['passed']==3 and not report['failed']
assert all(row['machine']==498 for row in report['results'])
products={}
for row,(name,table,construct) in zip(report['results'],FACTORIES):
    factory='Cgs'+name+'Factory'
    assert row['source']==PREFIX+factory+'.cpp'
    listing=pathlib.Path(row['assembly']).read_text(errors='replace')
    for method,suffix,expected in [('Destruct','UAAXXZ',['4e800020']),
                                   ('Prepare','UAA_NXZ',['38600001','4e800020'])]:
        symbol=f'?{method}@{factory}@@{suffix}'
        body=listing.split('Begin code for function: '+symbol,1)[1].split('End code for function: '+symbol,1)[0]
        words=re.findall(r'^\s*[0-9a-f]{5}\s+([0-9a-f]{8})\s',body,re.M)
        assert words==expected,(symbol,words)
        products[symbol]=words
message=OUT/'COMMIT_MESSAGE.txt'
message.write_text('Restore ARTIST render-state factory virtual methods\n\nCo-Authored-By: GPT-6 <noreply@openai.com>\n')
git('add','--',*paths)
assert set(git('diff','--cached','--name-only').splitlines())==set(paths)
git('commit','--file',str(message))
assert not git('status','--porcelain=v1')
receipt={'parent':PARENT,'commit':git('rev-parse','HEAD'),'paths':{p:sha(WORK/p) for p in paths},
         'artist_vtables':{n:{'table':hex(t),'slots':[hex(c),'0x8284cb38','0x82c296c8']} for n,t,c in FACTORIES},
         'emitted_and_artist_words':products,'report':str(report_path),'report_sha256':sha(report_path)}
(OUT/'DELIVERY.json').write_text(json.dumps(receipt,indent=2)+'\n')
print(receipt['commit'])
