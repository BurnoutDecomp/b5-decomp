// ============================================================================
// BrnDirector::ICEWrapper::Construct / ::Destruct, split out of
// SDKs/Packages/ICE/ICEWrapper.cpp: that TU's other bodies (PlayMovie, Update,
// UpdateAction, ...) need the two dev-tools control->action converter tables and
// the ICE movie-player half, none of which has a definition in the tree.
// DELETE-WHEN: ICEWrapper.cpp can mount -- then move these bodies back into it.
// MainDirector embeds the wrapper by value and Constructs it at boot, before the
// debug log exists: nothing here may log.
// ============================================================================

#include "GameSource/Director/BrnDirectorICEWrapper.h"

namespace BrnDirector
{

// ----------------------------------------------------------------------------
// Construct
//
// Build the runtime state: construct the vehicle ref and seed its bound-state fields
// (the inlined VehicleRef::Set(E_PLAYER_CAR, ..)), clear the dev-tools action queue,
// zero the two ICE load-state scalars, construct the ICE camera, drop the sim-time
// scale. The heaps / manager / mover are built by the constructor.
//
// Member map (provenance): VehicleRef::Construct(&mVehicleRef @+0x120F0) then
// +0x120F0=0, +0x120FC=1, +0x120F8=0, +0x120F4=-1; mActionQueue's miLength at +0x11BC8
// is zeroed; +0x120E8 then +0x120E4 are the two load-state scalars; Camera::Construct
// on +0x11D70 (mICECamera's embedded director camera); mfTimeScale at +0x9B20. No
// write to the manager playback flag, the current-movie id or the accept-input gate.
// ----------------------------------------------------------------------------
void ICEWrapper::Construct()
{
    mVehicleRef.Construct();

    // VehicleRef::Set(E_PLAYER_CAR, ..) inlined -- the ref is bound to the player car.
    mVehicleRef.meType         = VehicleRef::E_PLAYER_CAR;
    mVehicleRef.mbSet          = true;
    mVehicleRef.muRef          = 0;
    mVehicleRef.miRaceCarIndex = -1;

    // Make the dev-tools action queue usable (off the unconstructed sentinel the
    // constructor seeds).
    mActionQueue.Clear();

    // Reset the two ICE load-state scalars; miICELoadStateB is the stage word
    // ICEWrapper::Prepare switches on, so a fresh wrapper enters Prepare at stage 0.
    miICELoadStateB = 0;
    miICELoadStateA = 0;

    // The ICE camera's bring-up is inlined at this call site to exactly its embedded
    // director camera's Construct -- the only store the recorded call makes.
    mICECamera.GetCamera()->Construct();

    // No sim-time scale until the first Update.
    mfTimeScale = 0.0f;
}

// ----------------------------------------------------------------------------
// Destruct
//
// Tear down the runtime: destruct the ICE manager (+0xA40), then the base heap --
// the CgsMemory::HeapMalloc this wrapper IS.
// ----------------------------------------------------------------------------
void ICEWrapper::Destruct()
{
    mICEManager.Destruct();
    HeapMalloc::Destruct();
}

} // namespace BrnDirector
