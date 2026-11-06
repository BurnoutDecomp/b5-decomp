import hashlib,json,os,pathlib,struct,subprocess,sys
D=pathlib.Path(__file__).resolve().parent
W=pathlib.Path('D:/Reverse/BP-X360-Experimental-Worktrees/r1_vecfloat_repair_terra')
HEAD='07bcdd6f066c96d96d9cb426745a4a894316a88b'
sources=[
    ('src/SDKs/EATech/include/snd/CMpegBase.cpp',0x8214B6A8,0x8214B5E8),
    ('vendor/renderware/src/rw/audio/core/CMpegBase.cpp',0x8215A66C,0x8215A680)
]
sys.path.insert(0,'D:/Reverse/BP-Decomp_Workflow/tools/re')
from x360rd import rd,u32
def git(*a):return subprocess.check_output(['git',*a],cwd=W).decode().strip()
def sha(p):return hashlib.sha256(pathlib.Path(p).read_bytes()).hexdigest()
def array(ty,name,address,values,columns):
    rows=[', '.join(map(str,values[i:i+columns])) for i in range(0,len(values),columns)]
    return (f'    // ARTIST 0x{address:08X}\n    const {ty} {name}[{len(values)}] =\n    {{\n'+
            ',\n'.join('        '+row for row in rows)+'\n    };')
assert git('rev-parse','HEAD')==HEAD and not git('status','--porcelain=v1')
assert u32(0x82815570)==0x7c8a2378 and u32(0x82F264A8)==4
assert rd(sources[0][1],20)==rd(sources[1][1],20)
assert rd(sources[0][2],192)==rd(sources[1][2],192)
tables={}
for path,sample,rate in sources:
    source=W/path
    raw=source.read_bytes()
    eol='\r\n' if b'\r\n' in raw else '\n'
    s=raw.decode().replace('\r\n','\n')
    sr=f'word_{sample:08X}'
    br=f'word_{rate:08X}'
    a=s.index(f'extern "C" const u16 {sr}[];')
    z=s.index('\n',s.index(f'extern "C" const s16 {br}[];',a))
    raw_sample=rd(sample,20)
    raw_rate=rd(rate,192)
    sample_table=array('u16','KAU_SAMPLE_RATE_HZ',sample,struct.unpack('>10H',raw_sample),10)
    rate_table=array('s16','KAI_BIT_RATE_KBPS',rate,struct.unpack('>96h',raw_rate),16)
    s=s[:a]+'namespace\n{\n'+sample_table+'\n\n'+rate_table+'\n}'+s[z:]
    assert s.count(sr)==s.count(br)==1
    s=s.replace(sr,'KAU_SAMPLE_RATE_HZ').replace(br,'KAI_BIT_RATE_KBPS')
    source.write_bytes(s.replace('\n',eol).encode())
    tables[path]=(sample,raw_sample,rate,raw_rate)
paths=sorted(tables)
assert set(git('diff','--name-only').splitlines())==set(paths)
git('diff','--check','--',*paths)
args=json.loads((D.parent/'ping-server-root/command.json').read_text())
args=args[:args.index('--source')]
args[args.index('--out')+1]=str(D/'native')
for p in paths:args+=['--source',p]
temp=D/'temp'
temp.mkdir()
(D/'command.json').write_text(json.dumps(args,indent=2)+'\n')
subprocess.run(args,cwd=W,env=dict(os.environ,TEMP=str(temp),TMP=str(temp)),check=True)
report=D/'native/compile-results.json'
r=json.loads(report.read_text())
assert r['passed']==r['requested']==2 and r['failed']==0
for row in r['results']:
    assert row['machine']==498
    obj=pathlib.Path(row['object']).read_bytes()
    s,bs,a,ba=tables[row['source']]
    assert obj.count(bs)==obj.count(ba)==1
    listing=pathlib.Path(row['assembly']).read_text(errors='replace')
    assert f'word_{s:08X}' not in listing and f'word_{a:08X}' not in listing
msg=D/'message.txt'
msg.write_text('Restore native MPEG sample-rate and bitrate tables\n\nCo-Authored-By: GPT-6 <noreply@openai.com>\n')
git('add','--',*paths)
assert set(git('diff','--cached','--name-only').splitlines())==set(paths)
git('commit','--file',str(msg))
assert not git('status','--porcelain=v1')
result={'base':HEAD,'commit':git('rev-parse','HEAD'),'paths':{p:sha(W/p) for p in paths},
        'native_tables':{p:{hex(s):len(bs),hex(a):len(ba)} for p,(s,bs,a,ba) in tables.items()},
        'table_bytes_match_coff':True,'report':str(report),'report_sha256':sha(report)}
(D/'DELIVERY.json').write_text(json.dumps(result,indent=2)+'\n')
print(result['commit'])
