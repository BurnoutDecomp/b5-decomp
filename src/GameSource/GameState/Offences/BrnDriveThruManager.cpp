#include "GameSource/GameState/Offences/BrnDriveThruManager.h"

#include <cstddef>   // offsetof (layout asserts)
#include <cstring>   // memset/memcpy (opaque action payloads)

#include "rw/math/vpu/vector3_operation.h"                          // rw::math::vpu::operator- (discovery delta)

#include "GameShared/GameClasses/Core/CgsAssert.h"                   // CgsDev::Assert::Begin/Fire/EndAssert
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"     // CgsModule::VariableEventQueue<13312,16> (game-action queue)

#include "SharedClasses/Trigger/BrnTriggerData.h"        // BrnTrigger::TriggerData (GetGenericRegion*/count)
#include "SharedClasses/Trigger/BrnGenericRegion.h"      // BrnTrigger::GenericRegion (Type, GetType/IsDriveThru/GetId)
#include "SharedClasses/Trigger/BrnRegion.h"             // BrnTrigger::BoxRegion (GetPosition/ComputeTransform)
#include "SharedClasses/DataLists/VehicleList.h"         // BrnResource::VehicleList (GetVehicleIndex/GetVehicleData)
#include "SharedClasses/DataLists/VehicleListEntry.h"    // BrnResource::VehicleListEntry
#include "SharedClasses/Graphics/BrnGlobalColourPalette.h"  // BrnWorld::GlobalColourPalette

#include "SharedClasses/Progression/BrnTrainingTypes.h"           // BrnProgression::ETrainingType
#include "GameSource/GameState/Progression/BrnProfile.h"           // BrnProgression::Profile / ProfileEvent
#include "GameSource/GameState/Progression/BrnProgressionManager.h"// BrnProgression::ProgressionManager
#include "GameSource/GameState/Progression/BrnProgressionCarData.h" // BrnProgression::CarData (GetId/SetColourIndex/SetPaletteIndex)
#include "SharedClasses/Progression/BrnProgressionData.h"          // BrnProgression::ProgressionData / EventJunction
#include "SharedClasses/Progression/BrnRaceEventData.h"            // BrnProgression::RaceEventData (GetUnlockCarId)
#include "GameSource/GameState/CarSelect/BrnCarSelectManager.h"    // BrnGameState::CarSelectManager (EnterJunkyard)
#include "GameSource/GameState/TrainingManager/BrnTrainingManager.h"// BrnGameState::TrainingManager
#include "GameSource/GameState/ModeManager/BrnModeManager.h"       // BrnGameState::ModeManager
#include "GameSource/GameState/BrnGameStateModule.h"               // BrnGameState::GameStateModule (CheckForAllEventsBeingFound)
#include "GameSource/GameState/AchievementManager/BrnGameStateAchievementManagerBase.h" // OnFindAllCarParks/OnBodyShop
#include "GameSource/GameState/BrnGameStateModuleIO.h"             // GameStateModuleIO::OutputBuffer
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h" // RCEntityActiveRaceCarOutputInterface

