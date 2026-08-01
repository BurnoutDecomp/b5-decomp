// ============================================================================
// GameSource/Director/BrnDirectorResourceManager.cpp
//
// BrnDirector::DirectorResourceManager -- the out-of-line bodies.
//
// ⭐ REWRITTEN 2026-07-31 (shot-group wave). What used to be in this file was a
// reconstruction of the CONSTRUCTOR @0x827DEB98 written through raw console byte offsets
// (`*(void**)(base + 552) = vtable; for (off = 568; off <= 1592; off += 16) ...`), over a
// LOCAL `struct DirectorResourceManager { DirectorResourceManager(); };` re-declaration.
// It was never mountable: those are 4-byte-pointer console offsets, so on x64 it would
// have scribbled across the live class and the DirectorModule that embeds it, and its
// ctor collided at link with the header's implicit one (LNK2005 vs BrnGameModule.obj).
//
// That whole TU is now OBSOLETE rather than merely wrong: the header declares all 65
// shot-group slots as real members, so the console's "default-construct 65 sub-objects at
// stride 0x10, one of them with a different element ctor" IS the compiler-generated
// default constructor, member by member, in the same order. There is nothing left for a
// hand-written ctor to do. (The element ctors it called through -- sub_827DC838 and
// sub_827DC8C8 -- are now named: Attrib::Gen::shotgroup(Collection*, owner) and
// Attrib::Gen::cameradefaults(Collection*, owner).)
//
// ⚠️ ONE CONSOLE BEHAVIOUR THE IMPLICIT CTOR DROPS, deliberately: the console ctor also
// clears bytes 1608..1627 (std/std/stw == the whole 20-byte Attrib::RefSpec
// mAfterTouchCam). Attrib::RefSpec's own default ctor zeroes all three of its fields by
// name, so the effect is reproduced -- but by RefSpec, not here.
//
// What lives here now are the manager's genuinely out-of-line members. Two of them are
// X360-attested symbols; the rest are the ones the header cannot define inline because
// BrnDirector::ICEWrapper is only forward-declared there (its home drags the whole ICE
// manager/camera/editor cone and forward-declares this class in return).
//
// ⭐ MOUNTED 2026-08-01 (Prepare wave). The blocker was never this file's own code: it was
// that the five ICE-cone bodies dragged six unresolved externals (MakeICEMovieId,
// ICEWrapper::GetICETakeData / GetShakeGroup / GetAuthor,
// ICEAuthor::FindEditedTakeFromGuid, ICEList::GetICETakeDataFromGuid) into the link. Those
// five moved VERBATIM to BrnDirectorResourceManagerICE.cpp, which stays out of the link;
// nothing was stubbed and nothing was invented. What is left here -- GetEventIntroShots and
// the real Prepare @0x8225CA08 -- is self-contained.
// ============================================================================

#include "GameSource/Director/BrnDirectorResourceManager.h"
#include "GameSource/Director/DirectorModule/BrnDirectorModuleIOOutputBuffer.hpp"  // Prepare's r4
#include "GameSource/Resource/SharedIO/BrnGameDataRequestQueueImpl.h"  // RequestInterface<512> builders
#include "GameSource/Resource/SharedIO/BrnGameDataEvents.h"            // GameDataAssetEvent (the two list replies)
#include "GameShared/GameClasses/System/Resource/CgsResourceIOEvents.h"// AcquireResourceResponse
#include "GameShared/GameClasses/System/AttribSys/CgsAttribSysModuleIO.h" // AttribSysRequestInterface<512>::RegisterVault
#include "GameShared/GameClasses/Core/CgsAssert.h"                     // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"             // the one-shot bank diagnostic
#include "GameSource/AttribSys/Generated/classes/aftertouchcam.h"      // the mAfterTouchCam class key
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/AttributeKey.h"  // Attrib::StringToKey

namespace BrnDirector
{

namespace
{
    // Prepare's own immediates, named. All four are literal `li` operands in the asm.
    const s32 KI_EVENTID_CAMERA_VAULT   = 0;   // the AcquireResource("CameraVault") event id
    const s32 KI_EVENTID_VEHICLE_LIST   = 1;   // GetVehicleList's
    const s32 KI_EVENTID_ICE_LIST       = 2;   // GetICEList's
    const s32 KI_CAMERA_VAULT_POOL_ID   = 5;   // `li r10, 5` -- the GameData resource pool

    // The receiver-queue event TYPE ids the stage-2 drain switches on (the `cmpwi r3, N`
    // chain at 0x8225CB64): the shared CgsResource acquire reply, and the two GameData
    // response ids GameDataModule::ProcessGetGameDataEvent stamps (52 VehicleList,
    // 64 ICEList).
    const s32 KI_EVENT_ACQUIRE_RESOURCE = 4;
    const s32 KI_EVENT_GET_VEHICLE_LIST = 52;
    const s32 KI_EVENT_GET_ICE_LIST     = 64;

