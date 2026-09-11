// ============================================================================
// BrnTrafficEntityModule_wG_NearMissOutput.cpp
//
// TrafficEntityModule::GenerateNearMissOutput -- one of PreSceneUpdate's per-frame output
// producers. It copies the module's two near-miss collections wholesale into the
// traffic->race-car pre-scene interface, which is what carries them across the module
// boundary to the race-car module's post-scene drain.
//
// The collections are filled by ProcessNearbyTrafficSceneQueryResults (the post-physics drain
// of the player-centred sphere query). This publish is the only reader of them.
// ============================================================================

#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModuleIO.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/SharedIO/BrnTrafficToRaceCarInterface.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include <cstring>   // std::memcpy

namespace BrnTraffic
{

// ----------------------------------------------------------------------------
// TrafficEntityModule::GenerateNearMissOutput
//
// Two whole-collection copies, each preceded by a non-NULL assert on the source. The console
// folds both collection addresses to their member offsets and copies 132 bytes (the
// Array<NearMissData,16>, +0x228 in this module) and then 68 bytes (the Array<NearMissData,8>,
// +0x2AC) -- the whole Array including its trailing live-element count, which is what makes
// the consumer's GetLength() see this frame's candidates.
//
// The write getter is called once per collection, exactly as the console does: each call runs
// the output buffer's write-lock assert.
//
// ORDER NOTE: the collections this publishes were filled by the PREVIOUS frame's post-physics
// drain -- pre-scene runs ahead of post-physics. That one-frame lag is the console's own and
// is not corrected here.
// ----------------------------------------------------------------------------
void TrafficEntityModule::GenerateNearMissOutput( BrnTrafficIO::InputBuffer_PreScene*  lpInput,
                                                  BrnTrafficIO::OutputBuffer_PreScene* lpOutput )
{
    // The input buffer is the producer family's shared third argument; this member never reads
    // it (the console does not even save the register). Kept so the argument list matches its
    // siblings' and its caller's.
    (void)lpInput;

    typedef BrnTrafficIO::OutputBuffer_PreScene::TrafficToRaceCarInterface_PreScene
            TrafficInterface;

    // The copy sizes are the whole Array objects; pinned so a layout drift fails the compile
    // instead of publishing a short collection.
    static_assert( sizeof( TrafficInterface::NearMissTrafficCollection ) == 132,
                   "NearMissTrafficCollection must stay 16 * 8 + 4 bytes" );
    static_assert( sizeof( TrafficInterface::NearMissRaceCarCollection ) == 68,
                   "NearMissRaceCarCollection must stay 8 * 8 + 4 bytes" );

    {
        TrafficInterface::NearMissTrafficCollection* lpNearMissTrafficCollection =
                &mNearMissTrafficCollection;

        TrafficInterface* lpInterface = lpOutput->GetTrafficToRaceCarInterface_PreScene();

        CGS_ASSERT( lpNearMissTrafficCollection != 0, "lpNearMissTrafficCollection != NULL" );

        std::memcpy( lpInterface->GetNearMissTrafficCollection(),
                     lpNearMissTrafficCollection,
                     sizeof( TrafficInterface::NearMissTrafficCollection ) );
    }

    {
        TrafficInterface::NearMissRaceCarCollection* lpNearMissRaceCarCollection =
                &mNearMissRaceCarCollection;

        TrafficInterface* lpInterface = lpOutput->GetTrafficToRaceCarInterface_PreScene();

        CGS_ASSERT( lpNearMissRaceCarCollection != 0, "lpNearMissRaceCarCollection != NULL" );

        std::memcpy( lpInterface->GetNearMissRaceCarCollection(),
                     lpNearMissRaceCarCollection,
                     sizeof( TrafficInterface::NearMissRaceCarCollection ) );
    }
}

}   // namespace BrnTraffic