namespace BrnGameState
{

using BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface;
typedef CgsModule::VariableEventQueue<13312, 16> GameActionQueueImpl;

// ============================================================================
// X360-baked tuning constants. The DWARF declares the named class-scope KF_* statics at
// BrnDriveThruManager.cpp:34-51; only the immediates the reconstructed bodies use are recovered.
//   flt_82001CC0 == -1.0 : the "drive-thru inactive" sentinel timer value.
//   flt_8202AC20 == 3.0  : KF_DRIVE_THRU_PRESENTATION_TIME (HandleDriveThru arm value).
//   flt_82001C98 == 1.0  : the re-arm minimum (HandleDriveThru).
//   flt_8200426C == 5.0  : the training-tip "settle" delay (seconds since region change).
// The three thresholds below are rodata floats NOT present in the exports (see `flagged`):
//   flt_82CDBD90 : the timer activation window (KF_DRIVE_THRU_PRESENTATION_TIME?).
//   flt_82CDBD94 : KF_DRIVE_THRU_DISCOVERY_DISTANCE_SQ (squared XZ discovery radius).
//   flt_82FADEC8 : the "just-armed min-time" comparison bound.
//   flt_82F31928 : exit-speed scale applied to KAF_MAX_DRIVE_THRU_EXIT_SPEEDS[i].
// ============================================================================
static const f32 KF_DRIVE_THRU_INACTIVE_TIME         = -1.0f;  // flt_82001CC0
static const f32 KF_DRIVE_THRU_REARM_TIME            =  1.0f;  // flt_82001C98
static const f32 KF_DRIVE_THRU_PRESENTATION_TIME     =  3.0f;  // flt_8202AC20
static const f32 KF_TRAINING_TIP_SETTLE_TIME         =  5.0f;  // flt_8200426C
static const f32 KF_DRIVE_THRU_ACTIVATION_TIME       =  0.0f;  // FLAG: flt_82CDBD90 (not in exports)
static const f32 KF_DRIVE_THRU_DISCOVERY_DISTANCE_SQ =  0.0f;  // FLAG: flt_82CDBD94 (not in exports)
static const f32 KF_DRIVE_THRU_DISCOVERY_MAX_Y       =  5.0f;  // X360 literal 5.0
static const f32 KF_DRIVE_THRU_JUST_ARMED_BOUND      =  0.0f;  // FLAG: flt_82FADEC8 (not in exports)
static const f32 KF_DRIVE_THRU_EXIT_SPEED_SCALE      =  1.0f;  // FLAG: flt_82F31928 (not in exports)

// ----------------------------------------------------------------------------
// X360-attested game-action event-type ids + payload sizes (the literal `li r5,<type>` /
// `li r6,<size>` immediates each AddEvent is given). Names mirror the GameStateModuleIO action
// structs the DWARF cpp lists; their field layouts are NOT in the exports, so the payloads are
// posted as sized opaque buffers (see `flagged`).
// ----------------------------------------------------------------------------
enum EGameAction
{
    KI_ACTION_GAS_STATION_DRIVE_THRU  = 100,  // size 144 (0x90)
    KI_ACTION_STOP_DRIVE_THRU_PRES    = 101,  // size 1
    KI_ACTION_STOPPED_DRIVE_THRU      = 102,  // size 1
    KI_ACTION_BODY_SHOP_DRIVE_THRU    = 97,   // size 144
    KI_ACTION_PAINT_SHOP_DRIVE_THRU   = 98,   // size 144
    KI_ACTION_DRIVE_THRU_DISCOVERED   = 104,  // size 12
    KI_ACTION_ALL_DRIVE_THRUS_FOUND   = 105,  // size 1
    KI_ACTION_REQUEST_AUTO_SAVE       = 55,   // size 1
    KI_ACTION_HUD_CANT_PAINT          = 263,  // size 1
    KI_ACTION_HUD_MUST_FIX_CAR        = 264,  // size 1
    KI_ACTION_CLOSE_DRIVE_THRU        = 46,   // size 8
    KI_ACTION_SEND_JUNCTION_PLAYER_AT = 201,  // size 40 (0x28)
};

// X360 BrnProgression training-type ids passed to IsTipAllowedInGameMode / HasPlayerSeenTrainingType
// (the `li r4,<n>` immediates). Cast to ETrainingType at the call (the enum's concrete enumerator
// names are not in this TU's exports; the numeric ids are X360-attested).
static const s32 KI_TRAIN_JUNKYARD   = 4;
static const s32 KI_TRAIN_GAS        = 10;
static const s32 KI_TRAIN_CAR_PARK   = 34;  // 0x22
static const s32 KI_TRAIN_PAINT      = 46;  // 0x2E

// Reinterpret the forward-declared queue handle the callers hold (an GameStateModuleIO::GameActionQueue*,
// X360 == OutputBuffer this+4) to the real CgsModule::VariableEventQueue<13312,16>. (Established
// precedent: BrnTriggerQueryManager.cpp.)
static inline GameActionQueueImpl* AsActionQueue(GameStateModuleIO::GameActionQueue* lpQueue)
{
    // IDENTITY. GameStateModuleIO::GameActionQueue IS CgsModule::VariableEventQueue<13312,16>
    // (BrnGameStateSharedIO.h). The helper survives only as the call-site vocabulary.
    return lpQueue;
}

// Post one shop drive-thru action (gas/body/paint, all 144-byte payloads seeded from the cached
// region transform). The payload's exact field layout (a Matrix44Affine + flags) is NOT in the
// exports, so it is a sized opaque buffer; the X360 stores `(a6 == 0)` (offline) as a flag byte.
// FLAG: GasStation/BodyShop/PaintShop action struct layouts not recovered.
static void PostShopAction(GameActionQueueImpl* lpQueue, const BrnTrigger::BoxRegion& lrCache,
                           s32 liActionType, bool lbIsOnline)
{
    u8 lacPayload[144];
    std::memset(lacPayload, 0, sizeof(lacPayload));
    Matrix44Affine lTransform = lrCache.ComputeTransform();
    std::memcpy(lacPayload, &lTransform, sizeof(lTransform) <= 64 ? sizeof(lTransform) : 64);
    lacPayload[143] = lbIsOnline ? 0 : 1;   // FLAG: exact flag offset within the 144B payload unknown
    lpQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(lacPayload), liActionType, 144);
}

// The shop-tip guard the X360 inlines before each training tip: the manager's pending-tip slot is
// empty, the tip is allowed in this game mode, the player hasn't already seen it, and >=5s have
// elapsed since the last region change. Reconstructed as a helper; per-call training-type id passed.
// FLAG: TrainingManager pending-slot/profile accessors are recovered from the asm offsets, not the
// exports (see `shared_header_grows`).
static void TryPlayTrainingTip(TrainingManager* lpTrainingManager, s32 liTrainingTypeId)
{
    const BrnProgression::ETrainingType leType =
        static_cast<BrnProgression::ETrainingType>(liTrainingTypeId);
    if (lpTrainingManager->IsTipPending())
        return;
    if (!lpTrainingManager->IsTipAllowedInGameMode(leType))
        return;
    BrnProgression::Profile* lpProfile = lpTrainingManager->GetProfile();
    if (lpProfile->HasPlayerSeenTrainingType(leType))
        return;
    if (lpTrainingManager->GetTimeSinceLastTip() < KF_TRAINING_TIP_SETTLE_TIME)
        return;
    lpTrainingManager->RequestTip(leType);
}

// ----------------------------------------------------------------------------
// Layout pins (never called). PARITY-BY-NAMED-MEMBER (the stunt-manager / BrnGameActions
// PrepareForModeAction precedent): the X360 byte offsets in the header (0x8A0..0x950) are exact
// for the CONSOLE, but on the x64 gate every raw pointer is 8 bytes (not 4) and the embedded
// DriveThruTriggerData / BoxRegionCache carry pointers + an alignas(16) Vector3, so an ABSOLUTE
// `offsetof(...) == 0x950` static_assert CANNOT hold on x64 -- the pointer-width delta shifts every
// trailing member. We therefore reproduce the DWARF member ORDER + TYPES (semantic parity, all body
// access is by name) and do NOT bake the absolute byte offsets. The RELATIVE asserts below survive
// the ABI difference and still pin the structural facts the verifier fixes established:
//   * the two CgsID members are CONTIGUOUS (the deleted maPad93C/maPad944 fix -- CgsID is 8 bytes),
//   * mDriveThruToTurnOffId immediately follows mRepairedCarID,
//   * mpProgressionManager is the last member and sits after the seven trailing bool flags.
// ----------------------------------------------------------------------------
void DriveThruManager::_AssertLayout()
{
    // mfTimeToActiveation sits one Vector3 (16B) past mPosition within the entry, independent of the
    // leading pointer's width: pin that relative fact rather than the X360 absolute +0x20.
    static_assert(offsetof(DriveThruTriggerData, mfTimeToActiveation)
                      == offsetof(DriveThruTriggerData, mPosition) + sizeof(Vector3),
                  "DriveThruTriggerData: timer immediately follows the 16B position");

    // VERIFIER FIX (1) PIN: CgsID is u64 (8 bytes); the two id members are contiguous (no spurious
    // pad between them) -- the X360 does std@+0x938 then std@+0x940.
    static_assert(offsetof(DriveThruManager, mDriveThruToTurnOffId)
                      == offsetof(DriveThruManager, mRepairedCarID) + sizeof(CgsID),
                  "mDriveThruToTurnOffId is contiguous with mRepairedCarID (CgsID == 8 bytes)");

    // mpProgressionManager is the final member, laid out after the seven trailing bool flags.
    static_assert(offsetof(DriveThruManager, mpProgressionManager)
                      > offsetof(DriveThruManager, mbNeedToSendMustFixMessage),
                  "mpProgressionManager follows the trailing bool flag block");
    static_assert(offsetof(DriveThruManager, mpProgressionManager) + sizeof(void*)
                      == sizeof(DriveThruManager),
                  "mpProgressionManager is the last member");
}

