// ============================================================================
// GameSource/Effects/Particles/Native/BrnTrailSystem.cpp
//
// Out-of-line bodies for BrnParticle::Native::EmitterArray::AddEntry and
// ::RemoveEntry(int32_t), reconstructed store-for-store from the X360 ARTIST asm
// (AddEntry @ 0x82277D50, RemoveEntry @ 0x82277DC8). mnCurrentSize lives at +0x180
// (== 96 * sizeof(pointer)); the array is dense (RemoveEntry swap-removes the last
// element into the freed slot).
// ============================================================================

#include "GameSource/Effects/Particles/Native/BrnTrailSystem.h"
#include "GameShared/GameClasses/Core/CgsAssert.h" // CGS_ASSERT

namespace BrnParticle
{
namespace Native
{
    void EmitterArray::AddEntry( TrailEmitter* lpEmitter )
    {
        CGS_ASSERT( mnCurrentSize < KN_TRAIL_EMITTER_POOL_SIZE,
                    "mnCurrentSize < knTrailEmitterPoolSize" );

        mapEmitters[mnCurrentSize] = lpEmitter;
        ++mnCurrentSize;
    }

    void EmitterArray::RemoveEntry( s32 liIndex )
    {
        CGS_ASSERT( liIndex < mnCurrentSize, "Element not found in array" );

        const s32 liLastIndex = mnCurrentSize - 1;
        if ( liIndex == liLastIndex )
        {
            mapEmitters[liIndex] = nullptr;
        }
        else
        {
            mapEmitters[liIndex]            = mapEmitters[liLastIndex];
            mapEmitters[mnCurrentSize - 1]  = nullptr;
        }
        --mnCurrentSize;
    }
}
}
