#include "GameSource/Sound/Vehicles/BrnPlayerVehicleState.h"
#include "GameSource/Sound/Vehicles/BrnVehicleStateManager.h"
#include "GameSource/Sound/Module/LogicModule/BrnSoundLogicModule.h"
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h"
#include "SharedClasses/DataLists/VehicleListEntry.h"
#include "GameShared/GameClasses/Core/CgsID.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

#include <cstring>

// =============================================================================
// BrnSound::Vehicles::PlayerVehicleState -- out-of-line deleting-destructor body.
// Reconstructed from BURNOUT_X360_ARTIST.XEX. See BrnPlayerVehicleState.h for the
// inheritance rationale. Mirrors the committed sibling scalar deleting destructors
// StreamingState (CgsState.cpp @ 0x826C9B28) and GlobalState (BrnGlobalState.cpp
// @ 0x826D2250).
// =============================================================================

namespace BrnSound
{
namespace Vehicles
{

// ---------------------------------------------------------------------------
// ~PlayerVehicleState  @ 0x826CA250  (the X360 `scalar deleting destructor')
//
// The two vtable installs and the conditional allocator-routed free (off_82FFB954,
// vtable slot +0x14) are MSVC's compiler-synthesised deleting-destructor thunk,
// re-emitted from this virtual destructor + operator delete -- NOT hand-written.
// The single observable source-level side effect is the DestroyEffects() call on
// the State grandparent base, reused BY NAME (its body is un-homed -- a separate
// sound-logic recon slice; declared in BrnState.h, no body fabricated here).
//
// FLAG: the (a2 & 1) tail invokes the global sound allocator (off_82FFB954) to free
// the object; that allocator is not homed here, so operator-delete dispatch is left
// to the host toolchain (the `delete` half of the X360 deleting destructor).
// ---------------------------------------------------------------------------
PlayerVehicleState::~PlayerVehicleState()
{
    DestroyEffects();
}

CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::State>*
PlayerVehicleState::GetStaticTypeInfo()
{
    static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::State> sTypeInfo(
        0x10000, "PlayerVehicleState", 0, &PlayerVehicleState::CreateObject);
    return &sTypeInfo;
}

CgsSound::Logic::State* PlayerVehicleState::CreateObject(u32 /*auType*/)
{
    return new PlayerVehicleState();
}

CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::State>*
PlayerVehicleState::GetTypeInfo() const
{
    return GetStaticTypeInfo();
}

const char* PlayerVehicleState::GetTypeName() const
{
    return "PlayerVehicleState";
}

static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::State>* const
    gpPlayerVehicleStateReg = CgsSound::Logic::State::AddToClassTypeInfoArray(
        PlayerVehicleState::GetStaticTypeInfo());

void PlayerVehicleState::Attach(void* apvAttachment)
{
    CGS_ASSERT(apvAttachment != 0, "lpAttachInfo");
    const AttachInfo* lpInfo = static_cast<const AttachInfo*>(apvAttachment);
    mAttachInfo = *lpInfo;

    BrnSound::Module::SoundLogicModule* lpModule =
        static_cast<BrnSound::Module::SoundLogicModule*>(GetLogicModule());
    BrnSound::Module::Io::LogicInputBuffer* lpInput = lpModule->GetBrnInputStructure();
    const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpVehicles =
        lpInput->GetVehicleInterface();
    const EActiveRaceCarIndex leIndex = static_cast<EActiveRaceCarIndex>(mAttachInfo.muVehicleIndex);
    const BrnPhysics::Vehicle::RaceCarState* lpState = lpVehicles->GetRaceCarState(leIndex);
    CGS_ASSERT(lpState != 0, "lpRaceCarState");
    if (lpState)
        mVehiclePhysicsData = *lpState;

    std::memset(mauVehicleBoostInfo, 0, sizeof(mauVehicleBoostInfo));
    const BrnResource::VehicleListEntry* lpVehicle =
        static_cast<const BrnResource::VehicleListEntry*>(mAttachInfo.mpVehicleAsset);
    CGS_ASSERT(lpVehicle != 0 && lpVehicle->GetAttribCollectionKeyHash() != 0,
               "mAttachInfo.mpVehicleAsset->GetAttribCollectionKey()");
    if (lpVehicle)
    {
        CgsIDConvertToString(lpVehicle->GetEngineName(),
                             mcaEngineComponentName[E_ENGINE]);
        CgsIDConvertToString(lpVehicle->GetExhaustName(),
                             mcaEngineComponentName[E_EXHAUST]);
        const u64 luEngineKey = lpVehicle->GetEngineKey();
        const u64 luExhaustKey = lpVehicle->GetExhaustKey();
        std::memcpy(&mEngineComponentKey[E_ENGINE], &luEngineKey, sizeof(luEngineKey));
        std::memcpy(&mEngineComponentKey[E_EXHAUST], &luExhaustKey, sizeof(luExhaustKey));
    }

    CgsSound::Logic::State::Attach(apvAttachment);
}

void PlayerVehicleState::UpdateParams(f32 af32DeltaTime)
{
    VehicleState::UpdateParams(af32DeltaTime);

    if (mauUpdateState[0] == CgsSound::Logic::State::E_UPDATE_ATTACHED
        && mauUpdateState[1] != mauUpdateState[0])
    {
        VehicleStateManager* lpManager =
            static_cast<VehicleStateManager*>(GetStateManager());
        lpManager->OnAssetLoaded(mAttachInfo.mAttachToken,
                                 mAttachInfo.muVehicleIndex, true);
    }

    if (IsAttached()
        && mauUpdateState[0] == CgsSound::Logic::State::E_UPDATE_ATTACHED
        && !VehicleStateManager::IsDesiredEntryPlayer(mAttachInfo.muVehicleIndex))
    {
        Detach();
    }
}

bool PlayerVehicleState::Detach()
{
    const AttachInfo lInfo = mAttachInfo;
    if (!VehicleState::Detach())
        return false;
    VehicleStateManager* lpManager =
        static_cast<VehicleStateManager*>(GetStateManager());
    lpManager->OnAssetUnloaded(lInfo.mAttachToken, lInfo.muVehicleIndex);
    return true;
}

} // namespace Vehicles
} // namespace BrnSound