// ============================================================================
// Construct (X360 0x8236E2E8).
// ============================================================================
void DriveThruManager::Construct(CarSelectManager* lpCarSelectManager,
                                 TrainingManager*  lpTrainingManager,
                                 ModeManager*      lpModeManager,
                                 GameStateModule*  lpGameStateModule,
                                 BrnProgression::ProgressionManager* lpProgressionManager)
{
    mpCarSelectManager   = lpCarSelectManager;
    mpTrainingManager    = lpTrainingManager;
    mpModeManager        = lpModeManager;
    mpGameStateModule    = lpGameStateModule;
    mpProgressionManager = lpProgressionManager;

    for (u32 luDriveThruIndex = 0; luDriveThruIndex < KI_MAX_DRIVE_THRUS_IN_THE_WORLD; ++luDriveThruIndex)
    {
        maDriveThruTriggerData[luDriveThruIndex].mfTimeToActiveation = KF_DRIVE_THRU_INACTIVE_TIME;
        maDriveThruTriggerData[luDriveThruIndex].mpGenericRegion     = 0;
    }

    miTotalCarParks    = 0;
    miTotalGasStations = 0;
    miTotalBodyShops   = 0;
    miTotalPaintShops  = 0;
    miTotalJunkYards   = 0;
    miTotalDriveThrus  = 0;

    mRepairedCarID        = static_cast<CgsID>(-1);   // *(this+2360)
    mDriveThruToTurnOffId = static_cast<CgsID>(-1);   // *(this+2368)

    mbNeedToSendDriveThruOffMessage = false;          // +2377
    mbCurrentCarJustRepaired        = false;          // +2376
    mbPlayerCanUseDriveThrus        = false;          // +2378
    mbPlayerCanUseJunkyards         = false;          // +2379
    mbDriveThroughsCloseWhenUsed    = false;          // +2380
    mbIsClosed                      = false;          // +2336

    // X360 BaseCollisionGenerator::Destruct(this+2288) zero-inits the BoxRegion cache.
    std::memset(&mBoxRegionCache, 0, sizeof(mBoxRegionCache));

    maDriveThroughClosed.UnSetAll();                  // *(this+2208)

    miNumDriveThrusOfThisTypeFound = 0;               // +2244
    miTotalDriveThrusOfThisType    = 0;               // +2248
    mbCurrentDriveThruJustFound    = false;           // +2252
    mbNeedToSendCantPaintMessage   = false;           // +2381
    mbNeedToSendMustFixMessage     = false;           // +2382

    meDriveThruCache          = BrnTrigger::GenericRegion::E_TYPE_COUNT;                 // *(this+2324) = 32
    meDiscoveredDriveThruType = static_cast<BrnTrigger::GenericRegion::Type>(32);        // *(this+2240) = 32
}

// ============================================================================
// Prepare (X360 0x8236E3C0).
// ============================================================================
bool DriveThruManager::Prepare(const BrnTrigger::TriggerData* lpTriggerData,
                               CgsResource::ResourcePtr<BrnWorld::GlobalColourPalette> lpPlayerCarColours)
{
    if (lpTriggerData == 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(
            "lpTriggerData != NULL",
            "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/Offences/BrnDriveThruManager.cpp",
            191);
        CgsDev::Assert::EndAssert();
    }

    mpPlayerCarColours = lpPlayerCarColours;

    miTotalCarParks    = 0;
    miTotalGasStations = 0;
    miTotalBodyShops   = 0;
    miTotalPaintShops  = 0;
    miTotalJunkYards   = 0;
    miTotalDriveThrus  = 0;

    std::memset(&mBoxRegionCache, 0, sizeof(mBoxRegionCache));

    const s32 liRegionCount = lpTriggerData->GetGenericRegionCount();
    for (s32 liRegionIndex = 0; liRegionIndex < liRegionCount; ++liRegionIndex)
    {
        const BrnTrigger::GenericRegion* lpGenericRegion = lpTriggerData->GetGenericRegion(liRegionIndex);
        if (lpGenericRegion == 0)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(
                "lpGenericRegion != NULL",
                "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/Offences/BrnDriveThruManager.cpp",
                213);
            CgsDev::Assert::EndAssert();
        }

        if (!lpGenericRegion->IsDriveThru())
            continue;

        if (static_cast<u32>(miTotalDriveThrus) >= KI_MAX_DRIVE_THRUS_IN_THE_WORLD)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(
                "static_cast<uint32_t>( miTotalDriveThrus ) < KI_MAX_DRIVE_THRUS_IN_THE_WORLD",
                "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/Offences/BrnDriveThruManager.cpp",
                217);
            CgsDev::Assert::EndAssert();
        }

        DriveThruTriggerData& lrEntry = maDriveThruTriggerData[miTotalDriveThrus];
        lrEntry.mpGenericRegion     = lpGenericRegion;
        // X360 reads the region's leading BoxRegion position; GenericRegion's BoxRegion subobject is
        // at offset 0 (TriggerRegion::mBoxRegion), reached by name via GetBoxRegion() (no derivation
        // of GenericRegion from BoxRegion in the committed tree, so the X360 cast-to-leading-subobject
        // becomes the GetBoxRegion() accessor -- same address, identical behaviour).
        lrEntry.mPosition           = lpGenericRegion->GetBoxRegion()->GetPosition();
        lrEntry.mfTimeToActiveation = KF_DRIVE_THRU_INACTIVE_TIME;
        ++miTotalDriveThrus;

        switch (lpGenericRegion->GetType())
        {
            case BrnTrigger::GenericRegion::E_TYPE_JUNK_YARD:   ++miTotalJunkYards;    break;
            case BrnTrigger::GenericRegion::E_TYPE_GAS_STATION: ++miTotalGasStations;  break;
            case BrnTrigger::GenericRegion::E_TYPE_BODY_SHOP:   ++miTotalBodyShops;    break;
            case BrnTrigger::GenericRegion::E_TYPE_PAINT_SHOP:  ++miTotalPaintShops;   break;
            case BrnTrigger::GenericRegion::E_TYPE_CAR_PARK:    ++miTotalCarParks;     break;
            default:                                                                   break;
        }
    }

    // The X360 then emits a "Num drive thrus = <n>" debug print gated on the message filter;
    // omitted (pure logging, no game-state effect).

    mRepairedCarID = static_cast<CgsID>(-1);   // *(this+2360)
    mbIsClosed     = false;                     // *(this+2336)
    return true;
}

