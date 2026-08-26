// =================================================================================================
// GameSource/World/AI/SharedIO/BrnAIModuleResultInterface.cpp   (aicar_reset wave, 2026-08-26)
//
// The two element constructors of BrnAI::AIModuleIO's RESULT side. Both are DWARF members
// (BrnAIModuleResultInterface.h) that the X360 build INLINED at every call site, so there is no
// standalone symbol to name them after -- the attestation is the field-store sequence the callers
// emit, quoted per function below.
//
// ⭐ ResetOnTrackResult::Construct is what ResetOnTrackManager::ProcessResetOnTrackRequest
// @0x82799D38 builds on its stack before AddEvent, on BOTH its arms. Landing it as a real function
// is what keeps the FAILURE record's five fields from being written by hand at the one call site
// that happens to need them today -- an uninitialised or partly-written 48-byte record on a shared
// queue is exactly how a plausible-looking wrong pose gets read as data downstream.
// =================================================================================================

#include "GameSource/World/AI/SharedIO/BrnAIModuleResultInterface.h"

namespace BrnAI
{
namespace AIModuleIO
{
    // ---------------------------------------------------------------------------------------------
    // ResetOnTrackResult::Construct -- inlined by the X360 at ProcessResetOnTrackRequest
    // @0x82799F5C (SUCCESS) and @0x82799FD0 (FAILURE). The store set at both sites is the same
    // five fields in the same order:
    //     [sp+50h] = position   (stvx128, 16B)      -> mResetPosition   @+0x00
    //     [sp+60h] = direction  (stvx128, 16B)      -> mResetDirection  @+0x10
    //     [sp+70h] = state      (stw, 0 or 1)       -> meState          @+0x20
    //     [sp+74h] = index      (stw)               -> meGlobalRaceCarIndex @+0x24
    //     [sp+78h] = speed      (stfs)              -> mfResetSpeed     @+0x28
    // ---------------------------------------------------------------------------------------------
    void ResetOnTrackResult::Construct(State leState,
                                       EGlobalRaceCarIndex leGlobalRaceCarIndex,
                                       f32 lfResetSpeed,
                                       Vector3 lResetPosition,
                                       Vector3 lResetDirection)
    {
        mResetPosition       = lResetPosition;
        mResetDirection      = lResetDirection;
        meState              = leState;
        meGlobalRaceCarIndex = leGlobalRaceCarIndex;
        mfResetSpeed         = lfResetSpeed;
    }

    // ---------------------------------------------------------------------------------------------
    // PlaceOnTrackRequest::Construct -- the sibling element of the second queue this interface
    // holds. Same shape minus the state word (the queue itself IS the "place this car" claim).
    // Its producer (BrnAI::AIModule's place-on-track publish) is not reconstructed on this build;
    // the constructor lands with its sibling so the element type is complete rather than half
    // declared -- and so the consumer side (RaceCarEntityModule::ProcessResetOnTrackResultQueue's
    // second loop, which IS landed) reads a type whose fields have a written home.
    // ---------------------------------------------------------------------------------------------
    void PlaceOnTrackRequest::Construct(EGlobalRaceCarIndex leGlobalRaceCarIndex,
                                        f32 lfResetSpeed,
                                        Vector3 lResetPosition,
                                        Vector3 lResetDirection)
    {
        mResetPosition       = lResetPosition;
        mResetDirection      = lResetDirection;
        meGlobalRaceCarIndex = leGlobalRaceCarIndex;
        mfResetSpeed         = lfResetSpeed;
    }
}
}
