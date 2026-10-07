#pragma once

#include "types.hpp"
#include "GameSource/Replays/Stream/BrnReplayStreamHeader.h"
#include "GameSource/Replays/Stream/BrnReplayFrunkHeader.h"
#include "GameSource/Replays/BrnReplayShared.h"

namespace BrnReplays
{
    class DiskReadStream;
    class ReplayModule;
    // ARTIST's serialiser arrays have eleven slots. Their pointers widen on
    // the host; caller/module access is by the canonical named members.
    struct FrunkReadResult
    {
        f32 mfTime;
        u16 mxFlags;
        FrunkHeader mHeader;
        s32 maiSizes[E_ID_COUNT];
        void* mapBuffers[E_ID_COUNT];

        FrunkReadResult();
    };

    // DecFIGS BrnReplayReadStream.h:64; ARTIST 8265C1F0/8265E7F0/8264D060.
    // The intermediate bytes are class-static in both the DWARF and ARTIST
    // (Read/ReadCurrentFrunk address 82FBA380); they are not an object buffer.
    class ReadStream
    {
    public:
        static const s32 KI_INTERMEDIATEBUFFERSIZE = 131072;

        void Construct();
        s64 StartNewStream(void* lpHeaderBuffer, s32 liHeaderBufferSize,
                           DiskReadStream* lpReadStream);
        bool ReadCurrentFrunk(FrunkReadResult* lpInOutResult);
        bool MoveToNextFrunk();

    private:
        friend class ReplayModule;
        void Read(void* lpDest, s32 liSize);

        StreamHeader* mpStreamHeader;
        s32 miCurrentFrunk;
        s32 miStallCount;
        s32 miCurrentFrunkPos;
        DiskReadStream* mpStream;
        s32 miFilePosition;
        static char mpcIntermediateBuffer[KI_INTERMEDIATEBUFFERSIZE];
    };
}