// ============================================================================
// HandleDriveThru (X360 0x8239B010).
// ============================================================================
void DriveThruManager::HandleDriveThru(
        const BrnTrigger::GenericRegion*                lpRegion,
        const RCEntityActiveRaceCarOutputInterface*     lpActiveRaceCarInterface,
        const BrnResource::VehicleList*                 lpVehicleList,
        GameStateModuleIO::OutputBuffer*                lpOutput)
{
    if (lpRegion == 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(
            "lpGenericRegion != NULL",
            "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/Offences/BrnDriveThruManager.cpp",
            610);
        CgsDev::Assert::EndAssert();
    }
    if (!lpRegion->IsDriveThru())
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(
            "lpGenericRegion->IsDriveThru()",
            "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/Offences/BrnDriveThruManager.cpp",
            611);
        CgsDev::Assert::EndAssert();
    }

    mbIsClosed = false;   // *(this+2336)

    if (!mbPlayerCanUseDriveThrus)   // *(this+2378)
        return;

    const BrnTrigger::GenericRegion::Type leTriggerType = lpRegion->GetType();
    const u8 leType = static_cast<u8>(leTriggerType);
    BrnProgression::Profile* lpProfile = mpProgressionManager->GetProfile();
    const CgsID lRegionId = lpRegion->GetId();

    // ---- first-time discovery ---------------------------------------------
    if (!lpProfile->IsDriveThruDiscoverd(lRegionId, leTriggerType))
    {
        // OnDriveThru records the discovery + posts via the output buffer's queue
        // (X360 derives the queue from BrnGameState::GameStateModuleIO::Ou(output)).
        GameStateModuleIO::GameActionQueue* lpQueueHandle =
            reinterpret_cast<GameStateModuleIO::GameActionQueue*>(lpOutput->GetGameActionQueue());
        mpProgressionManager->OnDriveThru(lRegionId, leTriggerType, lpQueueHandle);

        const s32 liNumDiscovered = lpProfile->GetNumDriveThrusDiscovered(leTriggerType);
        meDiscoveredDriveThruType      = leTriggerType;            // *(this+2240)
        mbCurrentDriveThruJustFound    = true;                     // *(this+2252)
        miNumDriveThrusOfThisTypeFound = liNumDiscovered;          // *(this+2244)
        miTotalDriveThrusOfThisType    = GetTotalDriveThrusOfType(leTriggerType); // *(this+2248)

        if (leTriggerType == BrnTrigger::GenericRegion::E_TYPE_CAR_PARK &&
            miTotalCarParks == liNumDiscovered)
        {
            mpProgressionManager->GetAchievementManager()->OnFindAllCarParks();
        }
    }

    // ---- locate this region's entry ---------------------------------------
    s32 liDriveThruIndex = 0;
    for (; liDriveThruIndex < miTotalDriveThrus; ++liDriveThruIndex)
    {
        if (maDriveThruTriggerData[liDriveThruIndex].mpGenericRegion == lpRegion)
            break;
    }
    if (liDriveThruIndex >= miTotalDriveThrus)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(
            "Failed to find drive thru",
            "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/Offences/BrnDriveThruManager.cpp",
            652);
        CgsDev::Assert::EndAssert();
    }

    // Junkyards (type 0) only proceed when junkyards are enabled; shops proceed unconditionally.
    if (!mbPlayerCanUseJunkyards && leType == 0)
        return;

    // ---- auto-repair (the X360 v14==3 branch) pre-checks -------------------
    // FLAG: the asm compares meType against the literal 3. In the committed GenericRegion enum
    // value 3 == E_TYPE_PAINT_SHOP, yet the body does auto-repair/vehicle-list/deform work (a
    // body-shop concern). The enum<->shop mapping past the queue dispatch is NOT fully pinned by
    // the exports; the literal is reproduced faithfully and flagged.
    if (leType == 3)
    {
        // EActiveRaceCarIndex is a GLOBAL-scope enum (GameSource/BurnoutConstants.h:
        // `enum EActiveRaceCarIndex : s32`), NOT a member of the interface. GetPlayerActiveRaceCarIndex()
        // returns the global enum and GetCarModelId(EActiveRaceCarIndex) consumes it; the prior
        // `RCEntityActiveRaceCarOutputInterface::EActiveRaceCarIndex` did not name a type under
        // /permissive-.
        const EActiveRaceCarIndex leCarIndex =
            lpActiveRaceCarInterface->GetPlayerActiveRaceCarIndex();
        const CgsID lPlayerCarID = lpActiveRaceCarInterface->GetCarModelId(leCarIndex);

        const s32 liVehicleIndex = lpVehicleList->GetVehicleIndex(lPlayerCarID);
        const BrnResource::VehicleListEntry* lpVehicleData =
            (liVehicleIndex < 0) ? 0 : lpVehicleList->GetVehicleData(liVehicleIndex);
        if (lpVehicleData == 0)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(
                "lpVehicleList->GetVehicleData( lPlayerCarID ) != NULL",
                "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/Offences/BrnDriveThruManager.cpp",
                666);
            CgsDev::Assert::EndAssert();
        }

        // X360 reads VehicleListEntry word +0x94 bit 6 ("can be auto-repaired?"):
        // `lwz r11,0x94(r3); extrwi r11,r11,1,25` == `(word_0x94 >> 6) & 1` (LSB-numbered bit 6),
        // NOT bit 1 as previously flagged. CanAutoRepair() is bodied in VehicleListEntry's own home.
        if (!lpVehicleData->CanAutoRepair())   // FLAG: VehicleListEntry word +0x94 bit 6 (>>6 & 1) (name not in exports)
        {
            mbNeedToSendCantPaintMessage = true;   // *(this+2381)
            return;
        }
        if (lpProfile->GetPlayerBaseDeformAmount(lPlayerCarID) > 0.0f)
        {
            mbNeedToSendMustFixMessage = true;     // *(this+2382)
            return;
        }
        // X360: optional ModeManager virtual hook (*(*(this+2348)+3480)->vtbl+0x64)().
        mpModeManager->OnDriveThruRepairAvailable();  // FLAG: ModeManager hook (name/dispatch not in exports)
    }

    // ---- arm / cache the region timer -------------------------------------
    DriveThruTriggerData& lrEntry = maDriveThruTriggerData[liDriveThruIndex];
    bool lbCachedForProcessing = false;

    if (lrEntry.mfTimeToActiveation < 0.0f)   // not currently armed
    {
        if (!maDriveThroughClosed.IsBitSet(static_cast<u32>(liDriveThruIndex)))
        {
            if (leType != 0)   // non-junkyard
            {
                lrEntry.mfTimeToActiveation = KF_DRIVE_THRU_PRESENTATION_TIME;   // 3.0
                if (mbDriveThroughsCloseWhenUsed && leType == 2)   // body shop closes-when-used
                {
                    maDriveThroughClosed.SetBit(static_cast<u32>(liDriveThruIndex));
                    mbNeedToSendDriveThruOffMessage = true;      // *(this+2377)
                    mDriveThruToTurnOffId = lpRegion->GetId();   // *(this+2368)
                }
            }
            mBoxRegionCache  = *lpRegion->GetBoxRegion();   // 9-word copy of the leading BoxRegion subobject  // 9-word copy
            meDriveThruCache = leTriggerType;                                          // *(this+2324)
            mIdCache         = lpRegion->GetId();                                      // *(this+2328)
            lbCachedForProcessing = true;
        }
    }

    if (!lbCachedForProcessing)
    {
        if (maDriveThroughClosed.IsBitSet(static_cast<u32>(liDriveThruIndex)))
        {
            mbIsClosed       = true;     // *(this+2336)
            meDriveThruCache = leTriggerType;
            mBoxRegionCache  = *lpRegion->GetBoxRegion();   // 9-word copy of the leading BoxRegion subobject
            mIdCache         = lpRegion->GetId();
        }
        else
        {
            mbIsClosed = false;
        }

        const f32 lfTimer = lrEntry.mfTimeToActiveation;
        if (lfTimer < KF_DRIVE_THRU_JUST_ARMED_BOUND && leType != 0 && lfTimer < KF_DRIVE_THRU_REARM_TIME)
            lrEntry.mfTimeToActiveation = KF_DRIVE_THRU_REARM_TIME;   // 1.0
    }

    if (leType == 2)   // body shop -> auto-repair training
        PlayAutoRepairTraining(lpActiveRaceCarInterface, lpVehicleList);
}

