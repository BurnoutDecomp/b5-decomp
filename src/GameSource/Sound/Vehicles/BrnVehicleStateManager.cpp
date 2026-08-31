#include "GameSource/Sound/Vehicles/BrnVehicleStateManager.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (index-range tripwire)
#include "SharedClasses/DataLists/VehicleListEntry.h"
#include "GameSource/Sound/Module/LogicModule/BrnSoundLogicModule.h"
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h"
#include "GameShared/GameClasses/System/Resource/CgsResourcePtr.h"
#include "GameShared/GameClasses/System/Resource/CgsResourceID.h"
#include "GameShared/GameClasses/Sound/Playback/CgsRegistry.h"

#include <cstdio>
#include <cstring>

// =============================================================================
// BrnSound::Vehicles::VehicleStateManager -- out-of-line body for the single
// ledger function owned by this TU:
//   GetAIEngineAssignment  @ 0x82682050
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// =============================================================================

namespace BrnSound
{
namespace Vehicles
{
namespace {

// The AI engine-voice assignment table the X360 indexes as byte_82FFB838[index]
// (lbzx r3, r11, r31). It lives in the writable data segment (0x82FFxxxx), i.e. it
// is RUNTIME-POPULATED per-car state that other VehicleStateManager paths fill --
// NOT a static const lookup. Its contents are therefore NOT a fact recoverable
// from this getter, and are modelled as zero-initialised storage (the honest
// default state the read observes before any assignment write). One byte per
// active race-car slot.
static u8 gaAIEngineAssignment[VehicleStateManager::KI_ACTIVE_RACE_CAR_COUNT] = { 0 };
static const BrnResource::VehicleListEntry*
    gapLoadedVehicleEntries[VehicleStateManager::KI_ACTIVE_RACE_CAR_COUNT] = { 0 };
static CgsID gaDesiredAssetIds[VehicleStateManager::KI_ACTIVE_RACE_CAR_COUNT] = { 0 };
static CgsID gaAttachedAssetIds[VehicleStateManager::KI_ACTIVE_RACE_CAR_COUNT] = { 0 };
static u8 guAddedMask = 0;
static u8 guDesiredPlayerMask = 0;
static u8 guAttachedMask = 0;
static u8 guAttachedPlayerMask = 0;
static s32 giFallbackChoice = 0;
static u32 guAddedRegistryCount = 0;
static u64 gauAddedRegistries[128] = { 0 };
}

// ---------------------------------------------------------------------------
// GetAIEngineAssignment  @ 0x82682050
//
//   if ( !(liVehicleIndex >= 0 && liVehicleIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT) )
//       assert("liVehicleIndex >= 0 && liVehicleIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT",
//              BrnVehicleStateManager.h:143)               ; non-gating tripwire
//   return gaAIEngineAssignment[liVehicleIndex]            ; lbzx r3, byte_82FFB838[idx]
//
// Store-for-store with the X360: the bounds check is the signed pair
// (cmpwi r31,0 ; blt) and (cmpwi r31,8 ; blt ok), i.e. 0 <= index < 8; the read is
// a single zero-extended byte load (lbzx). The Hex-Rays `unsigned int a1` is the
// register width; the asm compares it SIGNED for the >=0 half of the assert, so
// the index is conceptually a signed in-range value -- the assert text spells out
// both halves.
// ---------------------------------------------------------------------------
u8 VehicleStateManager::GetAIEngineAssignment( u32 luVehicleIndex )
{
    CGS_ASSERT( static_cast<s32>(luVehicleIndex) >= 0
                && luVehicleIndex < static_cast<u32>(KI_ACTIVE_RACE_CAR_COUNT),
                "liVehicleIndex >= 0 && liVehicleIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT" );

    return gaAIEngineAssignment[luVehicleIndex];
}

void VehicleStateManager::AddRegistry(const char* lpcEngineName, bool lbUseFilePath)
{
    CGS_ASSERT(std::strlen(lpcEngineName) < 13,
               "static_cast<int32_t>(strlen(lpcEngineName)) < KI_CGSID_STRING_LEN");

    const u64 luName = static_cast<u32>(CgsResource::ID::HashString(
        reinterpret_cast<const u8*>(lpcEngineName)));
    for (u32 luIndex = 0; luIndex < guAddedRegistryCount; ++luIndex)
    {
        if (gauAddedRegistries[luIndex] == luName)
            return;
    }

    CGS_ASSERT(guAddedRegistryCount < 128, "muAddedRegistryCount < KU_MAXIMUM_ADDED_REGISTRIES");
    if (guAddedRegistryCount >= 128)
        return;
    gauAddedRegistries[guAddedRegistryCount++] = luName;

    char lacRegistry[64];
    char lacBundle[64];
    std::snprintf(lacRegistry, sizeof(lacRegistry), "%sRegistry", lpcEngineName);
    const char* lpcBundle = nullptr;
    if (lbUseFilePath)
    {
        std::snprintf(lacBundle, sizeof(lacBundle), "Engines\\%08x.bundle",
                      static_cast<u32>(luName));
        lpcBundle = lacBundle;
    }

    CgsResource::ResourceHandle* lpHandle =
        GetResourceRegistrar().GetResource(lpcBundle, lacRegistry);
    if (!lpHandle && !lbUseFilePath)
    {
        // ARTIST's resource-only request is resolved against the already-loaded
        // engine bundle.  The native host registrar keeps the bundle hash in its
        // lookup key, so repeat the same lookup with that materialized path.
        std::snprintf(lacBundle, sizeof(lacBundle), "Engines\\%08x.bundle",
                      static_cast<u32>(luName));
        lpHandle = GetResourceRegistrar().GetResource(lacBundle, lacRegistry);
    }
    CGS_ASSERT(lpHandle != nullptr, "lpResourceHandle");
    if (!lpHandle)
        return;

    CgsResource::ResourcePtr<CgsSound::Playback::Registry> lRegistry(*lpHandle);
    static_cast<BrnSound::Module::SoundLogicModule*>(GetLogicModule())
        ->GetPlaybackModule().AddRegistry(*lRegistry, 1);
}

u8 VehicleStateManager::GenerateAIEngineAssignment(
    const BrnResource::VehicleListEntry* lpVehicleEntry, u32 luVehicleIndex)
{
    // ARTIST rodata @ 0x820AA48C: the two fallback choices for each authored
    // primary exhaust index (1..5).  Index zero is invalid/unassigned.
    static const u8 KAAU8_FALLBACKS[5][2] = {
        { 4, 2 }, { 3, 4 }, { 2, 4 }, { 1, 2 }, { 0, 0 }
    };
    const u8 luPrimary = lpVehicleEntry->GetAIExhaustIndex();
    if (luPrimary == 0)
        return 1;

    CGS_ASSERT(luPrimary < 6,
               "laiAttempts[0] > E_INVALID && laiAttempts[0] < E_MAX_CAR_INDECIES");
    if (luPrimary >= 6)
        return 1;

    const u8 lauAttempts[3] = {
        luPrimary,
        KAAU8_FALLBACKS[luPrimary - 1][0],
        KAAU8_FALLBACKS[luPrimary - 1][1]
    };

    for (s32 liAttempt = 0; liAttempt < 3; ++liAttempt)
    {
        const u8 luCandidate = lauAttempts[liAttempt];
        if (luCandidate == 0)
            return lauAttempts[0];

        bool lbInUse = false;
        for (u32 luCar = 0; luCar < KI_ACTIVE_RACE_CAR_COUNT; ++luCar)
        {
            if (luCar != luVehicleIndex && gaAIEngineAssignment[luCar] == luCandidate)
            {
                lbInUse = true;
                break;
            }
        }
        if (!lbInUse)
            return luCandidate;
    }

    for (s32 liAttempt = 0; liAttempt < 3; ++liAttempt)
    {
        giFallbackChoice = (giFallbackChoice + 1) % 3;
        if (lauAttempts[giFallbackChoice] != 0)
            return lauAttempts[giFallbackChoice];
    }
    return lauAttempts[0];
}

bool VehicleStateManager::AddEntry(CgsID lAssetId,
                                   const BrnResource::VehicleListEntry* lpVehicleEntry,
                                   u64 luUserId, bool lbIsPlayer)
{
    CGS_ASSERT(luUserId < KI_ACTIVE_RACE_CAR_COUNT,
               "luUserId >= 0 && luUserId < static_cast<uint64_t>(BrnWorld::KI_MAX_ACTIVE_RACE_CARS)");
    const u32 luIndex = static_cast<u32>(luUserId);
    gapLoadedVehicleEntries[luIndex] = lpVehicleEntry;
    gaDesiredAssetIds[luIndex] = lAssetId;
    guAddedMask |= static_cast<u8>(1u << luIndex);
    if (lbIsPlayer)
        guDesiredPlayerMask |= static_cast<u8>(1u << luIndex);
    else
        guDesiredPlayerMask &= static_cast<u8>(~(1u << luIndex));
    gaAIEngineAssignment[luIndex] = GenerateAIEngineAssignment(lpVehicleEntry, luIndex);
    if (lbIsPlayer)
        gaAIEngineAssignment[luIndex] = 0;
    return true;
}

bool VehicleStateManager::RemoveEntry(CgsID lAssetId, u64 luUserId)
{
    CGS_ASSERT(luUserId < KI_ACTIVE_RACE_CAR_COUNT,
               "luUserId >= 0 && luUserId < static_cast<uint64_t>(BrnWorld::KI_MAX_ACTIVE_RACE_CARS)");
    const u32 luIndex = static_cast<u32>(luUserId);
    CGS_ASSERT(gaDesiredAssetIds[luIndex] == lAssetId,
               "Removing a different asset from a racecar from the one that was added");
    gapLoadedVehicleEntries[luIndex] = 0;
    gaDesiredAssetIds[luIndex] = 0;
    guAddedMask &= static_cast<u8>(~(1u << luIndex));
    guDesiredPlayerMask &= static_cast<u8>(~(1u << luIndex));
    gaAIEngineAssignment[luIndex] = 0;
    return true;
}

const BrnResource::VehicleListEntry* VehicleStateManager::GetLoadedVehicleEntry(u32 luUserId)
{
    CGS_ASSERT(luUserId < KI_ACTIVE_RACE_CAR_COUNT, "luUserId < KI_ACTIVE_RACE_CAR_COUNT");
    return gapLoadedVehicleEntries[luUserId];
}

CgsID VehicleStateManager::GetLoadedAssetId(u32 luUserId)
{
    CGS_ASSERT(luUserId < KI_ACTIVE_RACE_CAR_COUNT, "luUserId < KI_ACTIVE_RACE_CAR_COUNT");
    return gaDesiredAssetIds[luUserId];
}

bool VehicleStateManager::IsLoadedEntryPlayer(u32 luUserId)
{
    CGS_ASSERT(luUserId < KI_ACTIVE_RACE_CAR_COUNT, "luUserId < KI_ACTIVE_RACE_CAR_COUNT");
    return (guDesiredPlayerMask & static_cast<u8>(1u << luUserId)) != 0;
}

bool VehicleStateManager::IsEntryAdded(u32 luUserId)
{
    CGS_ASSERT(luUserId < KI_ACTIVE_RACE_CAR_COUNT, "luUserId < KI_ACTIVE_RACE_CAR_COUNT");
    return (guAddedMask & static_cast<u8>(1u << luUserId)) != 0;
}

bool VehicleStateManager::IsDesiredEntryPlayer(u32 luUserId)
{
    return IsLoadedEntryPlayer(luUserId);
}

bool VehicleStateManager::IsAssetAttached(u32 luUserId)
{
    CGS_ASSERT(luUserId < KI_ACTIVE_RACE_CAR_COUNT, "luUserId < KI_ACTIVE_RACE_CAR_COUNT");
    return (guAttachedMask & static_cast<u8>(1u << luUserId)) != 0;
}

void VehicleStateManager::OnAssetLoaded(CgsID lAssetId, u32 luUserId, bool lbIsPlayer)
{
    CGS_ASSERT(luUserId < KI_ACTIVE_RACE_CAR_COUNT, "invalid index");
    const u8 luBit = static_cast<u8>(1u << luUserId);
    CGS_ASSERT((guAttachedMask & luBit) == 0,
               "Loading a vehicle asset twice");

    guAttachedMask |= luBit;
    gaAttachedAssetIds[luUserId] = lAssetId;
    if (lbIsPlayer)
        guAttachedPlayerMask |= luBit;

    if ((guAddedMask & luBit) != 0 && gaDesiredAssetIds[luUserId] == lAssetId)
    {
        BrnWorld::RaceCarEntityModuleIO::AudioCarDataLoadedEvent lEvent;
        lEvent.meMessageType = BrnWorld::RaceCarEntityModuleIO::AudioCarDataLoadedEvent::E_DATA_IS_LOADED;
        lEvent.mpVehicleListEntry = 0;
        lEvent.mAssetID = lAssetId;
        lEvent.miActiveRaceCarIndex = static_cast<u8>(luUserId);
        lEvent.mbIsPlayer = (guAttachedPlayerMask & luBit) != 0;
        static_cast<BrnSound::Module::SoundLogicModule*>(GetLogicModule())
            ->GetPreUpdateOutput().GetCarDataLoadedQueue().AddEvent(lEvent);
    }
}

void VehicleStateManager::OnAssetUnloaded(CgsID lAssetId, u32 luUserId)
{
    CGS_ASSERT(luUserId < KI_ACTIVE_RACE_CAR_COUNT, "invalid index");
    const u8 luBit = static_cast<u8>(1u << luUserId);
    BrnWorld::RaceCarEntityModuleIO::AudioCarDataLoadedEvent lEvent;
    lEvent.meMessageType = BrnWorld::RaceCarEntityModuleIO::AudioCarDataLoadedEvent::E_DATA_IS_UNLOADED;
    lEvent.mpVehicleListEntry = 0;
    lEvent.mAssetID = lAssetId;
    lEvent.miActiveRaceCarIndex = static_cast<u8>(luUserId);
    lEvent.mbIsPlayer = (guAttachedPlayerMask & luBit) != 0;

    gaAttachedAssetIds[luUserId] = 0;
    guAttachedMask &= static_cast<u8>(~luBit);
    guAttachedPlayerMask &= static_cast<u8>(~luBit);
    static_cast<BrnSound::Module::SoundLogicModule*>(GetLogicModule())
        ->GetPreUpdateOutput().GetCarDataLoadedQueue().AddEvent(lEvent);
}
} // namespace Vehicles
} // namespace BrnSound
