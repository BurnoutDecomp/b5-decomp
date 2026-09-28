#include "GameSource/Sound/Module/LogicModule/BrnSoundLogicModule.h"
#include "GameSource/Sound/Passby/BrnPassbyStateManager.h"
#include "GameSource/Sound/Vehicles/Environment/BrnEnvironmentSoundDiag.h"
#include "GameShared/GameClasses/Development/DebugSystem/Interface/CgsDebugInterface.h"
#include "GameSource/Sound/Vehicles/Environment/BrnEnclosureControl.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "GameSource/Sound/Vehicles/Engines/BrnPhysicsControl.h"

// =============================================================================
// BrnSound::Vehicles::Environment::EnclosureControl -- out-of-line bodies.
// Reconstructed from BURNOUT_X360_ARTIST.XEX. Recon'd function set:
//   ConvertRegionTypeToIndex(int)  @ 0x82685FA0
//   Create(bool)                   @ 0x826D0A30
//   `vector deleting destructor'   @ 0x826B94A8  (-> ~EnclosureControl anchor)
// =============================================================================

namespace BrnSound
{
namespace Vehicles
{
namespace Environment
{

// ---------------------------------------------------------------------------
// EnclosureControl::ConvertRegionTypeToIndex(int)  @ 0x82685FA0
//
// The X360 lays out the switch as `switch(liRegionType - 25)` over a 6-entry jump table
// (cases 25..30) with everything else (including in-range 19..24, 31) falling to the
// default 19. The `this` pointer is passed but never dereferenced -> pure mapping.
// ---------------------------------------------------------------------------
int EnclosureControl::ConvertRegionTypeToIndex( int liRegionType ) const
{
    CGS_ASSERT(liRegionType >= 19 && liRegionType <= 31,
               "EnclosureControl : Region type out of range.");

    switch ( liRegionType )
    {
    case 25: return 9;
    case 26: return 13;
    case 27: return 8;
    case 28: return 14;
    case 29: return 17;
    case 30: return 15;
    default: return 19;
    }
}

// ---------------------------------------------------------------------------
// EnclosureControl::Create(bool)  @ 0x826D0A30   (the factory)
// Allocates a 0x50 (80) byte block via CgsSound::MemBase::operator new(size, tag,
// flavour) tagged "EnclosureControl" and inline-constructs an EnclosureControl, upcast
// to the EffectObject* base (+4 adjust). The bool arg only selects the operator-new
// flavour (0/1); both arms use the same size + ctor.
// FLAG (allocator gate): CgsSound::MemBase::operator new is not homed here, so this uses
// the host `new`; observable result matches. The 0x50 size is documentation only.
// ---------------------------------------------------------------------------
CgsSound::Logic::EffectControl* EnclosureControl::Create( bool /*lbFlavour*/ )
{
    return new EnclosureControl();
}

// The shortest gap between two whoosh triggers, in seconds; Attach starts the timer here
// so the first trigger is never suppressed.
static const f32 KF_MIN_TIME_BETWEEN_TRIGGERS = 0.5f;

// Controller slot 0 is the physics control.
s32 EnclosureControl::GetController(s32 aiIndex)
{
    return aiIndex == 0 ? 0 : -1;
}

void EnclosureControl::AttachController(CgsSound::Logic::EffectBase* apController)
{
    CGS_ASSERT(apController->GetEffectID() == 0, "Unexpected control.");
    if (apController->GetEffectID() == 0)
        mpPhysicsControl = static_cast<BrnSound::Vehicles::Engines::PhysicsControl*>(apController);
}

// Clears both trigger sets and arms the trigger timer.
bool EnclosureControl::Attach()
{
    if (!CgsSound::Logic::EffectBase::Attach())
        return false;
    for (s32 liPosition = 0; liPosition < E_TRIGGER_POSITION_COUNT; ++liPosition)
        maTriggerInfo[liPosition].Reset();
    mfTimeSinceTrigger = KF_MIN_TIME_BETWEEN_TRIGGERS;
    return true;
}

// ARTIST 82685D88. The two unrolled six-bit groups deliberately stop at 30;
// region 31 is not examined. The last changed region wins on both enter and exit.
BrnTrigger::GenericRegion::Type EntityTriggerInfo::GetChangeType() const
{
    CGS_ASSERT(HasChanged(), "EntityTriggerInfo : Has not changed.");
    s32 liFound = 19;
    const u32 luChanged = muActiveTriggers ^ muPrevTriggers;
    for (s32 liType = 19; liType < 31; ++liType)
        if (luChanged & (1u << (liType - 19)))
            liFound = liType;
    return static_cast<BrnTrigger::GenericRegion::Type>(liFound);
}

// Original debug bytes 82FFB8C8 and 82FFB8BE (both initially false).
bool KB_SHOW_STATIC_ENVIRONMENT = false;
static bool sbDebugWhoosh = false;

// ARTIST 826F4F40, adjusted EffectBase this. DWARF BrnEnclosureControl.cpp:190.
void EnclosureControl::UpdateParams(f32 afTimeStep)
{
    using BrnGameState::GameStateModuleIO::SoundTriggerAction;
    auto* lpLogicModule = static_cast<BrnSound::Module::SoundLogicModule*>(mpLogicModule);
    mfTimeSinceTrigger += afTimeStep;
    CGS_ASSERT(lpLogicModule != nullptr, "lpLogicModule");
    const EntityId leEntity = mpPhysicsControl->GetRawPhysicsData()->mEntityId;
    const SoundTriggerAction* lpAction = lpLogicModule->GetSoundTriggerAction(
        leEntity, SoundTriggerAction::E_TYPE_AT_ENTITY);
    EntityTriggerInfo& lrHere = maTriggerInfo[E_TRIGGER_POSITION_AT_ENTITY];
    if (lpAction)
    {
        lrHere.muPrevTriggers = lrHere.muActiveTriggers;
        lrHere.muActiveTriggers = lpAction->muActiveTriggers;
    }
    lpAction = lpLogicModule->GetSoundTriggerAction(
        leEntity, SoundTriggerAction::E_TYPE_AHEAD_OF_ENTITY);
    if (lpAction)
        ProcessTriggerAction(*lpAction, E_TRIGGER_POSITION_AHEAD_OF_ENTITY);

    if (lrHere.HasChanged())
    {
        CgsSound::Io::Message<bool> lMessage((lrHere.muActiveTriggers & 1) != 0);
        // 826F5058..68: id39, manager2, instanceFFFF, effect0, CONTROL2.
        lMessage.Construct(39, 2, CgsSound::Io::MessageHeader::KU16_NO_DESTINATION,
                           0, CgsSound::Io::MessageHeader::E_EFFECT_TYPE_CONTROL);
        lpLogicModule->PostMessage(lMessage);
        static s32 siDiag = 0;
        if (SndEnvDiagBudget(siDiag))
            *CgsDev::Log::gpDebugPrint << "[sndenv] enclosure active=" << lrHere.muActiveTriggers
                << " previous=" << lrHere.muPrevTriggers << " tunnel=" << static_cast<s32>(lMessage.mData)
                << " [FLAG PC witness]\n";
    }
    if (KB_SHOW_STATIC_ENVIRONMENT)
        DrawDebug();
}

// ARTIST 8269AFC8. Both threshold branches are blt: unordered proceeds.
void EnclosureControl::ProcessTriggerAction(
    const BrnGameState::GameStateModuleIO::SoundTriggerAction& arAction,
    eTriggerPosition aePosition)
{
    CGS_ASSERT(aePosition < E_TRIGGER_POSITION_COUNT, "lePosition < E_TRIGGER_POSITION_COUNT");
    EntityTriggerInfo& lrInfo = maTriggerInfo[aePosition];
    lrInfo.muPrevTriggers = lrInfo.muActiveTriggers;
    lrInfo.muActiveTriggers = arAction.muActiveTriggers;
    if (aePosition != E_TRIGGER_POSITION_AHEAD_OF_ENTITY || !lrInfo.HasChanged())
        return;
    // 8269B058 blt ->B138 rejects; otherwise ->B05C continues, including NaN.
    if (mfTimeSinceTrigger < KF_MIN_TIME_BETWEEN_TRIGGERS)
    {
        if (sbDebugWhoosh)
            *CgsDev::Log::gpDebugPrint << "[Whoosh][FAIL] Too little time between triggers.\n";
        return;
    }
    const f32 lfSpeed = mpPhysicsControl->GetPhysicsData().mSpeedMPH.GetCurrent();
    const s32 liType = ConvertRegionTypeToIndex(lrInfo.GetChangeType());
    if (liType >= 19)
        return;
    // 8269B088 blt ->B11C rejects; otherwise ->B08C posts, including NaN.
    if (lfSpeed < 60.0f) // 82F2CDF0
    {
        if (sbDebugWhoosh)
            *CgsDev::Log::gpDebugPrint << "[Whoosh][FAIL] Car below velocity threshold.\n";
        return;
    }
    auto* lpLogicModule = static_cast<BrnSound::Module::SoundLogicModule*>(mpLogicModule);
    mfTimeSinceTrigger = 0.0f;
    auto* lpPassbys = static_cast<BrnSound::Logic::Passby::PassbyStateManager*>(
        lpLogicModule->GetEnvironment().GetStateManager(4));
    const BrnSound::Logic::Passby::PassbyStateManager::Passby lPassby(
        arAction.mQueryPos, 0.0f,
        static_cast<AttribSys::Enums::ePassbyTypes::ePassbyTypes>(liType), false, 1.0f);
    lpPassbys->PostPassby(lPassby);
    if (sbDebugWhoosh)
        *CgsDev::Log::gpDebugPrint << "[Whoosh][PASS][Type " << liType << "] Car above velocity threshold.\n";
    static s32 siDiag = 0;
    if (SndEnvDiagBudget(siDiag))
        *CgsDev::Log::gpDebugPrint << "[sndenv] enclosure passby type=" << liType
            << " mph=" << lfSpeed << " [FLAG PC witness]\n";
}

// ARTIST 8269B1B0; labels are the twelve pointers at 820A3A64..820A3A90.
void EnclosureControl::DrawDebug() const
{
    static const char* const kaNames[] = {
        "Tunnel", "Overpass", "Bridge", "Warehouse", "LargeOverheadobject", "NarrowAlley",
        "Pass Tunnel", "Pass Overpass", "Pass Bridge", "Pass Warehouse", "Pass LargeOverheadobject", "Pass NarrowAlley"
    };
    CgsDev::DebugInterface lDebug;
    auto& lrRender = lDebug.Get2dRender();
    f32 lfY = 50.0f;
    for (s32 liType = 19; liType < 31; ++liType)
        if (maTriggerInfo[E_TRIGGER_POSITION_AT_ENTITY].IsTypeActive(
            static_cast<BrnTrigger::GenericRegion::Type>(liType)))
        {
            lrRender.Draw2DText(kaNames[liType - 19], 900.0f, lfY, 25.0f, 0xFFFFFFFFu);
            lfY += 30.0f;
        }
}

// ---------------------------------------------------------------------------
// ~EnclosureControl  @ 0x826B94A8  (anchor for the X360 `vector deleting destructor').
// The observable member teardown -- the dual-base vptr settle + meDetachState/
// mbResourcesReady/meAttachState clears -- is the inherited ~BrnEffectObject chain
// (byte-identical to the sibling SpeedStreamControl @ 0x826BA0A0), so this leaf body is
// empty. The (a2 & 1) allocator-free tail is left to the host toolchain.
// ---------------------------------------------------------------------------
EnclosureControl::~EnclosureControl()
{
}

} // namespace Environment
} // namespace Vehicles
} // namespace BrnSound
