#include "rw/audio/core/EaLayer3Dec.h"

#include "rw/audio/core/CMpegBase.h"
#include "rw/audio/core/PlugIn.h"

#include <cstddef>
#include <cstring>
#include <new>

namespace rw
{
namespace audio
{
namespace core
{

extern "C" System *off_83271928;

namespace
{

struct HuffmanTable
{
    u16 sizeBytes;
    const s16 *pCodes;
};

#include "EaLayer3Tables.inl"

#define HUFFMAN(index) { sizeof(kHuffman##index), kHuffman##index }
const HuffmanTable kHuffmanTables[] =
{
    { 0, 0 }, HUFFMAN(1), HUFFMAN(2), HUFFMAN(3),
    { 0, 0 }, HUFFMAN(5), HUFFMAN(6), HUFFMAN(7),
    HUFFMAN(8), HUFFMAN(9), HUFFMAN(10), HUFFMAN(11),
    HUFFMAN(12), HUFFMAN(13), { 0, 0 }, HUFFMAN(15),
    HUFFMAN(16), HUFFMAN(16), HUFFMAN(16), HUFFMAN(16),
    HUFFMAN(16), HUFFMAN(16), HUFFMAN(16), HUFFMAN(16),
    HUFFMAN(24), HUFFMAN(24), HUFFMAN(24), HUFFMAN(24),
    HUFFMAN(24), HUFFMAN(24), HUFFMAN(24), HUFFMAN(24)
};
#undef HUFFMAN

const usize kCoreAlignment = 16;

usize AlignCore(usize address)
{
    return (address + kCoreAlignment - 1) & ~(kCoreAlignment - 1);
}

} // namespace

class CMpegLayer3Base : public CMpegBase
{
public:
    CMpegLayer3Base();
    void DestroyLayer3();
    s32 AllocateHybrid(s32 channels);
    static void *VectorDeletingDestructor(CMpegLayer3Base *self, char flags);

