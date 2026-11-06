import hashlib,json,os,pathlib,struct,subprocess,sys
D=pathlib.Path(__file__).resolve().parent
W=pathlib.Path('D:/Reverse/BP-X360-Experimental-Worktrees/r1_vecfloat_repair_terra')
HEAD='5c09399b5c17d424544248298b27b9939fd28d6f'
P='src/GameShared/GameClasses/Graphics/MoviePlayer/'
H=P+'CSeqDissolveDetector.h'
C=P+'CSeqDissolveDetector.cpp'
A=P+'CArrayLookahead.cpp'
sys.path.insert(0,'D:/Reverse/BP-Decomp_Workflow/tools/re')
from x360rd import rd,u32
def git(*a):return subprocess.check_output(['git',*a],cwd=W).decode().strip()
def sha(p):return hashlib.sha256(pathlib.Path(p).read_bytes()).hexdigest()
def update(rel,fn):
    file=W/rel
    b=file.read_bytes()
    eol='\r\n' if b'\r\n' in b else '\n'
    file.write_bytes(fn(b.decode().replace('\r\n','\n')).replace('\n',eol).encode())
def one(s,a,b):
    assert s.count(a)==1
    return s.replace(a,b)
assert git('rev-parse','HEAD')==HEAD and not git('status','--porcelain=v1')
assert u32(0x82815570)==0x7c8a2378 and u32(0x82F264A8)==4
assert rd(0x8210ef70,16).hex()=='4036cccccccccccdc036cccccccccccd'
assert u32(0x82001d9c)==0x40000000 and u32(0x82006530)==0x43160000
names={'mfReset0C':'mfMaxRecoveryDelta','mfReset10':'mfMinVarianceDelta',
       'miReset14':'miNonIncreasingCount','miReset18':'miConsecutiveNonIncreasingCount',
       'miReset1C':'miMeanSignChanges','miReset20':'miFallingSamples','miReset24':'miRisingSamples'}
def rename(s):
    for old,new in names.items():
        s=s.replace(old,new)
    return s
def header(s):
    s=s[s.index('#pragma once'):]
    a=s.index('    // --- engine methods')
    z=s.index('private:',a)
    s=s[:a]+"""    enum { KI_FEATURE_HISTORY = 21 };

    CSeqDissolveDetector* Reset();
    CSeqDissolveDetector* Detect(f32 lfPrevVarDelta, f32 lfCurVarDelta,
                                 f32 lfPrevMeanDelta, f32 lfCurMeanDelta);
    CSeqDissolveDetector* ComputeLocalFeature();

    s32 miDissolveDetected;
    s32 miDissolveLength;
    s32 miMaxDissolveLength;
    f32 mfMaxRecoveryDelta;
    f32 mfMinVarianceDelta;
    s32 miNonIncreasingCount;
    s32 miConsecutiveNonIncreasingCount;
    s32 miMeanSignChanges;
    s32 miFallingSamples;
    s32 miRisingSamples;

    f32 mafFeatureHistory[KI_FEATURE_HISTORY];
    s32 miFeatureSampleCount;
    f32 mfLocalFeature;
    s32 miFeatureIndex;

"""+s[z:]
    s=rename(s)
    a='    CGS_STATIC_ASSERT((offsetof(CSeqDissolveDetector, maState)             == 0x28), "CSeqDissolveDetector::maState");'
    s=one(s,a,"""    CGS_STATIC_ASSERT((offsetof(CSeqDissolveDetector, mafFeatureHistory)   == 0x28), "dissolve feature history");
    CGS_STATIC_ASSERT((offsetof(CSeqDissolveDetector, miFeatureSampleCount)== 0x7C), "dissolve sample count");
    CGS_STATIC_ASSERT((offsetof(CSeqDissolveDetector, mfLocalFeature)     == 0x80), "dissolve local feature");
    CGS_STATIC_ASSERT((offsetof(CSeqDissolveDetector, miFeatureIndex)     == 0x84), "dissolve feature index");""")
    lines=[]
    for line in s.splitlines():
        if 'offsetof(CSeqDissolveDetector,' in line:
            a,b=line.split(' == ',1) if ' == ' in line else line.split('== ',1)
            line=a.rstrip()+' == '+b
        lines.append(line)
    return '\n'.join(lines)+'\n'
