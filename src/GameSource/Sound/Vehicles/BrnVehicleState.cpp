#include "GameSource/Sound/Vehicles/BrnVehicleState.h"
#include "GameSource/Sound/Vehicles/BrnVehicleStateManager.h"
#include "GameSource/Sound/Module/LogicModule/BrnSoundLogicModule.h"
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h"
#include "GameSource/AttribSys/Generated/classes/burnoutcarasset.h"
#include "GameSource/AttribSys/Generated/classes/physicsvehiclehandling.h"
#include "GameSource/AttribSys/Generated/classes/physicsvehicleengineattribs.h"
#include "SharedClasses/DataLists/VehicleListEntry.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

#include <algorithm>
#include <cstring>

// =============================================================================
// BrnSound::Vehicles::VehicleState out-of-line bodies.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. See BrnVehicleState.h for the
// DWARF-reconciled layout (2026-08-25 wave 5: VehicleState is a STRUCT per the
// DWARF -- the old namespace modelling and the GameShared CgsState.h rival are
// both folded onto the single header definition; the ctor body relocated here
// from the CgsState.cpp rival home).
//
//   VehicleState::VehicleState        @ 0x826C9E70
//   VehicleState::AttachInfo::Construct @ 0x82681FC8
//   VehicleState::GetEngineComponentKey @ (inlined at its PhysicsControl
//                                          forwarder @0x82682D10)
// =============================================================================