// ============================================================================
// ProcessDriveThru (X360 0x8239B6E8).
// ============================================================================
void DriveThruManager::ProcessDriveThru(BrnTrigger::GenericRegion::Type leTriggerType,
                                        GameStateModuleIO::GameActionQueue*   lpActionQueue,
                                        CgsSystem::TimerRequestInterface* lpTimerRequestInterface,
                                        RCEntityActiveRaceCarOutputInterface* lpRcOutputInterface,
                                        bool  lbIsOnline,
                                        bool* lpbIsInJunkyard)
{
    (void)lpRcOutputInterface;
    GameActionQueueImpl* lpQueue = AsActionQueue(lpActionQueue);

    if (mbIsClosed)   // *(this+2336): a closed drive-thru cancels the presentation
    {
        mbIsClosed = false;
        u8 lac[1] = { 0 };
        lpQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(lac), KI_ACTION_STOP_DRIVE_THRU_PRES, 1);
        return;
    }

    switch (leTriggerType)
    {
        case BrnTrigger::GenericRegion::E_TYPE_JUNK_YARD:   // case 0
        {
            // Hand the cached junkyard id to the car-select junkyard flow.
            mpCarSelectManager->EnterJunkyard(lpActionQueue, mIdCache);
            TryPlayTrainingTip(mpTrainingManager, KI_TRAIN_JUNKYARD);
            *lpbIsInJunkyard = true;
            break;
        }
        case BrnTrigger::GenericRegion::E_TYPE_GAS_STATION: // case 1
        {
            PostShopAction(lpQueue, mBoxRegionCache, KI_ACTION_GAS_STATION_DRIVE_THRU, lbIsOnline);
            if (!lbIsOnline)
                SetPlayerCarDriver(lpActionQueue, lpTimerRequestInterface, &mBoxRegionCache, false, 0.0f);
            TryPlayTrainingTip(mpTrainingManager, KI_TRAIN_GAS);
            break;
        }
        case BrnTrigger::GenericRegion::E_TYPE_BODY_SHOP:   // case 2 (auto-repair)
        {
            BrnProgression::CarData* lpCarData = mpProgressionManager->GetCurrentCarData();
            if (lpCarData != 0)
            {
                PostShopAction(lpQueue, mBoxRegionCache, KI_ACTION_BODY_SHOP_DRIVE_THRU, lbIsOnline);

                mpProgressionManager->GetAchievementManager()->OnBodyShop(mpModeManager->GetCurrentGameModeType());
                mpModeManager->GetScoringSystem()->ResetRoadRageCrashesForPlayer();

                if (!lbIsOnline)
                    SetPlayerCarDriver(lpActionQueue, lpTimerRequestInterface, &mBoxRegionCache, false, 0.0f);

                const CgsID lCarId = lpCarData->GetId();
                BrnProgression::Profile* lpProfile = mpProgressionManager->GetProfile();
                mbCurrentCarJustRepaired = (lpProfile->GetPlayerBaseDeformAmount(lCarId) > 0.0f);  // *(this+2376)
                mRepairedCarID = lCarId;                                                            // *(this+2360)
                mpProgressionManager->RepairUnlockedVehicle(lCarId);

                u8 lacAuto[1] = { 0 };
                lpQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(lacAuto), KI_ACTION_REQUEST_AUTO_SAVE, 1);
            }
            break;
        }
        case BrnTrigger::GenericRegion::E_TYPE_PAINT_SHOP:  // case 3 (paint)
        {
            BrnProgression::CarData* lpCarData = mpProgressionManager->GetCurrentCarData();
            if (lpCarData != 0)
            {
                const CgsID lCarId = lpCarData->GetId();

                s32 liColourIndex  = 0;
                s32 liPaletteIndex = 0;
                mpProgressionManager->GetCarColourAndPalette(lCarId, &liColourIndex, &liPaletteIndex);
                liPaletteIndex = 2;   // X360 forces palette index 2 (the paint-shop palette)

                // Advance the colour by one within palette 2's colour count (wrap-around). The X360
                // traps on a zero divisor (__twllei).
                const s32 liNumColours = mpPlayerCarColours.operator->()->maPalettes[2].miNumColours;
                if (liNumColours <= 0)
                {
                    CgsDev::Assert::BeginAssert();
                    CgsDev::Assert::FireAssert(
                        "GlobalColourPalette palette[2] colour count > 0",
                        "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/Offences/BrnDriveThruManager.cpp",
                        0 /*FLAG: assert line not in exports*/);
                    CgsDev::Assert::EndAssert();
                }
                else
                {
                    liColourIndex = (liColourIndex + 1) % liNumColours;
                }

                PostShopAction(lpQueue, mBoxRegionCache, KI_ACTION_PAINT_SHOP_DRIVE_THRU, lbIsOnline);

                lpCarData->SetColourIndex(liColourIndex);
                lpCarData->SetPaletteIndex(liPaletteIndex);

                u8 lacAuto[1] = { 0 };
                lpQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(lacAuto), KI_ACTION_REQUEST_AUTO_SAVE, 1);

                if (!lbIsOnline)
                    SetPlayerCarDriver(lpActionQueue, lpTimerRequestInterface, &mBoxRegionCache, false, 0.0f);

                TryPlayTrainingTip(mpTrainingManager, KI_TRAIN_PAINT);
            }
            break;
        }
        case BrnTrigger::GenericRegion::E_TYPE_CAR_PARK:    // case 4
        {
            TryPlayTrainingTip(mpTrainingManager, KI_TRAIN_CAR_PARK);
            break;
        }
        default:
            break;
    }
}

