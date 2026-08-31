#include "GameSource/Sound/Vehicles/Engines/BrnSweetenersEffect.h"
#include "GameSource/Sound/Vehicles/Engines/BrnPhysicsControl.h"
#include "GameSource/Sound/Vehicles/Engines/BrnShiftControl.h"
#include "GameSource/Sound/Module/LogicModule/BrnSoundLogicModule.h"
#include "GameSource/Sound/Module/BrnRootSoundModuleIo.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

#include <cstring>   // std::memcpy

// =============================================================================
// BrnSound::Vehicles::Engines::SweetenersEffect -- out-of-line bodies.
// Reconstructed from BURNOUT_X360_ARTIST.XEX. Recon'd function set:
//   CreateObjec(u32)               @ 0x826CF5A0  (the factory hook)
//   SweetenersEffect()             @ 0x826CF290  (MSVC inlined full-object ctor)
//   `vector deleting destructor'   @ 0x826E4B18  (-> ~SweetenersEffect anchor)
// =============================================================================

namespace BrnSound
{
namespace Vehicles
{
namespace Engines
{

// ---------------------------------------------------------------------------
// SweetenersEffect::CreateObjec  @ 0x826CF5A0   (the factory hook)
//
// The X360 allocates an 800-byte (0x320) block through CgsSound::MemBase::operator
// new(size, tag, flavour) tagged "SweetenersEffect" and placement-constructs a
// SweetenersEffect, returning `this + 4` (the IResourceRequester secondary-base view).
// `luFlavour` only selects the operator-new flavour (0/1).
//
// FLAG (allocator gate): CgsSound::MemBase does NOT model operator new(size, tag,
// flavour) here, so this uses the host `new`; the +4 adjust is the static_cast to the
// IResourceRequester base. The 0x320 size is documentation only.
// ---------------------------------------------------------------------------
BrnSound::Logic::IResourceRequester* SweetenersEffect::CreateObjec( u32 luFlavour )
{
    (void)luFlavour; // selects only the operator-new flavour (0/1) on the X360
    return static_cast<BrnSound::Logic::IResourceRequester*>( new SweetenersEffect() );
}

// ---------------------------------------------------------------------------
// SweetenersEffect::SweetenersEffect  @ 0x826CF290   (MSVC inlined full-object ctor)
//
// The dual-vptr install + base-region zero stores are the compiler-emitted
// BrnEffectObject base sub-object construction, reproduced by the base default ctor
// (reused BY NAME). The body then constructs the five embedded VoiceWrappers and the
// PathLine<2>, and seeds the per-effect RNG jitter table.
//
// FLAG (un-homed leaf members -- DEFERRED): the X360 ctor also installs an inner
// {off_820B3250,0,0} sub-object @ +0x158 and zero-/one-inits leaf scalar fields whose
// names/types (and the +0x158 vtable identity) are un-homed. Those are DECLARATION-ONLY
// / DEFERRED (see header) and NOT fabricated. The +0x158 sub-object is NOT bound to the
// committed CgsSound::Playback::Content (unproven identity).
// ---------------------------------------------------------------------------
SweetenersEffect::SweetenersEffect()
    : BrnEffectObject()   // installs the base vptrs + zero-inits the base members (BY NAME)
    , mVoice0()           // bl VoiceWrapper::VoiceWrapper(this+0x7C)
    , mVoice1()           // bl VoiceWrapper::VoiceWrapper(this+0xCC)
    , mVoice2()           // bl VoiceWrapper::VoiceWrapper(this+0x1D4)
    , mVoice3()           // bl VoiceWrapper::VoiceWrapper(this+0x228)
    , mVoice4()           // bl VoiceWrapper::VoiceWrapper(this+0x28C)
    , mPathLine()         // bl PathLine<2>::PathLine/ClearStages(this+0x2DC)
    , mbEnableSweetners(false)
    , mpPhysicsControl(nullptr)
    , mpShiftingControl(nullptr)
    , meRaceCarEngineState(
        BrnWorld::RaceCarEntityModuleIO::E_ACTIVE_RACE_CAR_ENGINE_STATE_COUNT)
{
    // ----- per-effect random-jitter table seed (attestable; store-for-store) --------
    // X360 (0x826CF3B0..0x826CF57C): slot[0] = 1.0f, then SEVEN computed writes fill
    // slots ((++counter)&7) = 1..7 with random floats in [1.0, 2.0): each slot =
    // reinterpret(0x3F800000 | ((state>>32) & 0x7FFFFF)). The LCG is
    // state = state * 0x5851F42D4C957F2D + 1, seeded 0x1AD0891BC87CD8C9. After the 7
    // writes a final bare (++counter)&7 runs with NO store. slot[0] is NOT overwritten.
    mafJitterTable[0] = 1.0f;
    u64 lu64State = 0x1AD0891BC87CD8C9ULL;
    u32 luCounter = 0;
    for ( s32 li = 0; li < 7; ++li )
    {
        const u32 luHigh = static_cast<u32>( lu64State >> 32 ); // srdi r,state,32
        luCounter = ( luCounter + 1 ) & 7;                       // addi+1 ; clrlwi ,29
        const u32 luBits = 0x3F800000u | ( luHigh & 0x007FFFFFu ); // inslwi ,23,9
        f32 lfValue;
        std::memcpy( &lfValue, &luBits, sizeof(lfValue) );
        mafJitterTable[luCounter] = lfValue;                     // stwx r,r*4,r11
        lu64State = lu64State * 0x5851F42D4C957F2DULL + 1ULL;     // mulld ; addi+1
    }
    luCounter = ( luCounter + 1 ) & 7; // trailing bare counter bump (0x826CF570); no write
    (void)luCounter;
}

// ---------------------------------------------------------------------------
// ~SweetenersEffect  @ 0x826E4B18  (anchor for the X360 `vector deleting destructor').
// The member teardown -- the five embedded VoiceWrappers, the PathLine<2>, and the
// inherited BrnEffectObject dual-base settle -- is produced by the base destructor
// chain + the embedded member dtors (BY NAME), so this leaf body adds nothing. The
// (a2 & 1) allocator-free tail is left to the host toolchain (off_82FFB954 not homed).
// ---------------------------------------------------------------------------
SweetenersEffect::~SweetenersEffect()
{
}

// X360 0x82685558. The effect consumes PhysicsControl in controller slot 0 and
// ShiftControl in slot 1.
s32 SweetenersEffect::GetController(s32 aiSlot)
{
    if (aiSlot == 0)
        return 0;
    if (aiSlot == 1)
        return 2;
    return -1;
}

// X360 0x82685580. The effect-id field is the middle seven bits of ObjectId;
// EffectBase::GetEffectID exposes that authored identity directly.
void SweetenersEffect::AttachController(CgsSound::Logic::EffectBase* apController)
{
    CGS_ASSERT(apController != nullptr, "apController");
    if (!apController)
        return;

    switch (apController->GetEffectID())
    {
    case 0:
        mpPhysicsControl = static_cast<PhysicsControl*>(apController);
        break;
    case 2:
        mpShiftingControl = static_cast<ShiftControl*>(apController);
        break;
    default:
        CGS_ASSERT(false, "Unexpected control.");
        break;
    }
}

// X360 0x826FD7A0. The complete sweetener-bank voice preparation is independent
// of the engine bed, but the authored no-bank branch is load-bearing: it opens
// dynamic-mixer input 1 immediately. When a bank is present UpdateParams owns
// that input and follows the player's engine state instead.
bool SweetenersEffect::Attach()
{
    if (!CgsSound::Logic::EffectBase::Attach())
        return false;

    CGS_ASSERT(mpPhysicsControl != nullptr, "mpPhysicsControl");
    if (!mpPhysicsControl)
        return false;

    const Attrib::Gen::vehicleengine& lrVehicleEngine =
        mpPhysicsControl->GetVehicleEngineAttributes();
    mbEnableSweetners = lrVehicleEngine.SweetenersAsset() != 0;
    if (!mbEnableSweetners)
        SetMixerInputValue(1, 0x7FFF);

    return true;
}

// X360 0x826FCEE8. The player SweetenersEffect is the authoritative trigger for
// mix input 1. The original reads the copied active-race-car output interface,
// latches current/previous engine state, and opens the gate only for RUNNING(2).
void SweetenersEffect::UpdateParams(f32 afTimeStep)
{
    SetMixerInputValue(0, 0);
    mPathLine.Update(afTimeStep);

    if (!mbEnableSweetners || GetStateId() != 1)
        return;

    BrnSound::Module::SoundLogicModule* lpModule =
        static_cast<BrnSound::Module::SoundLogicModule*>(GetLogicModule());
    CGS_ASSERT(lpModule != nullptr, "mpLogicModule");
    if (!lpModule)
        return;

    BrnSound::Module::Io::LogicInputBuffer* lpInput = lpModule->GetBrnInputStructure();
    const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpVehicles =
        lpInput->GetVehicleInterface();

    BrnWorld::RaceCarEntityModuleIO::EActiveRaceCarEngineState leEngineState =
        BrnWorld::RaceCarEntityModuleIO::E_ACTIVE_RACE_CAR_ENGINE_STATE_COUNT;
    if (lpVehicles->GetPlayerActiveRaceCarIndex() != E_ACTIVE_RACE_CAR_INDEX_INVALID)
        leEngineState = lpVehicles->GetPlayerEngineState();

    meRaceCarEngineState.Update(leEngineState);
    SetMixerInputValue(
        1,
        leEngineState ==
                BrnWorld::RaceCarEntityModuleIO::E_ACTIVE_RACE_CAR_ENGINE_STATE_RUNNING
            ? 0x7FFF
            : 0);
}

} // namespace Engines
} // namespace Vehicles
} // namespace BrnSound
