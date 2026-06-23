#include "GameSource/World/AI/SharedIO/BrnAIModuleRequestInterface.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// BrnAI::AIModuleIO::ResetOnTrackRequest::Construct @0x822A0438.
//
// Initialises a queued reset-on-track request. The X360 build first range-checks the race
// car index (asserting 0 <= index < E_GLOBAL_RACE_CAR_INDEX_COUNT == 0x23; on failure it
// builds the "Invalid race car index: <n>\n" message through the dev StrStream and fires
// the assert -- all folded into CGS_ASSERT here), then stores the fields at their observed
// displacements: index @+0x0, resetSpeed @+0x4, resetDistance @+0x8, resetType @+0xC. The
// two speed/distance args arrive in fp registers and are narrowed to f32 by the stfs stores.
namespace BrnAI
{
    namespace AIModuleIO
    {
        void ResetOnTrackRequest::Construct(EGlobalRaceCarIndex leGlobalRaceCarIndex,
                                            f32 lfResetSpeed,
                                            f32 lfResetDistance,
                                            BrnAI::EResetType leResetType)
        {
            CGS_ASSERT(leGlobalRaceCarIndex >= 0 &&
                           leGlobalRaceCarIndex < E_GLOBAL_RACE_CAR_INDEX_COUNT,
                       "Invalid race car index: ");

            meGlobalRaceCarIndex = leGlobalRaceCarIndex;   // +0x0
            mfResetSpeed         = lfResetSpeed;            // +0x4
            mfResetDistance      = lfResetDistance;         // +0x8
            meResetType          = leResetType;             // +0xC
        }
    }
}