    // ========================================================================================
    // [RESOLVED 2026-08-01 -- the PC DATA GATE that used to live here is GONE]
    //
    // Prepare bakes 100 asserts in stage 4: 63 IsValid() statements (over 26 distinct slots,
    // mRankUpGroup twice thanks to the console's own copy-paste bug) and 37 Num_ShotList()
    // minimum-count statements. Until today the count half could not pass and was held behind
    // a KB_PC_ATTRIB_ARRAY_LENGTHS_VALID bool, because every array attribute in every ported
    // vault reported exactly ONE element.
    //
    // The cause was in the PC vault PORT, and the previous diagnosis of it was WRONG in a way
    // worth recording: it read "widen the entry stride to 24 in BOTH the transcoder and
    // Attrib::CollectionLoadData::Entry". The record does NOT widen. The serialised entry is
    // 16 bytes on x64 exactly as on the X360 --
    //     { u64 mKey; u32 muValue; u16 muTypeIndex; u8 mu8Flags; u8 mu8Pad }
    // -- because muValue is a POINTER SLOT INSIDE A SERIALISED RECORD, and Vault::Initialize's
    // PtrN type-3 case rebases it with a 32-BIT store (attribloadandgo.cpp; the resource heap
    // lives below 4 GB, the committed PointerFromU32 convention). attribsys_transcode.py was
    // byte-swapping that slot as one u64 because a PtrN record names it, which walked
    // mu8Flags off +0x0E; Attrib::Node::GetCount then took its `(muFlags & 0x2) == 0`
    // non-array exit for every attribute in the vault.
    //
    // Fixed in the porter (attribsys_transcode.py walk_attribute_header) and CAMERAS.BUNDLE /
    // SURFACELIST.BIN / WORLDVAULT.BIN re-emitted. Oracle: over the 74 entries the X360 and
    // the Remaster's shipped LE vaults share by collection key, {u32,u16,u8,u8} agrees 74/74
    // and one-u64 0/74. mGameIntroGroup("606002") now reports 3 shots -- guids 610132
    // Intro_FlyCam_Loop / 605855 DMV_IntroA / 605858 DMV_IntroB -- and all 37 count asserts
    // are armed and passing. Nothing in this file or in the AttribSys runtime changed.
    // ========================================================================================
}


// ⚠️ THE FIVE ICE-CONE BODIES MOVED OUT 2026-08-01, verbatim, to the sibling
// BrnDirectorResourceManagerICE.cpp (GetKeyAnim x2 / GetShakeTakes / GetICEAuthor /
// GetKeyAnimFromGuid). They are ONE console TU with what is left here; the split exists
// only so this file can be mounted for Prepare without pulling six unresolved ICE externals
// into the link, and it must be merged back when the ICE take-runtime group lands. See that
// file's header for why a split and not six link stubs.

// ----------------------------------------------------------------------------
// @0x821F6AB8 -- BrnDirector::DirectorResourceManager::GetEventIntroShots.
//
// Reconstructed 2026-07-31 from the export, jump table and all: 18 cases over
// jpt_821F6AF0, r4 = the mode, r5 = lbCarInFront, every arm an `addi r3,r31,<offset>`.
// It is one of only SEVEN out-of-line DirectorResourceManager symbols in the image.
//
// ⭐ IT IS ALSO AN INDEPENDENT CONFIRMATION OF NINE MEMBER OFFSETS: 0x238/0x248/0x268/
// 0x278/0x2B8/0x2D8/0x2E8 and 0x258 land exactly on mRaceStartGroup /
// mRoadRageStartGroup / mOnlineRaceStart / mBurningRouteStartGroup /
// mOnlineLobbyStartGroup / mStuntRaceStartGroup / mMarkedManStartGroup /
// mRaceStartRivalInFrontGroup, which is the head of the DWARF declaration order the
// 65-slot table is built on.
//
// ⚠️ TWO CONSOLE ODDITIES, REPRODUCED, DO NOT "FIX":
//   * E_MODE_ELIMINATOR (6) is NOT a case -- it falls into the default arm and fires the
//     "Invalid Event Requested" assert (cpp:615) before returning the race group. The
//     manager has an mEliminatorStartGroup (+664) and this function never returns it.
//   * mSurvivorStartGroup (+648), mTrafficAttackStartGroup (+680) and mPursuitStartGroup
//     (+712) are likewise never returned here: E_MODE_PURSUIT (4) and
//     E_MODE_TRAFFIC_ATTACK (9) both route to the plain race pair instead.
// (Both are consistent with those four groups being driven from somewhere else, or with
// them being dead vault entries; either way this function's behaviour is what it is.)
//
// ⚠️ The default arm falls THROUGH the assert into the same tail as 0/1/4/9 -- it does
// not return early. That is the console's `goto LABEL_9`.
// ----------------------------------------------------------------------------
const Attrib::Gen::shotgroup& DirectorResourceManager::GetEventIntroShots(
    s32 liEventMode, bool lbCarInFront) const
{
    switch (liEventMode)
    {
    case 2:     // E_MODE_OFFLINE_SHOWTIME
    case 16:    // E_MODE_ONLINE_SHOWTIME
        return mRaceStartGroup;                       // +568  (0x238)

    case 3:     // E_MODE_ROAD_RAGE
        return mRoadRageStartGroup;                   // +584  (0x248)

    case 5:     // E_MODE_BURNING_ROUTE
        return mBurningRouteStartGroup;               // +632  (0x278)

    case 7:     // E_MODE_STUNT_ATTACK
        return mStuntRaceStartGroup;                  // +728  (0x2D8)

    case 8:     // E_MODE_MARKED_MAN
        return mMarkedManStartGroup;                  // +744  (0x2E8)

    case 10:    // E_MODE_ONLINE_RACE
    case 11:    // E_MODE_ONLINE_ROAD_RAGE
    case 12:    // E_MODE_ONLINE_FUGITIVE
    case 13:    // E_MODE_ONLINE_BURNING_HOME_RUN
    case 14:    // E_MODE_ONLINE_FREE_BURN
    case 17:    // E_MODE_ONLINE_MODE_END / E_MODE_COUNT
        return mOnlineRaceStart;                      // +616  (0x268)

    case 15:    // E_MODE_ONLINE_FREE_BURN_LOBBY
        return mOnlineLobbyStartGroup;                // +696  (0x2B8)

    case 0:     // E_MODE_OFFLINE_RACE
    case 1:     // E_MODE_FACE_OFF
    case 4:     // E_MODE_PURSUIT
    case 9:     // E_MODE_TRAFFIC_ATTACK
        break;

    default:    // includes E_MODE_ELIMINATOR (6) -- see the note above
        CGS_ASSERT(false, "Invalid Event Requested");
        break;
    }

    return lbCarInFront ? mRaceStartRivalInFrontGroup   // +600  (0x258)
                        : mRaceStartGroup;              // +568  (0x238)
}


// ----------------------------------------------------------------------------
// [PC diagnostic] one-shot report of how many of the 65 slots actually bound a collection.
// NOT an X360 function. Precedent: GameDataModule::PrepareVehicleList's own "430 vehicles /
// PUSMC01 index=0" probe. Rationale (the standing rule: when data can arrive
// wrong-but-plausible, print it at BOTH ENDS): a slot that misses its collection is
// indistinguishable at runtime from a slot that was never built -- both are null -- so
// without this line a silently empty CameraVault and a silently broken key hash look the
// same. It also prints the ONE slot the whole intro campaign is about.
// ----------------------------------------------------------------------------
void DirectorResourceManager::LogShotGroupBankState() const
{
    static bool sbLogged = false;
    if (sbLogged)
        return;
    sbLogged = true;

    const Attrib::Gen::shotgroup* lapSlots[64] = {
        &mRaceStartGroup, &mRoadRageStartGroup, &mRaceStartRivalInFrontGroup, &mOnlineRaceStart,
        &mBurningRouteStartGroup, &mSurvivorStartGroup, &mEliminatorStartGroup,
        &mTrafficAttackStartGroup, &mOnlineLobbyStartGroup, &mPursuitStartGroup,
        &mStuntRaceStartGroup, &mMarkedManStartGroup, &mBurningRouteFinishGroup,
        &mMarkedManFinishGroup, &mRaceFinishGroup, &mRoadRageFinishGroup, &mStuntFinishGroup,
        &mRaceFinishNorth, &mRaceFinishNorthEast, &mRaceFinishEast, &mRaceFinishSouthEast,
        &mRaceFinishSouth, &mRaceFinishSouthWest, &mRaceFinishWest, &mRaceFinishNorthWest,
        &mRankUpGroup, &mDriveThruGasStationGroup, &mDriveThruBodyShopGroup,
        &mDriveThruTyreShopGroup, &mDriveThruAutoPartsGroup, &mDriveThruTuningShopGroup,
        &mCarSelectMotorCity, &mCarSelectMotorCityRivalUnlock, &mCarSelectWestAcres,
        &mCarSelectWestAcresRivalUnlock, &mCarSelectSouthBay, &mCarSelectSouthBayRivalUnlock,
        &mCarSelectHeartbreak, &mCarSelectHeartbreakRivalUnlock, &mCarSelectLowerPeaks,
        &mCarSelectLowerPeaksRivalUnlock, &mCarSelectIdle, &mCarSelectOutro, &mCarUnlock,
        &mGameIntroGroup, &mBurnoutLicense, &mShakeAnimsGroup, &mJumpRig, &mHardStopWorldLeft,
        &mHardStopWorldRight, &mHardStopCarLeft, &mHardStopCarRight, &mFastCrashShotGroup,
        &mNormalCrashShotGroup, &mSlowCrashShotGroup, &mAfterCrash, &mAfterCrashSafe,
        &mOnlineCarSelect, &mFailsafe, &mTakedown, &mCrashbreaker, &mTakendown,
        &mTestbed, &mTestbed2
    };

    s32 liValid = 0;
    s32 liWithShots = 0;
    for (s32 li = 0; li < 64; ++li)
    {
        if (lapSlots[li]->IsValid())
        {
            ++liValid;
            if (lapSlots[li]->Num_ShotList() > 0)
                ++liWithShots;
        }
    }

    *CgsDev::Log::gpDebugPrint
        << "[Director] shot-group bank: " << liValid << "/64 shotgroups bound ("
        << liWithShots << " with shots), cameradefaults "
        << (mCameraDefaults.IsValid() ? "bound" : "NULL")
        << ", mGameIntroGroup(\"606002\") "
        << (mGameIntroGroup.IsValid() ? "bound" : "NULL")
        << " shots=" << (mGameIntroGroup.IsValid() ? (s32)mGameIntroGroup.Num_ShotList() : -1)
        << "\n";

    // [PC diagnostic -- CONSUMER end] the two GameData list replies stage-2 collected. Print
    // both ends of the seam (GameDataModule::ProcessGetICEListRequest logs the producer end)
    // so a null here bisects "reply never posted" from "reply arrived after the drain".
    *CgsDev::Log::gpDebugPrint
        << "[Director] list replies: mpVehicleList=" << const_cast<void*>(static_cast<const void*>(mpVehicleList))
        << " mpICEDictionaryList=" << const_cast<void*>(static_cast<const void*>(mpICEDictionaryList)) << "\n";
}


// ============================================================================
// @0x8225CA08 -- BrnDirector::DirectorResourceManager::Prepare.
//
// The manager's staged resource bring-up, and the ONLY thing that ever fills the 65-slot
// shot-group bank. Landed 2026-08-01; it was a `DirectorLinkStubs.cpp` `return true` until
// then, which is why every slot sat as a null-collection default construction.
//
// SIGNATURE, from the one call site (DirectorModule::Prepare @0x82271374):
//     r3 = &mDirectorResourceManager, r4 = the director OUTPUT buffer, r5 = the ICE wrapper.
// Nothing else is read. (The "dropped OutputBuffer_Prepare* argument" warning in the older
// notes belongs to RaceCarEntityModule::Prepare, a different function.)
//
// THE STAGE MACHINE (switch on mePrepareStage, falling through stage to stage; every
// non-terminal exit returns false and the module framework re-enters next tick):
//   0 CONSTRUCTED       Clear the receiver queue; stash the ICE wrapper (the ONLY write of
//                       mpICEWrapper anywhere).                              -> falls into 1
//   1 REQUEST_RESOURCES Clear AGAIN (the second Clear really is in the binary), then, all
//                       through the output buffer's GameData request interface:
//                         GetICEList(&mReceiverQueue, 2)
//                         GetVehicleList(&mReceiverQueue, 1)
//                         AcquireResource(&mReceiverQueue, 0, 5, "CameraVault")
//                                                                            -> falls into 2
//   2 ACQUIRE_RESOURCES store stage 2; wait for miCount >= 2; drain the replies:
//                         type  4 -> mAttribsysVaultResourceHandle = the response handle
//                                    (asserts the response's miEventId == 0, cpp:132)
//                         type 52 -> mpVehicleList       = reply.mHandle.mpResourceMemory
//                                    (asserts miEventId == 1, cpp:121)
//                         type 64 -> mpICEDictionaryList = reply.mHandle.mpResourceMemory
//                                    (asserts miEventId == 2, cpp:110)
//                         default -> assert "Invalid event id received" (cpp:139)
//                                                                            -> falls into 3
//   3 REGISTER_ATTRIBSYSVAULT   Clear, then RegisterVault(&mReceiverQueue, the handle,
//                       eventId 1, E_VAULT_TYPE_RESIDENT) on the output buffer's ATTRIBSYS
//                       vault request interface (+0x510).                    -> falls into 4
//                       (An ENTRY POINT only -- Prepare never STORES 3.)
//   4 ..._WAIT          store stage 4; wait for ANY event on the queue (its type and id are
//                       never checked); then build the 65 attrib slots + mAfterTouchCam with
//                       the 100 baked asserts.                               -> falls into 5
//   5 PREPARED          store stage 5, return true.
//   default             return true WITHOUT touching the stage.
//
// [!] THE STAGE-2 RELEASE GATE IS A CONSOLE RACE -- REPRODUCED, NOT FIXED. Three replies are
// outstanding (ICE list id 2, vehicle list id 1, CameraVault acquire id 0) and the gate is
// `miCount >= 2`, after which the queue is Clear()ed and any late reply is dropped. On this
// PC build that is not merely latent, it is LOAD-BEARING: GameDataModule's GetICEList
// handler is still the deferred one (BrnGameDataModule.cpp `DeferredGameDataRequest
// ("GetICEList (id 64)")`), so exactly TWO of the three ever arrive and the console's own
// early gate is what lets the machine advance. mpICEDictionaryList consequently stays null
// here -- the same value it had before this function existed. DO NOT "correct" the gate to
// 3 without landing BrnResource::ICEList first: that would deadlock the loading flow.
//
// [!] ONE CONSOLE COPY-PASTE BUG, REPRODUCED (confirmed from the asm): r30 is loaded with
// `this + 0x3C8` (== mRankUpGroup) for the cpp:325/:326 asserts and is NEVER reloaded before
// cpp:329/:330, even though the operator= between them targets `this + 0x5C8`
// (== mOnlineCarSelect) through a scratch register. So mOnlineCarSelect is constructed and
// then never validated, and cpp:329/:330 duplicate cpp:325/:326 verbatim.
//
// CONSTRUCTION ORDER IS NOT DECLARATION ORDER. It starts at mCameraDefaults (+1496), then
// mRoadRageStartGroup (+584), mRaceStartGroup (+568), ... and ends mRankUpGroup (+968),
// mOnlineCarSelect (+1480), mAfterTouchCam, mTestbed (+1576), mTestbed2 (+1592). The order
// below is the binary's, slot for slot.
//
// ASSERT COUNT CORRECTION: the header prose says "101 asserts". Counted mechanically off the
// pseudocode it is 100 in stage 4 plus 4 in the drain loop == 104.
// ============================================================================
bool DirectorResourceManager::Prepare(DirectorIO::OutputBuffer* lpOutputBuffer,
                                      ICEWrapper* lpHACKIceWrapper)
{
    switch (mePrepareStage)
    {
    case E_PREPARESTAGE_CONSTRUCTED:                        // 0
        mReceiverQueue.Clear();
        mpICEWrapper = lpHACKIceWrapper;                    // asm: stw r30, 0x230(r31)
        // fall through

    case E_PREPARESTAGE_REQUEST_RESOURCES:                  // 1
        mReceiverQueue.Clear();
        lpOutputBuffer->GetResour()->GetICEList(&mReceiverQueue, KI_EVENTID_ICE_LIST);
        lpOutputBuffer->GetResour()->GetVehicleList(&mReceiverQueue, KI_EVENTID_VEHICLE_LIST);
        // The console builds the 24-byte AcquireResourceRequest inline and AddEvent(.., 4, 24)s
        // it; that IS RequestInterface<512>::AcquireResource, store for store.
        lpOutputBuffer->GetResour()->AcquireResource(
            &mReceiverQueue, KI_EVENTID_CAMERA_VAULT, KI_CAMERA_VAULT_POOL_ID, "CameraVault");
        // fall through

    case E_PREPARESTAGE_ACQUIRE_RESOURCES:                  // 2
    {
        mePrepareStage = E_PREPARESTAGE_ACQUIRE_RESOURCES;
        if (mReceiverQueue.GetCount() < 2)                  // asm: lwz r11,8(r31); cmpwi 2; blt
            return false;

        const CgsModule::Event* lpEventData = 0;
        s32 liSize = 0;
        s32 liEventType = mReceiverQueue.GetFirstEvent(&lpEventData, &liSize);
        while (lpEventData != 0)
        {
            switch (liEventType)
            {
            case KI_EVENT_ACQUIRE_RESOURCE:                 // 4
            {
                const CgsResource::Events::AcquireResourceResponse* lpResponse =
                    reinterpret_cast<const CgsResource::Events::AcquireResourceResponse*>(lpEventData);
                CGS_ASSERT(lpResponse->miEventId == KI_EVENTID_CAMERA_VAULT,
                           "Invalid event id received\n");                              // cpp:132
                // asm: two lwz/stw from payload+0x18/+0x1C into this+0x218 -- the 8-byte handle.
                mAttribsysVaultResourceHandle.mpResourceMemory = lpResponse->mpResourceMemory;
                mAttribsysVaultResourceHandle.mpSourceEntry    = lpResponse->mpSourceEntry;
                break;
            }

            case KI_EVENT_GET_VEHICLE_LIST:                 // 52
            {
                const BrnResource::GameDataIO::GameDataAssetEvent* lpReply =
                    reinterpret_cast<const BrnResource::GameDataIO::GameDataAssetEvent*>(lpEventData);
                CGS_ASSERT(lpReply->miEventId == KI_EVENTID_VEHICLE_LIST,
                           "Invalid event id received\n");                              // cpp:121
                // asm: lwz r11,0x20(payload) -- the reply's mHandle.mpResourceMemory, which
                // ProcessGetVehicleListRequest sets to &GameDataModule::mVehicleList.
                mpVehicleList = reinterpret_cast<const BrnResource::VehicleList*>(
                    lpReply->mHandle.mpResourceMemory);
                break;
            }

            case KI_EVENT_GET_ICE_LIST:                     // 64
            {
                const BrnResource::GameDataIO::GameDataAssetEvent* lpReply =
                    reinterpret_cast<const BrnResource::GameDataIO::GameDataAssetEvent*>(lpEventData);
                CGS_ASSERT(lpReply->miEventId == KI_EVENTID_ICE_LIST,
                           "Invalid event id received\n");                              // cpp:110
                mpICEDictionaryList = reinterpret_cast<const BrnResource::ICEList*>(
                    lpReply->mHandle.mpResourceMemory);
                break;
            }

            default:
                CGS_ASSERT(false, "Invalid event id received\n");                       // cpp:139
                break;
            }

            liEventType = mReceiverQueue.GetNextEvent(lpEventData, &lpEventData, &liSize);
        }
    }
    // fall through

    case E_PREPARESTAGE_REGISTER_ATTRIBSYSVAULT:            // 3  (asm LABEL_22)
        mReceiverQueue.Clear();
        lpOutputBuffer->GetVaultRequestInterface()->RegisterVault(
            &mReceiverQueue, mAttribsysVaultResourceHandle,
            /*liEventId*/ 1, CgsAttribSys::AttribSysIO::E_VAULT_TYPE_RESIDENT);
        // fall through

    case E_PREPARESTAGE_REGISTER_ATTRIBSYSVAULT_WAIT:       // 4  (asm LABEL_23)
    {
        mePrepareStage = E_PREPARESTAGE_REGISTER_ATTRIBSYSVAULT_WAIT;
        if (mReceiverQueue.GetCount() == 0)                 // asm: lwz r10,8(r31); cmpwi 0; beq
            return false;

        mCameraDefaults = Attrib::Gen::cameradefaults(Attrib::StringToKey("430819"), 0);   // +1496
        mRoadRageStartGroup = Attrib::Gen::shotgroup(Attrib::StringToKey("474399"), 0);   // +584
        mRaceStartGroup = Attrib::Gen::shotgroup(Attrib::StringToKey("424409"), 0);   // +568
        mRaceStartRivalInFrontGroup = Attrib::Gen::shotgroup(Attrib::StringToKey("428119"), 0);   // +600
        mOnlineRaceStart = Attrib::Gen::shotgroup(Attrib::StringToKey("428118"), 0);   // +616
        mBurningRouteStartGroup = Attrib::Gen::shotgroup(Attrib::StringToKey("475199"), 0);   // +632
        mSurvivorStartGroup = Attrib::Gen::shotgroup(Attrib::StringToKey("474394"), 0);   // +648
        mEliminatorStartGroup = Attrib::Gen::shotgroup(Attrib::StringToKey("474388"), 0);   // +664
        mTrafficAttackStartGroup = Attrib::Gen::shotgroup(Attrib::StringToKey("475200"), 0);   // +680
        mOnlineLobbyStartGroup = Attrib::Gen::shotgroup(Attrib::StringToKey("480584"), 0);   // +696
        mPursuitStartGroup = Attrib::Gen::shotgroup(Attrib::StringToKey("474389"), 0);   // +712
        mStuntRaceStartGroup = Attrib::Gen::shotgroup(Attrib::StringToKey("559418"), 0);   // +728
        CGS_ASSERT(mStuntRaceStartGroup.IsValid(), "mStuntRaceStartGroup.IsValid()");   // cpp:181
        CGS_ASSERT(mStuntRaceStartGroup.Num_ShotList() > 0, "mStuntRaceStartGroup.Num_ShotList() > 0");   // cpp:182
        mMarkedManStartGroup = Attrib::Gen::shotgroup(Attrib::StringToKey("559367"), 0);   // +744
        CGS_ASSERT(mMarkedManStartGroup.IsValid(), "mMarkedManStartGroup.IsValid()");   // cpp:185
        CGS_ASSERT(mMarkedManStartGroup.Num_ShotList() > 0, "mMarkedManStartGroup.Num_ShotList() > 0");   // cpp:186
        mBurningRouteFinishGroup = Attrib::Gen::shotgroup(Attrib::StringToKey("480563"), 0);   // +760
        mMarkedManFinishGroup = Attrib::Gen::shotgroup(Attrib::StringToKey("480562"), 0);   // +776
        mRaceFinishGroup = Attrib::Gen::shotgroup(Attrib::StringToKey("476830"), 0);   // +792
        mRoadRageFinishGroup = Attrib::Gen::shotgroup(Attrib::StringToKey("561160"), 0);   // +808
        mStuntFinishGroup = Attrib::Gen::shotgroup(Attrib::StringToKey("480560"), 0);   // +824
        CGS_ASSERT(mBurningRouteFinishGroup.IsValid(), "mBurningRouteFinishGroup.IsValid()");   // cpp:194
        CGS_ASSERT(mMarkedManFinishGroup.IsValid(), "mMarkedManFinishGroup.IsValid()");   // cpp:195
        CGS_ASSERT(mRaceFinishGroup.IsValid(), "mRaceFinishGroup.IsValid()");   // cpp:196
        CGS_ASSERT(mRoadRageFinishGroup.IsValid(), "mRoadRageFinishGroup.IsValid()");   // cpp:197
        CGS_ASSERT(mStuntFinishGroup.IsValid(), "mStuntFinishGroup.IsValid()");   // cpp:198
        CGS_ASSERT(mBurningRouteFinishGroup.Num_ShotList() > 0, "mBurningRouteFinishGroup.Num_ShotList() > 0");   // cpp:200
        CGS_ASSERT(mMarkedManFinishGroup.Num_ShotList() > 0, "mMarkedManFinishGroup.Num_ShotList() > 0");   // cpp:201
        CGS_ASSERT(mRaceFinishGroup.Num_ShotList() > 0, "mRaceFinishGroup.Num_ShotList() > 0");   // cpp:202
        CGS_ASSERT(mRoadRageFinishGroup.Num_ShotList() > 0, "mRoadRageFinishGroup.Num_ShotList() > 0");   // cpp:203
        CGS_ASSERT(mStuntFinishGroup.Num_ShotList() > 0, "mStuntFinishGroup.Num_ShotList() > 0");   // cpp:204
        mRaceFinishNorth = Attrib::Gen::shotgroup(Attrib::StringToKey("561973"), 0);   // +840
        mRaceFinishNorthEast = Attrib::Gen::shotgroup(Attrib::StringToKey("561960"), 0);   // +856
        mRaceFinishEast = Attrib::Gen::shotgroup(Attrib::StringToKey("561961"), 0);   // +872
        mRaceFinishSouthEast = Attrib::Gen::shotgroup(Attrib::StringToKey("561962"), 0);   // +888
        mRaceFinishSouth = Attrib::Gen::shotgroup(Attrib::StringToKey("561963"), 0);   // +904
        mRaceFinishSouthWest = Attrib::Gen::shotgroup(Attrib::StringToKey("561964"), 0);   // +920
        mRaceFinishWest = Attrib::Gen::shotgroup(Attrib::StringToKey("561965"), 0);   // +936
        mRaceFinishNorthWest = Attrib::Gen::shotgroup(Attrib::StringToKey("561972"), 0);   // +952
        CGS_ASSERT(mRaceFinishNorth.IsValid(), "mRaceFinishNorth.IsValid()");   // cpp:215
        CGS_ASSERT(mRaceFinishNorthEast.IsValid(), "mRaceFinishNorthEast.IsValid()");   // cpp:216
        CGS_ASSERT(mRaceFinishEast.IsValid(), "mRaceFinishEast.IsValid()");   // cpp:217
        CGS_ASSERT(mRaceFinishSouthEast.IsValid(), "mRaceFinishSouthEast.IsValid()");   // cpp:218
        CGS_ASSERT(mRaceFinishSouth.IsValid(), "mRaceFinishSouth.IsValid()");   // cpp:219
        CGS_ASSERT(mRaceFinishSouthWest.IsValid(), "mRaceFinishSouthWest.IsValid()");   // cpp:220
        CGS_ASSERT(mRaceFinishWest.IsValid(), "mRaceFinishWest.IsValid()");   // cpp:221
        CGS_ASSERT(mRaceFinishNorthWest.IsValid(), "mRaceFinishNorthWest.IsValid()");   // cpp:222
        mDriveThruGasStationGroup = Attrib::Gen::shotgroup(Attrib::StringToKey("428141"), 0);   // +984
        mDriveThruBodyShopGroup = Attrib::Gen::shotgroup(Attrib::StringToKey("428144"), 0);   // +1000
        mDriveThruTyreShopGroup = Attrib::Gen::shotgroup(Attrib::StringToKey("428142"), 0);   // +1016
        mDriveThruAutoPartsGroup = Attrib::Gen::shotgroup(Attrib::StringToKey("428143"), 0);   // +1032
        mDriveThruTuningShopGroup = Attrib::Gen::shotgroup(Attrib::StringToKey("428140"), 0);   // +1048
        mShakeAnimsGroup = Attrib::Gen::shotgroup(Attrib::StringToKey("428114"), 0);   // +1304
        mCarSelectMotorCity = Attrib::Gen::shotgroup(Attrib::StringToKey("432577"), 0);   // +1064
        CGS_ASSERT(mCarSelectMotorCity.IsValid(), "mCarSelectMotorCity.IsValid()");   // cpp:233
        CGS_ASSERT(mCarSelectMotorCity.Num_ShotList() >= 4, "mCarSelectMotorCity.Num_ShotList() >= 4");   // cpp:234
        mCarSelectMotorCityRivalUnlock = Attrib::Gen::shotgroup(Attrib::StringToKey("558663"), 0);   // +1080
        CGS_ASSERT(mCarSelectMotorCityRivalUnlock.IsValid(), "mCarSelectMotorCityRivalUnlock.IsValid()");   // cpp:237
        CGS_ASSERT(mCarSelectMotorCityRivalUnlock.Num_ShotList() >= 2, "mCarSelectMotorCityRivalUnlock.Num_ShotList() >= 2");   // cpp:238
        mCarSelectWestAcres = Attrib::Gen::shotgroup(Attrib::StringToKey("450907"), 0);   // +1096
        CGS_ASSERT(mCarSelectWestAcres.IsValid(), "mCarSelectWestAcres.IsValid()");   // cpp:241
        CGS_ASSERT(mCarSelectWestAcres.Num_ShotList() >= 4, "mCarSelectWestAcres.Num_ShotList() >= 4");   // cpp:242
        mCarSelectWestAcresRivalUnlock = Attrib::Gen::shotgroup(Attrib::StringToKey("558657"), 0);   // +1112
        CGS_ASSERT(mCarSelectWestAcresRivalUnlock.IsValid(), "mCarSelectWestAcresRivalUnlock.IsValid()");   // cpp:245
        CGS_ASSERT(mCarSelectWestAcresRivalUnlock.Num_ShotList() >= 2, "mCarSelectWestAcresRivalUnlock.Num_ShotList() >= 2");   // cpp:246
        mCarSelectSouthBay = Attrib::Gen::shotgroup(Attrib::StringToKey("450916"), 0);   // +1128
        CGS_ASSERT(mCarSelectSouthBay.IsValid(), "mCarSelectSouthBay.IsValid()");   // cpp:249
        CGS_ASSERT(mCarSelectSouthBay.Num_ShotList() >= 4, "mCarSelectSouthBay.Num_ShotList() >= 4");   // cpp:250
        mCarSelectSouthBayRivalUnlock = Attrib::Gen::shotgroup(Attrib::StringToKey("558658"), 0);   // +1144
        CGS_ASSERT(mCarSelectSouthBayRivalUnlock.IsValid(), "mCarSelectSouthBayRivalUnlock.IsValid()");   // cpp:253
        CGS_ASSERT(mCarSelectSouthBayRivalUnlock.Num_ShotList() >= 2, "mCarSelectSouthBayRivalUnlock.Num_ShotList() >= 2");   // cpp:254
        mCarSelectHeartbreak = Attrib::Gen::shotgroup(Attrib::StringToKey("451080"), 0);   // +1160
        CGS_ASSERT(mCarSelectHeartbreak.IsValid(), "mCarSelectHeartbreak.IsValid()");   // cpp:257
        CGS_ASSERT(mCarSelectHeartbreak.Num_ShotList() >= 4, "mCarSelectHeartbreak.Num_ShotList() >= 4");   // cpp:258
        mCarSelectHeartbreakRivalUnlock = Attrib::Gen::shotgroup(Attrib::StringToKey("558660"), 0);   // +1176
        CGS_ASSERT(mCarSelectHeartbreakRivalUnlock.IsValid(), "mCarSelectHeartbreakRivalUnlock.IsValid()");   // cpp:261
        CGS_ASSERT(mCarSelectHeartbreakRivalUnlock.Num_ShotList() >= 2, "mCarSelectHeartbreakRivalUnlock.Num_ShotList() >= 2");   // cpp:262
        mCarSelectLowerPeaks = Attrib::Gen::shotgroup(Attrib::StringToKey("450948"), 0);   // +1192
        CGS_ASSERT(mCarSelectLowerPeaks.IsValid(), "mCarSelectLowerPeaks.IsValid()");   // cpp:265
        CGS_ASSERT(mCarSelectLowerPeaks.Num_ShotList() >= 4, "mCarSelectLowerPeaks.Num_ShotList() >= 4");   // cpp:266
        mCarSelectLowerPeaksRivalUnlock = Attrib::Gen::shotgroup(Attrib::StringToKey("558659"), 0);   // +1208
        CGS_ASSERT(mCarSelectLowerPeaksRivalUnlock.IsValid(), "mCarSelectLowerPeaksRivalUnlock.IsValid()");   // cpp:269
        CGS_ASSERT(mCarSelectLowerPeaksRivalUnlock.Num_ShotList() >= 2, "mCarSelectLowerPeaksRivalUnlock.Num_ShotList() >= 2");   // cpp:270
        mCarSelectIdle = Attrib::Gen::shotgroup(Attrib::StringToKey("611284"), 0);   // +1224
        CGS_ASSERT(mCarSelectIdle.IsValid(), "mCarSelectIdle.IsValid()");   // cpp:273
        CGS_ASSERT(mCarSelectIdle.Num_ShotList() > 0, "mCarSelectIdle.Num_ShotList() >= 1");   // cpp:274
        mCarSelectOutro = Attrib::Gen::shotgroup(Attrib::StringToKey("611285"), 0);   // +1240
        CGS_ASSERT(mCarSelectOutro.IsValid(), "mCarSelectOutro.IsValid()");   // cpp:277
        CGS_ASSERT(mCarSelectOutro.Num_ShotList() > 0, "mCarSelectOutro.Num_ShotList() >= 1");   // cpp:278
        mCarUnlock = Attrib::Gen::shotgroup(Attrib::StringToKey("553098"), 0);   // +1256
        CGS_ASSERT(mCarUnlock.IsValid(), "mCarUnlock.IsValid()");   // cpp:281
        CGS_ASSERT(mCarUnlock.Num_ShotList() > 0, "mCarUnlock.Num_ShotList() > 0");   // cpp:282
        mGameIntroGroup = Attrib::Gen::shotgroup(Attrib::StringToKey("606002"), 0);   // +1272
        CGS_ASSERT(mGameIntroGroup.IsValid(), "mGameIntroGroup.IsValid()");   // cpp:285
        CGS_ASSERT(mGameIntroGroup.Num_ShotList() > 0, "mGameIntroGroup.Num_ShotList() > 0");   // cpp:286
        mBurnoutLicense = Attrib::Gen::shotgroup(Attrib::StringToKey("605835"), 0);   // +1288
        CGS_ASSERT(mBurnoutLicense.IsValid(), "mBurnoutLicense.IsValid()");   // cpp:289
        CGS_ASSERT(mBurnoutLicense.Num_ShotList() > 0, "mBurnoutLicense.Num_ShotList() > 0");   // cpp:290
        mJumpRig = Attrib::Gen::shotgroup(Attrib::StringToKey("440805"), 0);   // +1320
        mHardStopWorldLeft = Attrib::Gen::shotgroup(Attrib::StringToKey("461063"), 0);   // +1336
        mHardStopWorldRight = Attrib::Gen::shotgroup(Attrib::StringToKey("461057"), 0);   // +1352
        mHardStopCarLeft = Attrib::Gen::shotgroup(Attrib::StringToKey("466945"), 0);   // +1368
        mHardStopCarRight = Attrib::Gen::shotgroup(Attrib::StringToKey("466946"), 0);   // +1384
        mAfterCrash = Attrib::Gen::shotgroup(Attrib::StringToKey("461719"), 0);   // +1448
        mAfterCrashSafe = Attrib::Gen::shotgroup(Attrib::StringToKey("466949"), 0);   // +1464
        mFailsafe = Attrib::Gen::shotgroup(Attrib::StringToKey("467917"), 0);   // +1512
        mTakedown = Attrib::Gen::shotgroup(Attrib::StringToKey("475241"), 0);   // +1528
        mCrashbreaker = Attrib::Gen::shotgroup(Attrib::StringToKey("478055"), 0);   // +1544
        mTakendown = Attrib::Gen::shotgroup(Attrib::StringToKey("575796"), 0);   // +1560
        CGS_ASSERT(mTakendown.IsValid(), "mTakendown.IsValid()");   // cpp:309
        CGS_ASSERT(mTakendown.Num_ShotList() > 0, "mTakendown.Num_ShotList() > 0");   // cpp:310
        mFastCrashShotGroup = Attrib::Gen::shotgroup(Attrib::StringToKey("494628"), 0);   // +1400
        CGS_ASSERT(mFastCrashShotGroup.IsValid(), "mFastCrashShotGroup.IsValid()");   // cpp:313
        CGS_ASSERT(mFastCrashShotGroup.Num_ShotList() > 0, "mFastCrashShotGroup.Num_ShotList() > 0");   // cpp:314
        mNormalCrashShotGroup = Attrib::Gen::shotgroup(Attrib::StringToKey("543590"), 0);   // +1416
        CGS_ASSERT(mNormalCrashShotGroup.IsValid(), "mNormalCrashShotGroup.IsValid()");   // cpp:317
        CGS_ASSERT(mNormalCrashShotGroup.Num_ShotList() > 0, "mNormalCrashShotGroup.Num_ShotList() > 0");   // cpp:318
        mSlowCrashShotGroup = Attrib::Gen::shotgroup(Attrib::StringToKey("542963"), 0);   // +1432
        CGS_ASSERT(mSlowCrashShotGroup.IsValid(), "mSlowCrashShotGroup.IsValid()");   // cpp:321
        CGS_ASSERT(mSlowCrashShotGroup.Num_ShotList() > 0, "mSlowCrashShotGroup.Num_ShotList() > 0");   // cpp:322
        mRankUpGroup = Attrib::Gen::shotgroup(Attrib::StringToKey("544056"), 0);   // +968
        CGS_ASSERT(mRankUpGroup.IsValid(), "mRankUpGroup.IsValid()");   // cpp:325
        CGS_ASSERT(mRankUpGroup.Num_ShotList() > 0, "mRankUpGroup.Num_ShotList() > 0");   // cpp:326
        mOnlineCarSelect = Attrib::Gen::shotgroup(Attrib::StringToKey("613970"), 0);   // +1480
        CGS_ASSERT(mRankUpGroup.IsValid(), "mRankUpGroup.IsValid()");   // cpp:329
        CGS_ASSERT(mRankUpGroup.Num_ShotList() > 0, "mRankUpGroup.Num_ShotList() > 0");   // cpp:330

        // ---- mAfterTouchCam (+1608) -------------------------------------------------------
        // @0x8225E3FC. NOT a generated-class slot: the console builds a BARE Attrib::RefSpec on
        // the stack from {aftertouchcam class key, StringToKey("428410")} with a null resolved
        // collection, assigns it through RefSpec::operator= @0x8280DFB0 (which resolves + AddRefs
        // into the destination) and then Clean()s the temp IF the assignment left it holding a
        // resolved collection. Reproduced verbatim.
        // ⚠️ The pseudocode's `StringToKey("428410") | 0x632388D600000000` is the same Hex-Rays
        // fusion artifact as the CameraVault id: the `mr r11,r3` copies the key whole and the
        // 0x632388D6 half belongs to the SEPARATE class-key doubleword stored at temp+0.
        {
            Attrib::RefSpec lAfterTouchCam(
                Attrib::Gen::aftertouchcam::KU_AFTERTOUCHCAM_CLASS_KEY,
                Attrib::StringToKey("428410"));
            mAfterTouchCam = lAfterTouchCam;
            if (lAfterTouchCam.HasResolvedCollection())
                lAfterTouchCam.Clean();
        }

        mTestbed = Attrib::Gen::shotgroup(Attrib::StringToKey("535488"), 0);   // +1576
        mTestbed2 = Attrib::Gen::shotgroup(Attrib::StringToKey("568320"), 0);   // +1592
        CGS_ASSERT(mTestbed.IsValid(), "mTestbed.IsValid()");   // cpp:337
        CGS_ASSERT(mTestbed2.IsValid(), "mTestbed2.IsValid()");   // cpp:338
        CGS_ASSERT(mRoadRageStartGroup.IsValid(), "mRoadRageStartGroup.IsValid()");   // cpp:342
        CGS_ASSERT(mRaceStartGroup.IsValid(), "mRaceStartGroup.IsValid()");   // cpp:343
        CGS_ASSERT(mRaceStartRivalInFrontGroup.IsValid(), "mRaceStartRivalInFrontGroup.IsValid()");   // cpp:344
        CGS_ASSERT(mBurningRouteStartGroup.IsValid(), "mBurningRouteStartGroup.IsValid()");   // cpp:345
        CGS_ASSERT(mSurvivorStartGroup.IsValid(), "mSurvivorStartGroup.IsValid()");   // cpp:346
        CGS_ASSERT(mEliminatorStartGroup.IsValid(), "mEliminatorStartGroup.IsValid()");   // cpp:347
        CGS_ASSERT(mTrafficAttackStartGroup.IsValid(), "mTrafficAttackStartGroup.IsValid()");   // cpp:348
        CGS_ASSERT(mOnlineRaceStart.IsValid(), "mOnlineRaceStart.IsValid()");   // cpp:349
        CGS_ASSERT(mOnlineLobbyStartGroup.IsValid(), "mOnlineLobbyStartGroup.IsValid()");   // cpp:350
        CGS_ASSERT(mPursuitStartGroup.IsValid(), "mPursuitStartGroup.IsValid()");   // cpp:351
        CGS_ASSERT(mDriveThruGasStationGroup.IsValid(), "mDriveThruGasStationGroup.IsValid()");   // cpp:353
        CGS_ASSERT(mDriveThruBodyShopGroup.IsValid(), "mDriveThruBodyShopGroup.IsValid()");   // cpp:354
        CGS_ASSERT(mDriveThruTyreShopGroup.IsValid(), "mDriveThruTyreShopGroup.IsValid()");   // cpp:355
        CGS_ASSERT(mDriveThruAutoPartsGroup.IsValid(), "mDriveThruAutoPartsGroup.IsValid()");   // cpp:356
        CGS_ASSERT(mDriveThruTuningShopGroup.IsValid(), "mDriveThruTuningShopGroup.IsValid()");   // cpp:357
        CGS_ASSERT(mShakeAnimsGroup.IsValid(), "mShakeAnimsGroup.IsValid()");   // cpp:359
        CGS_ASSERT(mJumpRig.IsValid(), "mJumpRig.IsValid()");   // cpp:360
        CGS_ASSERT(mHardStopWorldLeft.IsValid(), "mHardStopWorldLeft.IsValid()");   // cpp:361
        CGS_ASSERT(mHardStopWorldRight.IsValid(), "mHardStopWorldRight.IsValid()");   // cpp:362
        CGS_ASSERT(mHardStopCarLeft.IsValid(), "mHardStopCarLeft.IsValid()");   // cpp:363
        CGS_ASSERT(mHardStopCarRight.IsValid(), "mHardStopCarRight.IsValid()");   // cpp:364
        CGS_ASSERT(mAfterCrash.IsValid(), "mAfterCrash.IsValid()");   // cpp:365
        CGS_ASSERT(mAfterCrashSafe.IsValid(), "mAfterCrashSafe.IsValid()");   // cpp:366
        CGS_ASSERT(mFailsafe.IsValid(), "mFailsafe.IsValid()");   // cpp:367
        CGS_ASSERT(mCrashbreaker.IsValid(), "mCrashbreaker.IsValid()");   // cpp:368
        CGS_ASSERT(mRaceStartGroup.Num_ShotList() > 0, "mRaceStartGroup.Num_ShotList() > 0");   // cpp:371  [v149 == &mRaceStartGroup]
        CGS_ASSERT(mRaceStartRivalInFrontGroup.Num_ShotList() > 0, "mRaceStartRivalInFrontGroup.Num_ShotList() > 0");   // cpp:372
        CGS_ASSERT(mOnlineRaceStart.Num_ShotList() > 0, "mOnlineRaceStart.Num_ShotList() > 0");   // cpp:373
        CGS_ASSERT(mJumpRig.Num_ShotList() > 0, "mJumpRig.Num_ShotList() > 0");   // cpp:374
        CGS_ASSERT(mAfterCrash.Num_ShotList() > 0, "mAfterCrash.Num_ShotList() > 0");   // cpp:375
        CGS_ASSERT(mAfterCrashSafe.Num_ShotList() > 0, "mAfterCrashSafe.Num_ShotList() > 0");   // cpp:376
        CGS_ASSERT(mFailsafe.Num_ShotList() > 0, "mFailsafe.Num_ShotList() > 0");   // cpp:377
        CGS_ASSERT(mCrashbreaker.Num_ShotList() > 0, "mCrashbreaker.Num_ShotList() > 0");   // cpp:378
        CGS_ASSERT(mPursuitStartGroup.Num_ShotList() > 0, "mPursuitStartGroup.Num_ShotList() > 0");   // cpp:379
        LogShotGroupBankState();
    }
    // fall through

    case E_PREPARESTAGE_PREPARED:                           // 5  (asm LABEL_226)
        mePrepareStage = E_PREPARESTAGE_PREPARED;
        return true;                                        //    (asm LABEL_227)

    default:
        // The console default arm returns true WITHOUT touching the stage word.
        return true;
    }
}

}