// ============================================================================
// Update (X360 0x8239EEF0).
// ============================================================================
void DriveThruManager::Update(GameStateModuleIO::GameActionQueue*  lpActionQueue,
                              CgsSystem::TimerRequestInterface* lpTimerRequestInterface,
                              f32  lfTimeStep,
                              RCEntityActiveRaceCarOutputInterface* lpRcOutputInterface,
                              bool lbIsOnline,
                              bool lbIsFreeburn,
                              bool lbIsShowtiming,
                              bool lbIsAnythingPaused,
                              bool lbIsInJunkyard,
                              bool lbInviteInProgress,
                              const BrnResource::VehicleList* lpVehicleList)
{
    GameActionQueueImpl* lpQueue = AsActionQueue(lpActionQueue);

    BrnProgression::Profile* lpProfile = mpProgressionManager->GetProfile();
    if (lpProfile == 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(
            "lpProfile",
            "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/Offences/BrnDriveThruManager.cpp",
            338);
        CgsDev::Assert::EndAssert();
    }

    // ---- recompute the can-use flags --------------------------------------
    const bool lbPlayerCrashing = lpRcOutputInterface->IsPlayerCarCrashing();
    const bool lbCanUseBase = (!lbIsFreeburn) && (!lbPlayerCrashing);
    mbPlayerCanUseDriveThrus = lbCanUseBase;                          // *(this+2378)
    mbPlayerCanUseJunkyards =                                         // *(this+2379)
        lbCanUseBase && lbIsShowtiming && (!lbInviteInProgress) &&
        ((!lbIsAnythingPaused) || (!lbIsOnline));

    // ---- run any cached drive-thru ----------------------------------------
    if (meDriveThruCache != BrnTrigger::GenericRegion::E_TYPE_COUNT)  // *(this+2324) != 32
    {
        ProcessDriveThru(meDriveThruCache, lpActionQueue, lpTimerRequestInterface,
                         lpRcOutputInterface, lbIsInJunkyard, &lbIsInJunkyard);
        meDriveThruCache = BrnTrigger::GenericRegion::E_TYPE_COUNT;   // reset to 32
    }

    // ---- age timers + run proximity discovery over every entry ------------
    // FLAG: KAF_MAX_DRIVE_THRU_EXIT_SPEEDS[46] (per-type exit-speed table, rodata unk_8202AD38)
    // is not in the exports; the exit speed handed to SetPlayerCarDriver is modelled as 0.0.
    for (s32 liIndex = 0; liIndex < static_cast<s32>(KI_MAX_DRIVE_THRUS_IN_THE_WORLD); ++liIndex)
    {
        DriveThruTriggerData& lrEntry = maDriveThruTriggerData[liIndex];
        const f32 lfTimerBefore = lrEntry.mfTimeToActiveation;
        const u8  leType = lrEntry.mpGenericRegion ? static_cast<u8>(lrEntry.mpGenericRegion->GetType())
                                                   : static_cast<u8>(BrnTrigger::GenericRegion::E_TYPE_COUNT);

        if (lfTimerBefore >= 0.0f && leType != 4)   // armed, non-car-park
        {
            const f32 lfTimerAfter = lfTimerBefore - lfTimeStep;
            lrEntry.mfTimeToActiveation = lfTimerAfter;
            if (lfTimerBefore > KF_DRIVE_THRU_ACTIVATION_TIME && lfTimerAfter <= KF_DRIVE_THRU_ACTIVATION_TIME)
            {
                u8 lac[1] = { 0 };
                lpQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(lac), KI_ACTION_STOPPED_DRIVE_THRU, 1);
                if (!lbIsOnline)
                {
                    const f32 lfExitSpeed = 0.0f * KF_DRIVE_THRU_EXIT_SPEED_SCALE;  // FLAG: table not in exports
                    SetPlayerCarDriver(lpActionQueue, lpTimerRequestInterface,
                                       lrEntry.mpGenericRegion->GetBoxRegion(),   // leading BoxRegion subobject
                                       true, lfExitSpeed);
                    if (mbCurrentCarJustRepaired)   // *(this+2376)
                    {
                        UnlockCarChallengeForCar(mRepairedCarID, lpActionQueue);
                        mbCurrentCarJustRepaired = false;
                    }
                }
            }
        }

        // Proximity discovery (only when not in a junkyard and the player car is active).
        if (!lbIsInJunkyard && lbIsOnline && leType != 4)
        {
            if (lpRcOutputInterface->IsPlayerCarActive())
            {
                const Vector3 lPlayerPos = lpRcOutputInterface->GetPlayerPosition();
                const Vector3 lDelta = rw::math::vpu::operator-(lrEntry.mPosition, lPlayerPos);
                const f32 lfSeparationXZSq = (lDelta.x * lDelta.x) + (lDelta.z * lDelta.z);
                const f32 lfSeparationY    = (lDelta.y < 0.0f) ? -lDelta.y : lDelta.y;

                if (lfSeparationXZSq < KF_DRIVE_THRU_DISCOVERY_DISTANCE_SQ &&
                    lfSeparationY < KF_DRIVE_THRU_DISCOVERY_MAX_Y)
                {
                    const BrnTrigger::GenericRegion::Type leRegionType =
                        static_cast<BrnTrigger::GenericRegion::Type>(leType);
                    const CgsID lDriveThruID = lrEntry.mpGenericRegion->GetId();
                    if (!lpProfile->IsDriveThruDiscoverd(lDriveThruID, leRegionType))
                    {
                        mpProgressionManager->OnDriveThru(lDriveThruID, leRegionType, lpActionQueue);
                        mbCurrentDriveThruJustFound    = true;
                        miNumDriveThrusOfThisTypeFound = lpProfile->GetNumDriveThrusDiscovered(leRegionType);
                        miTotalDriveThrusOfThisType    = GetTotalDriveThrusOfType(leRegionType);
                        meDiscoveredDriveThruType      = leRegionType;
                    }
                }
            }
        }
    }

    // ---- post the "discovered" HUD action + per-type training tips --------
    // X360 also gates on (player active) AND (player engine state == 2 / engine on).
    if (mbCurrentDriveThruJustFound && !lbIsInJunkyard &&
        lpRcOutputInterface->IsPlayerCarActive() && lpRcOutputInterface->IsPlayerEngineOn())
    {
        s32 lacDiscovered[3];
        lacDiscovered[0] = static_cast<s32>(meDiscoveredDriveThruType);  // *(this+2240) -> [0]
        lacDiscovered[1] = miTotalDriveThrusOfThisType;                  // *(this+2248) -> [2]
        lacDiscovered[2] = miNumDriveThrusOfThisTypeFound;              // *(this+2244) -> [1]
        // NOTE: the X360 writes v80[0]=type, v80[2]=found, v80[1]=total; the slot order is preserved
        // here (the payload struct's exact field names are not in the exports).
        lpQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(lacDiscovered), KI_ACTION_DRIVE_THRU_DISCOVERED, 12);

        switch (meDiscoveredDriveThruType)
        {
            case BrnTrigger::GenericRegion::E_TYPE_GAS_STATION: TryPlayTrainingTip(mpTrainingManager, KI_TRAIN_GAS); break;
            case BrnTrigger::GenericRegion::E_TYPE_BODY_SHOP:   PlayAutoRepairTraining(lpRcOutputInterface, lpVehicleList); break;
            case BrnTrigger::GenericRegion::E_TYPE_PAINT_SHOP:  TryPlayTrainingTip(mpTrainingManager, KI_TRAIN_PAINT); break;
            default: break;
        }

        // "All drive-thrus found" check (skipped for car parks).
        if (meDiscoveredDriveThruType != BrnTrigger::GenericRegion::E_TYPE_CAR_PARK)
        {
            bool lbAreAllDriveThrusFound = true;
            for (s32 liTriggerTypeIndex = 0; liTriggerTypeIndex <= 3 && lbAreAllDriveThrusFound; ++liTriggerTypeIndex)
            {
                const s32 liFound = lpProfile->GetNumDriveThrusDiscovered(
                    static_cast<BrnTrigger::GenericRegion::Type>(liTriggerTypeIndex));
                switch (liTriggerTypeIndex)
                {
                    case 0: if (liFound < miTotalJunkYards)   lbAreAllDriveThrusFound = false; break;
                    case 1: if (liFound < miTotalGasStations) lbAreAllDriveThrusFound = false; break;
                    case 2: if (liFound < miTotalBodyShops)   lbAreAllDriveThrusFound = false; break;
                    case 3: if (liFound < miTotalPaintShops)  lbAreAllDriveThrusFound = false; break;
                    default:
                        CgsDev::Assert::BeginAssert();
                        CgsDev::Assert::FireAssert(
                            "Unknown trigger type \n",
                            "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/Offences/BrnDriveThruManager.cpp",
                            535);
                        CgsDev::Assert::EndAssert();
                        break;
                }
            }
            if (lbAreAllDriveThrusFound)
            {
                u8 lac[1] = { 0 };
                lpQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(lac), KI_ACTION_ALL_DRIVE_THRUS_FOUND, 1);
            }
        }
        mbCurrentDriveThruJustFound = false;   // *(this+2252)
    }

    // ---- drain the deferred HUD / close messages --------------------------
    if (mbNeedToSendCantPaintMessage)          // *(this+2381)
    {
        u8 lac[1] = { 0 };
        lpQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(lac), KI_ACTION_HUD_CANT_PAINT, 1);
        mbNeedToSendCantPaintMessage = false;
    }
    if (mbNeedToSendMustFixMessage)            // *(this+2382)
    {
        u8 lac[1] = { 0 };
        lpQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(lac), KI_ACTION_HUD_MUST_FIX_CAR, 1);
        mbNeedToSendMustFixMessage = false;
    }
    if (mbNeedToSendDriveThruOffMessage)       // *(this+2377)
    {
        CgsID lTurnOffId = mDriveThruToTurnOffId;   // *(this+2368), 8-byte payload
        lpQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lTurnOffId), KI_ACTION_CLOSE_DRIVE_THRU, 8);
        mbNeedToSendDriveThruOffMessage = false;
    }
}

