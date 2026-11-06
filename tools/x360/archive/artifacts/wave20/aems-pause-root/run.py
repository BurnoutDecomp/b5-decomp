import hashlib,json,os,pathlib,subprocess,sys
D=pathlib.Path(__file__).resolve().parent
W=pathlib.Path('D:/Reverse/BP-X360-Experimental-Worktrees/r1_vecfloat_repair_terra')
HEAD='9f8a0415ac0ee77a8ad89d562ea5142dcfd714ad'
P='src/SDKs/EATech/include/snd/'
C=P+'sndaemssampleplayer.cpp'
H=P+'sndaemssampleplayer.h'
A=P+'sndo.cpp'
sys.path.insert(0,'D:/Reverse/BP-Decomp_Workflow/tools/re')
from x360rd import u32
def git(*a):return subprocess.check_output(['git',*a],cwd=W).decode().strip()
def sha(p):return hashlib.sha256(pathlib.Path(p).read_bytes()).hexdigest()
def edit(p,fn):
    f=W/p
    b=f.read_bytes()
    eol='\r\n' if b'\r\n' in b else '\n'
    f.write_bytes(fn(b.decode().replace('\r\n','\n')).replace('\n',eol).encode())
def replace(s,a,b):
    assert s.count(a)==1
    return s.replace(a,b)
assert git('rev-parse','HEAD')==HEAD and not git('status','--porcelain=v1')
assert u32(0x82815570)==0x7c8a2378 and u32(0x82F264A8)==4
expected=[0x82b723c8,0x82b72060,0x82b720d8,0x82b71d10,0x82b71e98,0x82b71f20,0x82b72380]
assert [u32(a) for a in range(0x8214b310,0x8214b32c,4)]==expected
assert [u32(a) for a in range(0x820ab14c,0x820ab168,4)]==[0x82c08f60]*6+[0x82681950]
assert u32(0x82b720b8)==0x39600001 and u32(0x82b720bc)==0x917f0068
def header(s):
    a=s.index('// FLAG (methods declared here')
    b=s.index('// ============================================================================',a)
    s=s[:a]+s[b:]
    s=replace(s,"""    // @ 0x82B72380 -- BLOCKED (mis-attributed AemsRWSamplePlayer thunk; see file header).
    // Declared for the base virtual-dtor slot; NOT defined in this TU.
    virtual ~AemsStandardSamplePlayer();""","""    // ARTIST 0x82B72380 restores the IAemsSamplePlayer vtable before deletion.
    virtual ~AemsStandardSamplePlayer();""")
    return replace(s,'    virtual void Pause();                                         // own TU (declared for slot)',
                   '    virtual void Pause();                                         // @ 0x82B72060')
def cpp(s):
    s=replace(s,"""// NOT in this TU (see the header): ~AemsStandardSamplePlayer (the ledger's deleting
// destructor address belongs to AemsRWSamplePlayer), Pause (own TU).
""",'')
    a='// ----------------------------------------------------------------------------\n// Snd9::AemsStandardSamplePlayer::Release @ 0x82B723C8'
    s=replace(s,a,"""// ARTIST 0x82B72380. The base destructor restores the interface vtable.
AemsStandardSamplePlayer::~AemsStandardSamplePlayer()
{
}

"""+a)
    a='// ----------------------------------------------------------------------------\n// Snd9::AemsStandardSamplePlayer::Unpause @ 0x82B720D8'
    return replace(s,a,"""// ARTIST 0x82B72060
void AemsStandardSamplePlayer::Pause()
{
    rw::audio::core::PlugIn::SetAttribute(mpGain, 0, 0.0f);
    if (mpSendWet)
        rw::audio::core::PlugIn::SetAttribute(mpSendWet, 0, 0.0f);
    rw::audio::core::PlugIn::SetAttribute(mpResample, 0, 0.0f);
    mPauseState = 1;
}

"""+a)
edit(H,header)
edit(C,cpp)
assert set(git('diff','--name-only').splitlines())=={C,H}
git('diff','--check','--',C,H)
args=json.loads((D.parent/'ping-server-root/command.json').read_text())
args=args[:args.index('--source')]
args[args.index('--out')+1]=str(D/'native')
for p in (C,A):args+=['--source',p]
temp=D/'temp'
temp.mkdir()
(D/'command.json').write_text(json.dumps(args,indent=2)+'\n')
subprocess.run(args,cwd=W,env=dict(os.environ,TEMP=str(temp),TMP=str(temp)),check=True)
report=D/'native/compile-results.json'
r=json.loads(report.read_text())
assert r['passed']==r['requested']==2 and r['failed']==0
rows={row['source']:row for row in r['results']}
assert all(row['machine']==498 for row in r['results'])
asm=pathlib.Path(rows[C]['assembly']).read_text(errors='replace')
pause='?Pause@AemsStandardSamplePlayer@Snd9@@UAAXXZ'
dtor='??1AemsStandardSamplePlayer@Snd9@@UAA@XZ'
body=asm.split('Begin code for function: '+pause,1)[1].split('End code for function:',1)[0]
assert sum(' bl ' in l and 'SetAttribute' in l for l in body.splitlines())==3
assert '68h(' in body
assert 'Begin code for function: '+dtor in asm
msg=D/'message.txt'
msg.write_text('Restore native AEMS sample-player pause and teardown\n\nCo-Authored-By: GPT-6 <noreply@openai.com>\n')
git('add','--',C,H)
assert set(git('diff','--cached','--name-only').splitlines())=={C,H}
git('commit','--file',str(msg))
assert not git('status','--porcelain=v1')
result={'base':HEAD,'commit':git('rev-parse','HEAD'),'paths':{p:sha(W/p) for p in (C,H)},
        'artist_derived_vtable':{'address':'0x8214B310','entries':[hex(a) for a in expected]},
        'artist_base_vtable':'0x820AB14C, six pure control slots and actual base destructor 0x82681950',
        'artist_pause':'0x82B72060..0x82B720D7','artist_destructor':'0x82B72380..0x82B723C3',
        'report':str(report),'report_sha256':sha(report)}
(D/'DELIVERY.json').write_text(json.dumps(result,indent=2)+'\n')
print(result['commit'])
