import hashlib,json,os,pathlib,subprocess
D=pathlib.Path(__file__).resolve().parent
W=pathlib.Path('D:/Reverse/BP-X360-Experimental-Worktrees/r1_vecfloat_repair_terra')
HEAD='50600bd0cb6680c672a97fdb3a7e829a4fc4efa7'
SOURCE='src/GameSource/World/AI/BrnAICar.cpp'
CALLER='src/GameSource/World/AI/RaceBalancing/BrnRaceBalancingDebugComponent.cpp'
def git(*a):return subprocess.check_output(['git',*a],cwd=W).decode().strip()
def sha(p):return hashlib.sha256(pathlib.Path(p).read_bytes()).hexdigest()
assert git('rev-parse','HEAD')==HEAD and not git('status','--porcelain=v1')
p=W/SOURCE
raw=p.read_bytes()
eol='\r\n' if b'\r\n' in raw else '\n'
s=raw.decode().replace('\r\n','\n')
a='#include "GameSource/World/AI/BrnAICar.h"\n'
assert s.count(a)==1
s=s.replace(a,a+'#include "GameSource/World/AI/BrnAIDriver.h"\n')
a='    namespace vpu = rw::math::vpu;\n'
assert s.count(a)==1
s=s.replace(a,a+"""
    // ARTIST race-balancing HUD checks the driver before reading forced speed.
    bool AICar::IsSpeedMatching() const
    {
        return mpDriver != 0 && mpDriver->UseForcedSpeed();
    }

    f32 AICar::GetSpeedMatchSpeed() const
    {
        return mpDriver->GetForcedSpeed();
    }
""")
p.write_bytes(s.replace('\n',eol).encode())
assert git('diff','--name-only')==SOURCE
git('diff','--check','--',SOURCE)
args=json.loads((D.parent/'r26-root-providers/command.json').read_text())
args=args[:args.index('--source')]
args[args.index('--out')+1]=str(D/'native')
for source in (SOURCE,CALLER):args+=['--source',source]
temp=D/'temp'
temp.mkdir()
(D/'command.json').write_text(json.dumps(args,indent=2)+'\n')
subprocess.run(args,cwd=W,env=dict(os.environ,TEMP=str(temp),TMP=str(temp)),check=True)
report=D/'native/compile-results.json'
r=json.loads(report.read_text())
assert r['passed']==r['requested']==2 and r['failed']==0
rows={row['source']:row for row in r['results']}
assert all(row['machine']==498 for row in rows.values())
asm=pathlib.Path(rows[SOURCE]['assembly']).read_text(errors='replace')
for method,offset in [('?IsSpeedMatching@AICar@BrnAI@@QBA_NXZ','1D66h'),('?GetSpeedMatchSpeed@AICar@BrnAI@@QBAMXZ','1D18h')]:
    body=asm.split('Begin code for function: '+method,1)[1].split('End code for function:',1)[0]
    assert '14B0h' in body and offset in body
msg=D/'message.txt'
msg.write_text('Restore native AI forced-speed diagnostic accessors\n\nCo-Authored-By: GPT-6 <noreply@openai.com>\n')
git('add','--',SOURCE)
assert git('diff','--cached','--name-only')==SOURCE
git('commit','--file',str(msg))
assert not git('status','--porcelain=v1')
result={'base':HEAD,'commit':git('rev-parse','HEAD'),'source':SOURCE,'source_sha256':sha(p),
        'artist':'0x82797168 driver load, 0x82797818 null gate, 0x8279781C flag, 0x82797830 speed',
        'report':str(report),'report_sha256':sha(report)}
(D/'DELIVERY.json').write_text(json.dumps(result,indent=2)+'\n')
print(result['commit'])
