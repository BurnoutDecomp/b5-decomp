#include "rw/audio/core/EaLayer3Dec.h"

#include "rw/audio/core/CMpegBase.h"
#include "rw/audio/core/PlugIn.h"

#include <cstddef>
#include <cstring>
#include <new>
#include <xtl.h>

#if defined(_XBOX) && defined(_MSC_VER)
// The XDK size setting reproduces AllocateHybrid at 0x82B8C200 and the
// MPEG destructor chain at 0x82B93B08, 0x82B94FB0, 0x82B95020 and 0x82B95348.
#pragma optimize("s", on)
#endif

namespace rw
{
namespace audio
{
namespace core
{

struct HuffTable
{
    u16 tableSize;
    s16 *pEntries;
};

namespace
{

#include "EaLayer3Tables.inl"

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
    virtual ~CMpegLayer3Base();
    s32 AllocateHybrid(s32 channels);

private:
    void HuffmanTableSetup();

public:
    HuffTable mHuffTables[32];                   // 0x50
    u8 mGranuleAndScaleState[0x15C];    // 0x150
    const f32 *mpTwoToNegativeQuarterPower;       // 0x2AC
    const f32 *mpLoadedTwoToNegativeQuarterPower; // 0x2B0
    s16 *mpLoadedTableSelect[3];                  // 0x2B4
    f32 *mpHybridHistory;              // 0x2C0
    f32 *mpHybridWork;                 // 0x2C4
    f32 *mapStereoWork[3];             // 0x2C8
    u8 mDecodeState2D4[4];              // 0x2D4
    u16 mTwoToNegativeQuarterPowerTableSize;      // 0x2D8
};

class EALayer3Core : public CMpegLayer3Base
{
public:
    explicit EALayer3Core(s32 channels);
    virtual ~EALayer3Core() {}
};

CGS_STATIC_ASSERT(sizeof(HuffTable) == 8, "ARTIST Huffman descriptor stride");
CGS_STATIC_ASSERT(sizeof(CMpegBase) == 0x50, "ARTIST MPEG base layout");
CGS_STATIC_ASSERT(offsetof(CMpegBase, mOpened) == 0x17, "ARTIST MPEG open flag");
CGS_STATIC_ASSERT(offsetof(CMpegBase, mResetSynthRequired) == 0x34, "ARTIST MPEG synth reset");
CGS_STATIC_ASSERT(offsetof(CMpegLayer3Base, mHuffTables) == 0x50, "ARTIST Huffman descriptors");
CGS_STATIC_ASSERT(offsetof(CMpegLayer3Base, mpTwoToNegativeQuarterPower) == 0x2AC, "ARTIST global gain");
CGS_STATIC_ASSERT(offsetof(CMpegLayer3Base, mpHybridHistory) == 0x2C0, "ARTIST hybrid history");
CGS_STATIC_ASSERT(offsetof(CMpegLayer3Base, mTwoToNegativeQuarterPowerTableSize) == 0x2D8, "ARTIST Huffman scale");
CGS_STATIC_ASSERT(sizeof(EALayer3Core) == 0x2DC, "ARTIST core instance size");
CGS_STATIC_ASSERT(sizeof(Decoder) == 0x34, "ARTIST decoder base size");
CGS_STATIC_ASSERT(sizeof(EaLayer3DecBase) == 0x58, "ARTIST core table offset");

CMpegBase::CMpegBase()
{
    mResetSynthRequired = 0;
    mOpened = 0;
    muHeaderWord = 0;
    mpPolySynthHistory = 0;
    mpPolySynthWork = 0;
}

// ARTIST 0x82B93A98
CMpegLayer3Base::CMpegLayer3Base()
{
    mpTwoToNegativeQuarterPower = kGlobalGain;
    mTwoToNegativeQuarterPowerTableSize = sizeof(kGlobalGain);
    mpLoadedTwoToNegativeQuarterPower = 0;
    HuffmanTableSetup();
    mpLoadedTableSelect[0] = 0;
    mpLoadedTableSelect[1] = 0;
    mpLoadedTableSelect[2] = 0;
}

// ARTIST 0x82B8C068
void CMpegLayer3Base::HuffmanTableSetup()
{
#define SET_TABLE(slot, table) \
    mHuffTables[slot].tableSize = sizeof(kHuffman##table); \
    mHuffTables[slot].pEntries = kHuffman##table
    mHuffTables[0].tableSize = 0;
    mHuffTables[0].pEntries = 0;
    SET_TABLE(1, 1);
    SET_TABLE(2, 2);
    SET_TABLE(3, 3);
    mHuffTables[4].tableSize = 0;
    mHuffTables[4].pEntries = 0;
    SET_TABLE(5, 5);
    SET_TABLE(6, 6);
    SET_TABLE(7, 7);
    SET_TABLE(8, 8);
    SET_TABLE(9, 9);
    SET_TABLE(10, 10);
    SET_TABLE(11, 11);
    SET_TABLE(12, 12);
    SET_TABLE(13, 13);
    mHuffTables[14].tableSize = 0;
    mHuffTables[14].pEntries = 0;
    SET_TABLE(15, 15);
    SET_TABLE(16, 16);
    SET_TABLE(17, 16);
    SET_TABLE(18, 16);
    SET_TABLE(19, 16);
    SET_TABLE(20, 16);
    SET_TABLE(21, 16);
    SET_TABLE(22, 16);
    SET_TABLE(23, 16);
    SET_TABLE(24, 24);
    SET_TABLE(25, 24);
    SET_TABLE(26, 24);
    SET_TABLE(27, 24);
    SET_TABLE(28, 24);
    SET_TABLE(29, 24);
    SET_TABLE(30, 24);
    SET_TABLE(31, 24);
#undef SET_TABLE
}

// ARTIST 0x82B8C200
s32 CMpegLayer3Base::AllocateHybrid(s32 channels)
{
    const s32 bytes = channels * 2304;
    mpHybridHistory = static_cast<f32 *>(System::GetInstance()->Alloc(bytes, "MP3HybridHistory", 16, 0));
    if (!mpHybridHistory)
        return -1;
    XMemSet(mpHybridHistory, 0, bytes);
    return 0;
}

// ARTIST inlines the base cleanup into the derived destructor and its own deleting thunk.
CMpegBase::~CMpegBase()
{
    if (mpPolySynthHistory)
        System::GetInstance()->Free(mpPolySynthHistory, 0);
}

// ARTIST 0x82B93B08
CMpegLayer3Base::~CMpegLayer3Base()
{
    if (mpHybridHistory)
        System::GetInstance()->Free(mpHybridHistory, 0);
}

// ARTIST 0x82B94F50
EALayer3Core::EALayer3Core(s32 channels)
{
    AllocateSynth(channels);
    AllocateHybrid(channels);
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
        mpSystemUseGetSystemAccessor->Alloc(bytes, "EALayer3Core Instances", kCoreAlignment, 0));

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
