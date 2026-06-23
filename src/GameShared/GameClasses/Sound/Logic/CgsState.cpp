// ============================================================================
// CgsState.cpp
//
// Definition home for the CgsSound::Logic::State base + three concrete state
// subclasses, reconstructed from BURNOUT_X360_ARTIST.XEX:
//   State::IsAttachedToThis                                 @ 0x826916D8
//   BrnSound::Logic::Passby::PassbyState::PassbyState       @ 0x826BF5E0
//   BrnSound::Logic::Streaming::StreamingState::StreamingState @ 0x826B0CB0
//   BrnSound::Vehicles::VehicleState::VehicleState          @ 0x826C9E70
//
// The State default ctor reproduces the base-member zero/seed sequence the three
// derived ctors inline (offsets +4..+80); each derived ctor then zeros/seeds its
// own members by name. Member access is BY NAME (no raw-offset writes); the X360
// absolute offsets in CgsState.h are documentation only.
//
// Cited by X360 address only -- no leaked-source provenance.
// ============================================================================

#include "GameShared/GameClasses/Sound/Logic/CgsState.h"

#include <cstring> // std::memset

namespace CgsSound
{
namespace Logic
{

// CgsState.h:370. Zero/seed the State base members. This mirrors the inline base
// init the derived ctors emit (X360 stores 0 across +4..+80, with mfCurTime /
// mfDeltaTime seeded to 0.0). MemBase's vtable is installed by its own ctor.
State::State()
    : miInstNum(0)
    , meMapState(0)
    , miStateInstType(0)
    , mpvAttachment(0)
    , mpPrevState(0)
    , mpNextState(0)
    , mpHeadEffectControl(0)
    , mpHeadEffectObject(0)
    , mpStateManager(0)
    , mpLogicModule(0)
    , miSFXFlags(0)
    , miNumLoadedEffectObjects(0)
    , miNumLoadedEffectControls(0)
    , mePrepareState(E_PREPARE_STATE_CREATE_OBJECTS)
    , mpCurrentEffect(0)
    , mbIsAttached(false)
    , mfCurTime(0.0f)
    , mfDeltaTime(0.0f)
{
    mauUpdateState[0] = 0u;
    mauUpdateState[1] = 0u;
}

// 0x826916D8. The X360 computes (mpvAttachment - apv == 0) as a boolean: true when
// the supplied pointer is this state's attachment.
bool State::IsAttachedToThis(void* apvAttachment)
{
    return mpvAttachment == apvAttachment;
}

} // namespace Logic
} // namespace CgsSound

// --- derived ctors ----------------------------------------------------------

namespace BrnSound
{
namespace Logic
{
namespace Passby
{
    // 0x826BF5E0. Zero the +96 region, seed mi120=3 and mf124=1.0 (the X360 loads
    // 1.0 from flt_82001C98 into +124 and 3 into +120); everything else is 0/0.0.
    PassbyState::PassbyState()
        : CgsSound::Logic::State()
        , mi112(0)
        , mf116(0.0f)
        , mi120(3)
        , mf124(1.0f)
        , mu128(0)
    {
        std::memset(mau8Region, 0, sizeof(mau8Region));
    }
} // namespace Passby

namespace Streaming
{
    // 0x826B0CB0. Seed the small trailing block to 0 / 0.0.
    StreamingState::StreamingState()
        : CgsSound::Logic::State()
        , mi84(0)
        , mi88(0)
        , mf92(0.0f)
        , mf96(0.0f)
        , mi100(0)
        , mu104(0)
    {
    }
} // namespace Streaming
} // namespace Logic

namespace Vehicles
{
    // 0x826C9E70. Clear the embedded RaceCarState, then seed the tail fields to
    // 0 / 0.0 (the X360 zeros each, with mf1312 loaded from the 0.0 constant).
    VehicleState::VehicleState()
        : CgsSound::Logic::State()
        , mi1216(0)
        , mi1220(0)
        , mu1224(0u)
        , mu1268(0)
        , mu1281(0)
        , mu1296(0u)
        , mu1304(0u)
        , mf1312(0.0f)
        , mu1317(0)
        , mu1318(0)
    {
        // Clear the embedded RaceCarState BY NAME (X360 RaceCarState::Clear(this+0x60)).
        mRaceCarState.Clear();
    }
} // namespace Vehicles
} // namespace BrnSound
