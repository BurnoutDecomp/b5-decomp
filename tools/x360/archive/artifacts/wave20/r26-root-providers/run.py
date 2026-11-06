import hashlib,json,os,pathlib,subprocess
D=pathlib.Path(__file__).resolve().parent
W=pathlib.Path('D:/Reverse/BP-X360-Experimental-Worktrees/r1_vecfloat_repair_terra')
BASE='c7376f3fed4105792eabdb9622d3ffefd7535009'
END='5cf70ba971fafdcae3011463e54ca435b33094ec'
H='src/SDKs/EATech/rwmovie/CLocalHuffman.h'
BRANCH='codex/experimental-x360-r26-root-providers'
OLD=b'extern void* XMemAlloc(u32 uSize, u32 uAttributes);\nextern void  XMemFree(void* pAddress, u32 uAttributes);'
NEW=b'#include <xtl.h> // XDK XMemAlloc / XMemFree'
GROUPS=[('6530087646d77f9452e09cff11c2d8eb8292fac4','18430d2df92efdd34fa777f988eae5af1866d362',
         'Recover native movie Huffman code paths',6),
        ('18430d2df92efdd34fa777f988eae5af1866d362',END,
         'Restore native corona resource release order',4)]
def raw(*a):return subprocess.check_output(['git',*a],cwd=W)
def git(*a):return raw(*a).decode().strip()
def sha(p):return hashlib.sha256(pathlib.Path(p).read_bytes()).hexdigest()
def replace(s,a,b):
    assert s.count(a)==1
    return s.replace(a,b)
assert git('rev-parse','HEAD')==END and not git('status','--porcelain=v1')
prepared=[]
paths=set()
for start,stop,msg,count in GROUPS:
    group_paths=git('diff','--name-only',start,stop).splitlines()
    assert len(group_paths)==count and not paths.intersection(group_paths)
    paths.update(group_paths)
    contents={}
    for p in group_paths:
        original=raw('show',f'{start}:{p}')
        target=raw('show',f'{BASE}:{p}')
        recovered=raw('show',f'{stop}:{p}')
        if p==H:
            assert replace(original,OLD,NEW)==target
            recovered=replace(recovered,OLD,NEW)
        else:
            assert original==target,p
        contents[p]=recovered
    prepared.append((msg,contents))
git('switch','-c',BRANCH,BASE)
assert git('rev-parse','HEAD')==BASE and not git('status','--porcelain=v1')
for msg,contents in prepared:
    for p,data in contents.items():
        file=W/p
        original=file.read_bytes()
        if b'\r\n' in original and b'\r\n' not in data:
            data=data.replace(b'\n',b'\r\n')
        file.write_bytes(data)
assert set(git('diff','--name-only').splitlines())==paths
git('diff','--check','--',*sorted(paths))
args=json.loads((D.parent/'huffman-combined-root/command.json').read_text())
args=args[:args.index('--source')]
args[args.index('--out')+1]=str(D/'native')
sources=sorted(p for p in paths if p.endswith('.cpp'))+['src/GameSource/Graphics/BrnRendererModule.cpp']
assert len(sources)==6
for p in sources:args+=['--source',p]
before={p:sha(W/p) for p in sorted(paths|set(sources))}
temp=D/'temp'
temp.mkdir()
(D/'command.json').write_text(json.dumps(args,indent=2)+'\n')
subprocess.run(args,cwd=W,env=dict(os.environ,TEMP=str(temp),TMP=str(temp)),check=True)
assert all(sha(W/p)==h for p,h in before.items())
report=D/'native/compile-results.json'
r=json.loads(report.read_text())
assert r['passed']==r['requested']==6 and r['failed']==0 and all(row['machine']==498 for row in r['results'])
for row in r['results']:
    if pathlib.Path(row['source']).name in ('CLocalHuffman.cpp','CLocalHuffmanEncoder.cpp','CAltTablesEncoder.cpp'):
        listing=pathlib.Path(row['assembly']).read_text(errors='replace')
        assert '?XMemAlloc@@YAPAXII@Z' not in listing and '?XMemFree@@YAXPAXI@Z' not in listing
        assert 'XMemAlloc' in listing and 'XMemFree' in listing
commits=[]
for i,(msg,contents) in enumerate(prepared,1):
    git('add','--',*contents)
    assert set(git('diff','--cached','--name-only').splitlines())==set(contents)
    msg_file=D/f'message-{i}.txt'
    msg_file.write_text(msg+'\n\nCo-Authored-By: GPT-6 <noreply@openai.com>\n')
    git('commit','--file',str(msg_file))
    commits.append({'commit':git('rev-parse','HEAD'),'paths':sorted(contents)})
assert not git('status','--porcelain=v1')
receipt={'base':BASE,'head':git('rev-parse','HEAD'),'groups':commits,'input_sha256':before,
         'report':str(report),'report_sha256':sha(report),
         'verified_artifact_sha256':{row['source']:{k:sha(row[k]) for k in ('object','assembly','log')} for row in r['results']},
         'preserved_r24_xdk_heap_abi':True}
(D/'DELIVERY.json').write_text(json.dumps(receipt,indent=2)+'\n')
print(json.dumps(commits))
