#pragma once

#include "rw/audio/core/Decoder.h"

namespace rw
{
namespace audio
{
namespace core
{

class EALayer3Core;

// The fixed ARTIST decoder header is followed by an inline core-pointer table.
// Each core decodes a pair of channels, with a mono core for an odd final channel.
class EaLayer3DecBase : public Decoder
{
public:
    s32 CreateInstance(u8 version);

private:
    EALayer3Core *mpLoadedEALayer3Core; // 0x34
    const u8 *mpEncodedSample;        // 0x38
    EALayer3Core **mppEaLayer3Core;    // 0x3C
    s32 mRemainingSamples;            // 0x40
    s32 mTotalChannels;               // 0x44
    s32 mNumEaLayer3CoreInstances;    // 0x48
    s32 mLatency;                     // 0x4C
    s32 mSkipSamples;                 // 0x50
    u8 mVersion;                      // 0x54
    u8 mNewFeed;                      // 0x55
};

class EaLayer31Dec : public EaLayer3DecBase
{
public:
    static s32 CreateInstanceEvent(EaLayer31Dec *self);
};

class EaLayer32PcmDec : public EaLayer3DecBase
{
public:
    static s32 CreateInstanceEvent(EaLayer32PcmDec *self);
};

} // namespace core
} // namespace audio
} // namespace rw