def caller(s):
    s=rename(s)
    a=s.index('//   Reset/Detect/ComputeLocalFeature engine bodies')
    z=s.index('\n',a)
    if 'separate TU' in s[a:z]:
        next_line=s.index('\n',z+1)
        if s[z+1:next_line].startswith('//'):
            s=s[:a]+'//   The detector engine methods live in CSeqDissolveDetector.cpp.'+s[next_line:]
    return s
update(H,header)
update(A,caller)
with (W/C).open('x',encoding='utf-8',newline='\n') as f:
    f.write("""#include "GameShared/GameClasses/Graphics/MoviePlayer/CSeqDissolveDetector.h"

#include <cmath>

namespace
{
    const double KD_RECOVERY_THRESHOLD = 22.8;
    const double KD_FALL_THRESHOLD = -22.8;
    const f32 KF_VARIANCE_STEP = 2.0f;
    const f32 KF_REJECT_RATE = 150.0f;
}

// ARTIST 0x82A04AF8
CSeqDissolveDetector* CSeqDissolveDetector::ComputeLocalFeature()
{
    mfLocalFeature = mafFeatureHistory[(miFeatureIndex + 1) % KI_FEATURE_HISTORY];
    for (s32 liOffset = 2; liOffset <= 15; ++liOffset)
    {
        const f32 lfFeature = mafFeatureHistory[(miFeatureIndex + liOffset) % KI_FEATURE_HISTORY];
        if (mfLocalFeature < lfFeature)
            mfLocalFeature = lfFeature;
    }
    return this;
}

// ARTIST 0x82A05278
CSeqDissolveDetector* CSeqDissolveDetector::Reset()
{
    if (miDissolveLength > 0)
        ComputeLocalFeature();

    mfMaxRecoveryDelta = 0.0f;
    miDissolveDetected = 0;
    mfMinVarianceDelta = 0.0f;
    miDissolveLength = 0;
    miNonIncreasingCount = 0;
    miConsecutiveNonIncreasingCount = 0;
    miMeanSignChanges = 0;
    miFallingSamples = 0;
    miRisingSamples = 0;
    return this;
}

// ARTIST 0x82A052D8
CSeqDissolveDetector* CSeqDissolveDetector::Detect(f32 lfPrevVarDelta, f32 lfCurVarDelta,
                                                  f32 lfPrevMeanDelta, f32 lfCurMeanDelta)
{
    if (miMaxDissolveLength <= 0)
        return this;

    const s32 liSample = miFeatureSampleCount;
    miFeatureSampleCount = static_cast<s32>(static_cast<u32>(liSample) + 1u);
    miFeatureIndex = liSample % KI_FEATURE_HISTORY;
    mafFeatureHistory[miFeatureIndex] = std::fabs(lfCurVarDelta);
    if (miDissolveLength == 0)
        ComputeLocalFeature();

    if (miDissolveLength == 0)
    {
        if (lfCurVarDelta < KD_FALL_THRESHOLD)
        {
            mfMinVarianceDelta = lfCurVarDelta;
            miDissolveLength = 1;
            miFallingSamples = 1;
        }
        return this;
    }

    ++miDissolveLength;
    if (lfCurVarDelta < mfMinVarianceDelta)
    {
        if (miDissolveLength == 2)
        {
            if (lfPrevVarDelta - lfCurVarDelta > KF_VARIANCE_STEP)
                ++miFallingSamples;
        }
        else
        {
            miFallingSamples = 1;
        }

        mfMinVarianceDelta = lfCurVarDelta;
        miDissolveLength = 1;
        miNonIncreasingCount = 0;
        miConsecutiveNonIncreasingCount = 0;
        miMeanSignChanges = 0;
        miRisingSamples = 0;
        mfMaxRecoveryDelta = 0.0f;
    }
    else
    {
        if ((lfPrevMeanDelta > 0.0f && lfCurMeanDelta < 0.0f) ||
            (lfPrevMeanDelta < 0.0f && lfCurMeanDelta > 0.0f))
            ++miMeanSignChanges;
        if (miMeanSignChanges > 0)
            return Reset();

        if (!(lfCurVarDelta > lfPrevVarDelta))
        {
            ++miNonIncreasingCount;
            ++miConsecutiveNonIncreasingCount;
        }
        else
        {
            if (miNonIncreasingCount > 3)
                return Reset();
            if (mfMaxRecoveryDelta < lfCurVarDelta)
                mfMaxRecoveryDelta = lfCurVarDelta;
            miConsecutiveNonIncreasingCount = 0;
            if (lfCurVarDelta - lfPrevVarDelta > KF_VARIANCE_STEP)
                ++miRisingSamples;
        }
    }

    const f32 lfFall = -mfMinVarianceDelta;
    f32 lfFeature = mfMaxRecoveryDelta;
    const bool lbRejectFall = lfFeature < lfFall && miFallingSamples < 4 &&
        -(mfMinVarianceDelta / static_cast<f32>(miFallingSamples)) > KF_REJECT_RATE;
    const bool lbRejectRise = miRisingSamples < 4 &&
        (lfFeature - mfMinVarianceDelta) / static_cast<f32>(miRisingSamples) > KF_REJECT_RATE;

    if (miDissolveLength >= miMaxDissolveLength)
    {
        if (lfFeature < KD_RECOVERY_THRESHOLD)
            return Reset();
    }
    else
    {
        if (miConsecutiveNonIncreasingCount < 2)
            return this;
        if (miDissolveLength < 6 || !(lfFeature > KD_RECOVERY_THRESHOLD))
            return Reset();
    }

    if (lfFall < lfFeature)
        lfFeature = lfFall;
    if (lfFeature < mfLocalFeature || lbRejectFall || lbRejectRise)
        return Reset();

    miDissolveDetected = 1;
    return this;
}
""")
git('diff','--check','--',H,A)
args=json.loads((D.parent/'r26-root-providers/command.json').read_text())
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
assert all(row['machine']==498 for row in rows.values())
source=pathlib.Path(rows[C]['assembly']).read_text(errors='replace')
caller=pathlib.Path(rows[A]['assembly']).read_text(errors='replace')
for sym in ('?Reset@CSeqDissolveDetector@@QAAPAV1@XZ','?Detect@CSeqDissolveDetector@@QAAPAV1@MMMM@Z','?ComputeLocalFeature@CSeqDissolveDetector@@QAAPAV1@XZ'):
    assert 'Begin code for function: '+sym in source and sym in caller
assert '7Ch(' in source and '84h(' in source and '80h(' in source
msg=D/'message.txt'
msg.write_text('Recover the native movie dissolve detector\n\nCo-Authored-By: GPT-6 <noreply@openai.com>\n')
git('add','--',A,H,C)
assert set(git('diff','--cached','--name-only').splitlines())=={A,H,C}
git('commit','--file',str(msg))
assert not git('status','--porcelain=v1')
result={'base':HEAD,'commit':git('rev-parse','HEAD'),'paths':{p:sha(W/p) for p in (A,H,C)},
        'artist':{'ComputeLocalFeature':'0x82A04AF8','Reset':'0x82A05278','Detect':'0x82A052D8'},
        'layout':{'history':'0x28, 21 f32','sample_count':'0x7C','local_feature':'0x80','history_index':'0x84','size':'0x88'},
        'report':str(report),'report_sha256':sha(report)}
(D/'DELIVERY.json').write_text(json.dumps(result,indent=2)+'\n')
print(result['commit'])
