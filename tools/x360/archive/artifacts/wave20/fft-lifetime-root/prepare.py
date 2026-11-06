from pathlib import Path
import os,json,re,subprocess
d=Path(__file__).resolve().parent
w=Path('D:/Reverse/BP-X360-Experimental-Worktrees/r1_vecfloat_repair_terra')
r26='3792b7d0f752405cbf160c530832492df09da639'
def git(*args):return subprocess.check_output(['git',*args],cwd=w).decode().rstrip()
assert not git('status','--porcelain=v1')
git('switch','-c','codex/experimental-x360-r27-fft-lifetime',r26)
def edit(rel,changes):
    p=w/rel
    data=p.read_bytes()
    eol=b'\r\n' if b'\r\n' in data else b'\n'
    for before,after in changes:
        a=before.encode().replace(b'\n',eol)
        b=after.encode().replace(b'\n',eol)
        assert data.count(a)==1,(p,before,data.count(a))
        data=data.replace(a,b)
    p.write_bytes(data)
header='vendor/renderware/include/rw/audio/core/FastFirEngine.h'
source='vendor/renderware/src/rw/audio/core/FastFirEngine.cpp'
fft='vendor/renderware/src/rw/audio/core/FFT.cpp'
edit(header,[
('''void  FFT_Alloc(s32 sizeLog2, char flag, void **outHandle);
void  FFT_Init(void *handle);
void *FFT_Free(void **handle);''','''struct FFTState;

struct FFTAllocation
{
    FFTState* mpContext;
    u32 muBytes;
};

s32 FFT_Alloc(s32 sizeLog2, char flag, FFTAllocation* pAllocation);
s32 FFT_Init(FFTState* pState);
s32 FFT_Free(FFTAllocation* pAllocation);'''),
('''    void *mpFft;              // +0x6C  the FFT context handle (FFT_Alloc/FFT_Free)
    s32   miField70;          // +0x70  (cleared by ctor; unused by the bodied members)''',
 '''    FFTAllocation mFft;      // +0x6C context, +0x70 allocation bytes'''),
])
edit(source,[
('''    mpFft = 0;    // +0x6C
    miField70 = 0;      // +0x70''','''    mFft.mpContext = 0;
    mFft.muBytes = 0;'''),
('''    if (mpFft)
        FFT_Free(&mpFft);''','''    if (mFft.mpContext)
        FFT_Free(&mFft);'''),
('''    void *result = self;

    if (self->mpBuffer)''','''    if (self->mpBuffer)'''),
('''    if (self->mpFft)
    {
        result = FFT_Free(&self->mpFft);
        self->mpFft = 0;
    }''','''    if (self->mFft.mpContext)
    {
        FFT_Free(&self->mFft);
        self->mFft.mpContext = 0;
    }'''),
('''    (void)result;
    return self;''','''    return self;'''),
('''    if (!self->mpFft)
    {
        FFT_Alloc(sizeLog2, 0, &self->mpFft);
        FFT_Init(self->mpFft);
    }''','''    if (!self->mFft.mpContext)
    {
        FFT_Alloc(sizeLog2, 0, &self->mFft);
        FFT_Init(self->mFft.mpContext);
    }'''),
])
p=w/source
data=p.read_bytes()
assert data.count(b'void *fft = self->mpFft;')==2
p.write_bytes(data.replace(b'void *fft = self->mpFft;',b'void *fft = self->mFft.mpContext;'))
with (w/fft).open('x',encoding='utf-8',newline='\n') as f:
    f.write("""#include "rw/audio/core/FastFirEngine.h"
#include "rw/audio/core/System.h"
#include "coreallocator/icoreallocator_interface.h"
#include <cmath>
#include <cstddef>

namespace rw { namespace audio { namespace core
{
    struct FFTState
    {
        s32 miSizeLog2;
        s32 miTwiddleCount;
        s32 miTransformSize;
        u32 muCosOffset;
        u32 muSinOffset;
        u32 muPermutationOffset;
    };

    CGS_STATIC_ASSERT(sizeof(FFTState) == 0x18, "ARTIST FFT context header");
    CGS_STATIC_ASSERT(sizeof(FFTAllocation) == 8, "ARTIST FFT allocation pair");
    CGS_STATIC_ASSERT(offsetof(FFTAllocation, muBytes) == 4, "ARTIST FFT allocation size");
    CGS_STATIC_ASSERT(offsetof(FastFirEngine, mFft) == 0x6C, "ARTIST FFT allocation owner");
    CGS_STATIC_ASSERT(offsetof(FastFirEngine, miInputChannels) == 0x74, "ARTIST FFT allocation end");

    extern "C" System* off_83271928;

    // ARTIST 0x82B6D550
    s32 FFT_Alloc(s32 sizeLog2, char flag, FFTAllocation* pAllocation)
    {
        const s32 log2 = flag ? sizeLog2 : sizeLog2 - 1;
        const s32 count = 1 << log2;
        const s32 twiddles = (count >> 1) + 1;
        const u32 tableBytes = twiddles * sizeof(f32);
        const u32 bytes = sizeof(FFTState) + 2 * tableBytes + (count + 2) * sizeof(u32);

        FFTState* state = static_cast<FFTState*>(
            off_83271928->mpAllocator->Alloc(bytes, "FFT instance", 1, 128, 0));
        if (state)
        {
            state->miSizeLog2 = log2;
            state->miTwiddleCount = twiddles;
            state->miTransformSize = count;
            state->muCosOffset = sizeof(FFTState);
            state->muSinOffset = state->muCosOffset + tableBytes;
            state->muPermutationOffset = state->muSinOffset + tableBytes;
            pAllocation->muBytes = bytes;
        }
        else
        {
            pAllocation->muBytes = 0;
        }
        pAllocation->mpContext = state;
        return state ? 0 : -1;
    }

    // ARTIST 0x82B68888
    s32 FFT_Init(FFTState* pState)
    {
        u8* bytes = reinterpret_cast<u8*>(pState);
        f32* cosine = reinterpret_cast<f32*>(bytes + pState->muCosOffset);
        f32* sine = reinterpret_cast<f32*>(bytes + pState->muSinOffset);
        u32* permutation = reinterpret_cast<u32*>(bytes + pState->muPermutationOffset);
        const s32 count = pState->miTransformSize;
        const f32 denominator = static_cast<f32>(count);
        const f32 pi = 3.1415927410125732f; // ARTIST 0x8214AED4

        for (s32 i = 0; i < pState->miTwiddleCount; ++i)
        {
            const f32 angle = (static_cast<f32>(i) * pi) / denominator;
            cosine[i] = static_cast<f32>(std::cos(static_cast<f64>(angle)));
            sine[i] = static_cast<f32>(std::sin(static_cast<f64>(angle)));
        }

        for (s32 i = 0; i < count; ++i)
        {
            u32 input = static_cast<u32>(i);
            u32 reversed = 0;
            for (s32 remaining = pState->miSizeLog2; remaining > 0; --remaining)
            {
                reversed = (reversed << 1) | (input & 1);
                input >>= 1;
            }
            permutation[i] = reversed;
        }
        permutation[count] = count;
        permutation[count + 1] = count + 1;
        return 0;
    }

    // ARTIST 0x82B6D618
    s32 FFT_Free(FFTAllocation* pAllocation)
    {
        off_83271928->mpAllocator->Free(pAllocation->mpContext, 0);
        pAllocation->mpContext = 0;
        pAllocation->muBytes = 0;
        return 0;
    }
} } }
""")
git('diff','--check')
prior=json.loads((d.parent/'r26-stream-lock-root/command.json').read_text())
args=prior[:prior.index('--source')]
args[args.index('--out')+1]=str(d/'native')
for path in (fft,source):args+=['--source',path]
(d/'command.json').write_text(json.dumps(args,indent=2)+'\n')
tmp=d/'temp'
tmp.mkdir()
subprocess.run(args,cwd=w,env=dict(os.environ,TEMP=str(tmp),TMP=str(tmp)),check=True)