    HuffmanTable maHuffman[32];        // 0x50
    u8 mGranuleAndScaleState[0x15C];    // 0x150
    const f32 *mpGlobalGain;            // 0x2AC
    const f32 *mpCurrentGain;           // 0x2B0
    const s16 *mapActiveHuffman[3];     // 0x2B4
    f32 *mpHybridHistory;              // 0x2C0
    f32 *mpHybridWork;                 // 0x2C4
    f32 *mapStereoWork[3];             // 0x2C8
    u8 mDecodeState2D4[4];              // 0x2D4
    u16 muHuffmanScale;                // 0x2D8
};

class EALayer3Core : public CMpegLayer3Base
{
public:
    explicit EALayer3Core(s32 channels);
    static void *VectorDeletingDestructor(EALayer3Core *self, char flags);
};

namespace
{

void *(*kLayer3BaseVtable[])(CMpegLayer3Base *, char) =
{
    &CMpegLayer3Base::VectorDeletingDestructor
};

void *(*kEaLayer3CoreVtable[])(EALayer3Core *, char) =
{
    &EALayer3Core::VectorDeletingDestructor
};

} // namespace

CGS_STATIC_ASSERT(sizeof(HuffmanTable) == 8, "ARTIST Huffman descriptor stride");
CGS_STATIC_ASSERT(sizeof(kHuffmanTables) == 32 * sizeof(HuffmanTable), "ARTIST Huffman descriptor count");
CGS_STATIC_ASSERT(sizeof(CMpegBase) == 0x50, "ARTIST MPEG base layout");
CGS_STATIC_ASSERT(offsetof(CMpegLayer3Base, maHuffman) == 0x50, "ARTIST Huffman descriptors");
CGS_STATIC_ASSERT(offsetof(CMpegLayer3Base, mpGlobalGain) == 0x2AC, "ARTIST global gain");
CGS_STATIC_ASSERT(offsetof(CMpegLayer3Base, mpHybridHistory) == 0x2C0, "ARTIST hybrid history");
CGS_STATIC_ASSERT(offsetof(CMpegLayer3Base, muHuffmanScale) == 0x2D8, "ARTIST Huffman scale");
CGS_STATIC_ASSERT(sizeof(EALayer3Core) == 0x2DC, "ARTIST core instance size");
CGS_STATIC_ASSERT(sizeof(Decoder) == 0x34, "ARTIST decoder base size");
CGS_STATIC_ASSERT(sizeof(EaLayer3DecBase) == 0x58, "ARTIST core table offset");

// ARTIST 0x82B93A98 and 0x82B8C068
CMpegLayer3Base::CMpegLayer3Base()
{
    std::memset(mPad34, 0, sizeof(mPad34));
    mPad17 = 0;
    muHeaderWord = 0;
    mpPolySynthHistory = 0;
    mpPolySynthWork = 0;
    mpVTable = kLayer3BaseVtable;
    mpGlobalGain = kGlobalGain;
    muHuffmanScale = 1024;
    mpCurrentGain = 0;
    for (s32 table = 0; table < 32; ++table)
    {
        maHuffman[table].sizeBytes = kHuffmanTables[table].sizeBytes;
        maHuffman[table].pCodes = kHuffmanTables[table].pCodes;
    }
    mapActiveHuffman[0] = 0;
    mapActiveHuffman[1] = 0;
    mapActiveHuffman[2] = 0;
}

// ARTIST 0x82B8C200
s32 CMpegLayer3Base::AllocateHybrid(s32 channels)
{
    const u32 bytes = static_cast<u32>(channels) * 2304;
    mpHybridHistory = static_cast<f32 *>(System::Alloc(off_83271928, bytes, "MP3HybridHistory", 16, 0));
    if (!mpHybridHistory)
        return -1;
    std::memset(mpHybridHistory, 0, bytes);
    return 0;
}

// ARTIST 0x82B93B08
void CMpegLayer3Base::DestroyLayer3()
{
    mpVTable = kLayer3BaseVtable;
    if (mpHybridHistory)
        System::Free(off_83271928, mpHybridHistory, 0);
    CMpegBase::VectorDeletingDestructor(this, 0);
}

// ARTIST 0x82B95020
void *CMpegLayer3Base::VectorDeletingDestructor(CMpegLayer3Base *self, char flags)
{
    self->DestroyLayer3();
    if ((flags & 1) != 0)
        ::operator delete(self);
    return self;
}

// ARTIST 0x82B94F50
EALayer3Core::EALayer3Core(s32 channels)
{
    mpVTable = kEaLayer3CoreVtable;
    AllocateSynth(channels);
    AllocateHybrid(channels);
}

// ARTIST 0x82B95348
void *EALayer3Core::VectorDeletingDestructor(EALayer3Core *self, char flags)
{
    self->mpVTable = kEaLayer3CoreVtable;
    self->DestroyLayer3();
    if ((flags & 1) != 0)
        ::operator delete(self);
    return self;
}

// ARTIST 0x82B953A0
s32 EaLayer3DecBase::CreateInstance(u8 version)
{
    mVersion = version;
    mRemainingSamples = 0;
    mpEncodedSample = 0;
    mTotalChannels = mucChannelCount;
    mNumEaLayer3CoreInstances = (mTotalChannels + 1) / 2;
    mppEaLayer3Core = reinterpret_cast<EALayer3Core **>(this + 1);

    const usize stride = AlignCore(sizeof(EALayer3Core));
    const usize bytes = mNumEaLayer3CoreInstances == 0
        ? 0 : (mNumEaLayer3CoreInstances - 1) * stride + sizeof(EALayer3Core);
    u8 *storage = static_cast<u8 *>(
        System::Alloc(mpSystemUseGetSystemAccessor, bytes, "EALayer3Core Instances", kCoreAlignment, 0));

    for (s32 core = 0; core < mNumEaLayer3CoreInstances; ++core)
    {
        u8 *aligned = reinterpret_cast<u8 *>(AlignCore(reinterpret_cast<usize>(storage)));
        storage = aligned + sizeof(EALayer3Core);
        mppEaLayer3Core[core] = reinterpret_cast<EALayer3Core *>(aligned);
        std::memset(aligned, 0, sizeof(EALayer3Core));
        const s32 channels = core == mTotalChannels / 2 ? 1 : 2;
        mppEaLayer3Core[core] = aligned ? ::new (aligned) EALayer3Core(channels) : 0;
    }

    mNewFeed = 0;
    mSkipSamples = 0;
    mLatency = 1105;
    return 1;
}

} // namespace core
} // namespace audio
} // namespace rw