// ============================================================================
// UnlockCarChallengeForCar (X360 0x82386840).
// ============================================================================
void DriveThruManager::UnlockCarChallengeForCar(CgsID lRepairedCarID, GameStateModuleIO::GameActionQueue* lpActionQueue)
{
    GameActionQueueImpl* lpQueue = AsActionQueue(lpActionQueue);

    const BrnProgression::ProgressionData* lpProgressionData = mpProgressionManager->GetProgressionData();
    if (lpProgressionData == 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(
            "lpProgressionData",
            "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/Offences/BrnDriveThruManager.cpp",
            999);
        CgsDev::Assert::EndAssert();
    }

    const u32 luEventJunctionCount = lpProgressionData->GetEventJunctionCount();
    const BrnProgression::EventJunction* lpEventJunction = 0;
    const BrnProgression::RaceEventData* lpRaceEventData = 0;

    u32 luEventJunctionIndex = 0;
    for (; luEventJunctionIndex < luEventJunctionCount; ++luEventJunctionIndex)
    {
        lpEventJunction = lpProgressionData->GetEventJunction(luEventJunctionIndex);
        if (lpEventJunction == 0)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(
                "lpEventJunction",
                "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/Offences/BrnDriveThruManager.cpp",
                1008);
            CgsDev::Assert::EndAssert();
        }
        lpRaceEventData = lpEventJunction->GetOfflineEvent();
        if (lpRaceEventData == 0)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(
                "lpRaceEventData",
                "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/Offences/BrnDriveThruManager.cpp",
                1011);
            CgsDev::Assert::EndAssert();
        }
        if (lpRaceEventData->GetUnlockCarId() == lRepairedCarID)   // RaceEventData word +0x14 == a2
            break;
    }
    if (luEventJunctionIndex >= luEventJunctionCount)
        return;

    // Resolve the ProfileEvent for this junction (X360 linear search of the profile's race-event
    // id table; word +1000 == count, +29168 == id table base).
    BrnProgression::Profile* lpProfile = mpProgressionManager->GetProfile();
    BrnProgression::ProfileEvent* lpProfileEvent =
        lpProfile->FindProfileEventByRaceEventId(lpEventJunction->GetEventId());
    if (lpProfileEvent == 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(
            "lpProfileEvent",
            "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/Offences/BrnDriveThruManager.cpp",
            1021);
        CgsDev::Assert::EndAssert();
    }

    if (!lpProfileEvent->IsFound())   // ProfileEvent flag bit 0 (X360 lhz +4, bit0)
    {
        lpProfileEvent->SetFound(true);
        lpProfile->IncrementNumDiscoveredEvents();   // *(Profile + 580)++

        // SendJunctionPlayerIsAtAction (40 bytes); only the junction id (+0x14) is X360-attested.
        // FLAG: full struct layout not in exports.
        u8 lacJunction[40];
        std::memset(lacJunction, 0, sizeof(lacJunction));
        const CgsID lJunctionId = lpEventJunction->GetId();
        std::memcpy(lacJunction + 0x14, &lJunctionId, sizeof(lJunctionId));
        lpQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(lacJunction), KI_ACTION_SEND_JUNCTION_PLAYER_AT, 40);

        u8 lacAuto[1] = { 0 };
        lpQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(lacAuto), KI_ACTION_REQUEST_AUTO_SAVE, 1);

        mpGameStateModule->CheckForAllEventsBeingFound(lpProfile, lpActionQueue);
    }
}

// ============================================================================
// GetTotalDriveThrusOfType (X360 0x82356468).
// ============================================================================
s32 DriveThruManager::GetTotalDriveThrusOfType(BrnTrigger::GenericRegion::Type leTriggerType)
{
    s32 liResult;
    switch (leTriggerType)
    {
        case BrnTrigger::GenericRegion::E_TYPE_JUNK_YARD:   liResult = miTotalJunkYards;    break;
        case BrnTrigger::GenericRegion::E_TYPE_GAS_STATION: liResult = miTotalGasStations;  break;
        case BrnTrigger::GenericRegion::E_TYPE_BODY_SHOP:   liResult = miTotalBodyShops;    break;
        case BrnTrigger::GenericRegion::E_TYPE_PAINT_SHOP:  liResult = miTotalPaintShops;   break;
        case BrnTrigger::GenericRegion::E_TYPE_CAR_PARK:    liResult = miTotalCarParks;     break;
        default:                                            liResult = 0;                   break;
    }
    return liResult;
}
}