namespace BrnSound
{
namespace Vehicles
{

// @ 0x826C9E70. Clear the physics blob via RaceCarState::Clear (@0x8229FFC8) and
// seed the tail fields the asm writes: the attach record zeroed (asset/index/token
// = the old rival's mi1216/mi1220/mu1224 zero-seeds), the two component-name
// strings' first chars zeroed (bytes 1268/1281), the two key elements zeroed
// (mu1296/mu1304), mfMaxRpm = 0.0, and the collision flag + the is-active
// DataPoint bytes zeroed (mu1317/mu1318). mVehicleBoostInfo is untouched by the
// X360 ctor; value-init keeps it defined without fabricating stores.
VehicleState::VehicleState()
    : BrnSound::Logic::BrnState()
    , mAttachInfo()
    , mfMaxRpm(0.0f)
    , mbCollisionOccuredFlag(false)
    , bIsRaceCarActive()
{
    mVehiclePhysicsData.Clear();            // bl RaceCarState::Clear @0x8229FFC8

    mAttachInfo.mpVehicleAsset = 0;         // the old mi1216 zero-seed
    mAttachInfo.muVehicleIndex = 0;         // the old mi1220 zero-seed
    mAttachInfo.mAttachToken   = 0;         // the old mu1224 zero-seed

    for (u32 lu = 0; lu < sizeof(mauVehicleBoostInfo); ++lu)
        mauVehicleBoostInfo[lu] = 0;        // value-defined (untouched by the X360 ctor)

    mcaEngineComponentName[0][0] = '\0';    // stb 0, +1268
    mcaEngineComponentName[1][0] = '\0';    // stb 0, +1281

    mEngineComponentKey[0].mKey  = 0;       // the old mu1296 zero-seed
    mEngineComponentKey[0].muPad = 0;
    mEngineComponentKey[1].mKey  = 0;       // the old mu1304 zero-seed
    mEngineComponentKey[1].muPad = 0;
}

// The component attribute key, by name (the console (type+0xA2)*8 walk == this
// member array: 0xA2*8 == 0x510 == +1296) with the console's non-zero guard
// (see the PhysicsControl forwarder @0x82682D10, which inlines this read).
u64 VehicleState::GetEngineComponentKey( EEngineComponentType aeComponentType ) const
{
    CGS_ASSERT(aeComponentType >= E_ENGINE && aeComponentType < E_MAX_TYPES,
               "leComponentType >= E_ENGINE && leComponentType < E_MAX_TYPES");
    u64 lKey = 0;
    std::memcpy(&lKey, &mEngineComponentKey[aeComponentType], sizeof(lKey));
    CGS_ASSERT(lKey != 0, "mEngineComponentKey != 0");
    return lKey;
}

const char* VehicleState::GetEngineComponentName(EEngineComponentType aeComponentType) const
{
    CGS_ASSERT(aeComponentType >= E_ENGINE && aeComponentType < E_MAX_TYPES,
               "leComponentType >= E_ENGINE && leComponentType < E_MAX_TYPES");
    const char* lpcName = &mcaEngineComponentName[aeComponentType][0];
    CGS_ASSERT(lpcName[0] != '\0',
               "strcmp(&mcaEngineComponentName[leComponentType][0],\"\") != 0");
    return lpcName;
}

void VehicleState::Attach(void* apvAttachment)
{
    CgsSound::Logic::State::Attach(apvAttachment);
}

bool VehicleState::IsAttachedToThis(void* apvAttachment)
{
    if (!IsAttached() || !apvAttachment)
        return false;
    const AttachInfo* lpInfo = static_cast<const AttachInfo*>(apvAttachment);
    return lpInfo->mAttachToken == mAttachInfo.mAttachToken
        && lpInfo->muVehicleIndex == mAttachInfo.muVehicleIndex;
}

void VehicleState::Clear()
{
    bIsRaceCarActive.Flush(false);
    mVehiclePhysicsData.Clear();
    mAttachInfo.mpVehicleAsset = 0;
    mAttachInfo.muVehicleIndex = 0;
    mAttachInfo.mAttachToken = 0;
    mcaEngineComponentName[E_ENGINE][0] = '\0';
    mcaEngineComponentName[E_EXHAUST][0] = '\0';
    std::memset(mEngineComponentKey, 0, sizeof(mEngineComponentKey));
    mfMaxRpm = 0.0f;
}

void VehicleState::UpdateParams(f32 af32DeltaTime)
{
    CgsSound::Logic::State::UpdateParams(af32DeltaTime);
    if (!IsAttached() || mauUpdateState[0] != CgsSound::Logic::State::E_UPDATE_ATTACHED)
        return;

    BrnSound::Module::SoundLogicModule* lpModule =
        static_cast<BrnSound::Module::SoundLogicModule*>(GetLogicModule());
    BrnSound::Module::Io::LogicInputBuffer* lpInput = lpModule->GetBrnInputStructure();
    const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpVehicles =
        lpInput->GetVehicleInterface();
    const EActiveRaceCarIndex leIndex = static_cast<EActiveRaceCarIndex>(mAttachInfo.muVehicleIndex);
    const BrnPhysics::Vehicle::RaceCarState* lpState = lpVehicles->GetRaceCarState(leIndex);
    const bool lbActive = lpVehicles->IsRaceCarActive(leIndex)
        && lpState != 0 && lpState->mCarAssetAttribKey != 0;
    bIsRaceCarActive.Update(lbActive);

    if (lbActive)
    {
        const BrnWorld::RaceCarEntityModuleIO::BoostOutputInfo* lpBoost =
            lpVehicles->GetBoostOutputInfoN(leIndex);
        CGS_ASSERT(lpBoost != 0, "lpBoostInfo");
        mVehiclePhysicsData = *lpState;
        if (lpBoost)
            std::memcpy(mauVehicleBoostInfo, lpBoost, sizeof(mauVehicleBoostInfo));

        if (!bIsRaceCarActive.GetPrevious())
        {
            const BrnResource::VehicleListEntry* lpVehicle =
                static_cast<const BrnResource::VehicleListEntry*>(mAttachInfo.mpVehicleAsset);
            CGS_ASSERT(lpVehicle != 0 && lpVehicle->GetAttribCollectionKeyHash() != 0,
                       "mAttachInfo.mpVehicleAsset->GetAttribCollectionKey()->GetHashKey()");
            if (lpVehicle)
            {
                Attrib::Gen::burnoutcarasset lCarAsset(lpVehicle->GetAttribCollectionKeyHash(), 0);
                Attrib::RefSpec* lpHandlingSpec = lCarAsset.GetPhysicsVehicleHandlingRefSpec();
                Attrib::Gen::physicsvehiclehandling lHandling(
                    lpHandlingSpec ? const_cast<Attrib::Collection*>(lpHandlingSpec->GetCollection()) : 0, 0);
                Attrib::Gen::physicsvehicleengineattribs lEngine(
                    const_cast<Attrib::Collection*>(
                        lHandling.PhysicsVehicleEngineAttribs().GetCollection()), 0);
                mfMaxRpm = -1.0f;
                // ARTIST reduces the two authored GearUpRPM vec3s' six lanes.
                for (u32 luGear = 0; luGear < 6; ++luGear)
                    mfMaxRpm = (std::max)(mfMaxRpm, lEngine.GetGearUpRPM(luGear));
            }
        }
    }

    if (IsAttached()
        && VehicleStateManager::GetLoadedAssetId(mAttachInfo.muVehicleIndex)
            != mAttachInfo.mAttachToken)
    {
        Detach();
    }
}

bool VehicleState::Detach()
{
    if (mauUpdateState[0] != CgsSound::Logic::State::E_UPDATE_ATTACHED)
        return false;
    CgsSound::Logic::State::Detach();
    Clear();
    return true;
}

// =============================================================================
// VehicleState::AttachInfo::Construct  @ 0x82681FC8
//
// Validates the active-race-car index (signed bounds check: liVehicleIndex >= 0
// && < KI_MAX_ACTIVE_RACE_CARS) and the asset pointer, then writes the three
// fields in the X360 store order.
//   cmpwi r30,0 ; blt -> assert       (index < 0 fires the range assert)
//   cmpwi r30,8 ; blt skip-assert     (index >= 8 fires the range assert)
//   cmplwi r28,0 ; bne skip-assert    (asset == 0 fires the non-null assert)
//   stw r28,0 ; std r27,8 ; stw r30,4 ; return this(r31)
// =============================================================================
VehicleState::AttachInfo* VehicleState::AttachInfo::Construct(
    u64 aAttachToken, void* apVehicleAsset, u32 auVehicleIndex )
{
    // Signed bounds check -- the X360 compares as a signed int (blt/cmpwi).
    CGS_ASSERT(
        static_cast<s32>(auVehicleIndex) >= 0
            && static_cast<s32>(auVehicleIndex) < static_cast<s32>(KI_MAX_ACTIVE_RACE_CARS),
        "liVehicleIndex >= 0 && liVehicleIndex < static_cast<int32_t>(BrnWorld::KI_MAX_ACTIVE_RACE_CARS)");

    CGS_ASSERT(apVehicleAsset != 0, "lpVehicleAsset");

    mpVehicleAsset = apVehicleAsset; // stw r28, 0x00
    mAttachToken   = aAttachToken;   // std r27, 0x08
    muVehicleIndex = auVehicleIndex; // stw r30, 0x04

    return this;
}

} // namespace Vehicles
} // namespace BrnSound
