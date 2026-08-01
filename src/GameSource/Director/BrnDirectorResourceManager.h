#ifndef BRN_DIRECTOR_RESOURCE_MANAGER_H
#define BRN_DIRECTOR_RESOURCE_MANAGER_H

#include "SDKs/Packages/ICE/ICEData.hpp"
#include "types.hpp"
#include "GameShared/GameClasses/System/Resource/CgsResourceID.h"   // CgsResource::ID (GetICETakeData arg / MakeICEMovieId return)
#include "GameShared/GameClasses/System/Resource/CgsResourceHandle.h"      // CgsResource::ResourceHandle (mAttribsysVaultResourceHandle)
#include "GameShared/GameClasses/Module/CgsBaseEventReceiverQueue.h"       // CgsModule::EventReceiverQueue<512,16> (mReceiverQueue)
#include "GameSource/AttribSys/Generated/classes/shotgroup.h"              // Attrib::Gen::shotgroup      (64 of the 65 slots)
#include "GameSource/AttribSys/Generated/classes/cameradefaults.h"         // Attrib::Gen::cameradefaults (the 65th)
#include <cstdio>                                                   // snprintf (GetKeyAnim name formatting)

namespace BrnResource { class ICEList; struct VehicleList; }   // mpICEDictionaryList / mpVehicleList (by pointer)
namespace ICE { struct ICEGroup; }    // GetShakeTakes' return type (pointer only; no full home yet)
namespace ICE { struct ICEAuthor; }   // SDKs/Packages/ICE/ICEAuthor.hpp -- GetICEAuthor's return type.
                                      // ⚠️ IT IS A `struct` THERE. The ICE-anim fork this header
                                      // replaces spelled it `class`, which MSVC mangles differently
                                      // (V vs U) -- a silent LNK2019 waiting to happen.

namespace BrnDirector
{

// The director-side ICE owner GetKeyAnim / GetShakeTakes reach through. Forward-declared
// (the inlines below only call its methods through a pointer; a using-TU includes the
// full home GameSource/Director/BrnDirectorICEWrapper.h before instantiating them).
class ICEWrapper;
class DirectorResourceManager;

// The director output buffer the real Prepare takes (its request interfaces live there).
// Forward-declared: this header only passes the pointer through.
namespace DirectorIO { struct OutputBuffer; }

class ICEResourceMgr : public ICE::IResourceManager
{
public:
    void Construct(DirectorResourceManager* lpResourceManager);

    const ICE::ICETakeData* GetTakeData(CgsResource::ID lId) const override;
    const ICE::ICETakeData* GetTakeData(s32 liIndex) const override;

private:
    DirectorResourceManager* mpResourceManager;
};

}

// FLAG: BrnResource::MakeICEMovieId hashes an ICE take name into a take resource id.
// Referenced by DirectorResourceManager::GetKeyAnim but with no reconstructed home yet
// -- declared here (declaration-only; the per-TU `cl /c` gate does not link). Replace
// with its real home when the ICE-resource-name TU is reconstructed.
namespace BrnResource
{
    CgsResource::ID MakeICEMovieId(const char* lpacName);
}

namespace BrnDirector
{

// ============================================================================
// ⭐ RECON MAP for the real DirectorResourceManager (recovered + cross-checked 2026-07-30).
// Nothing below is implemented yet -- this is the ground truth the rebuild needs, recorded
// here so the next wave is mechanical rather than another recovery pass.
//
// SHAPE. DirectorResourceManager::DirectorResourceManager @0x827DEB98 default-constructs a run
// of exactly 65 contiguous 16-byte sub-objects spanning byte 568..1592, then clears three
// trailing fields (1608 / 1616 / 1624). Those 65 slots ARE the shot-group bank: 64
// Attrib::Gen::shotgroup + one Attrib::Gen::cameradefaults, in the DecFIGS DWARF's declaration
// order (which is the memory order). The DWARF head members land ahead of them:
//     +0x000  EventReceiverQueue<512,16>   mReceiverQueue                (spans .. +535)
//     +536    ResourceHandle               mAttribsysVaultResourceHandle (8 bytes; a1[134]/[135])
//     +544    const BrnResource::ICEList*  mpICEDictionaryList           (a1[136])
//     +548    EPrepareStage                mePrepareStage                (a1[137])
//     +552    ICEResourceMgr               mICEResourceMgr               (vptr + owner ptr)
//     +560    ICEWrapper*                  mpICEWrapper                  (a1[140])
//     +564    const VehicleList*           mpVehicleList                 (a1[141])
//     +1608   Attrib::RefSpec              mAfterTouchCam                (key "428410", refspec
//                                                                         hi word 0x75E62FC1632388D6)
// EPrepareStage (DWARF BrnDirectorResourceManager.h:245):
//     0 CONSTRUCTED  1 REQUEST_RESOURCES  2 ACQUIRE_RESOURCES
//     3 REGISTER_ATTRIBSYSVAULT  4 REGISTER_ATTRIBSYSVAULT_WAIT  5 PREPARED
//
// Prepare @0x8225CA08 is a switch on mePrepareStage that falls through stage to stage:
//   0/1: Clear the receiver queue; stash the ICE wrapper (case 0 is the ONLY place +560 is
//        written); Clear again (the second Clear really is in the binary); then through the
//        output buffer's resource request interface issue
//        GameDataIO::RequestInterface<512>::GetICEList(&mReceiverQueue, 2) and
//        ::GetVehicleList(&mReceiverQueue, 1), then
//        RequestInterface<512>::AcquireResource(&mReceiverQueue, 0, 5, "CameraVault")
//        -- already reconstructed at BrnGameDataRequestQueueImpl.h:303; it is what the raw
//        `AddEvent(&record, 4, 24)` in the pseudocode expands to.                  -> stage 2
//        (CORRECTED 2026-07-31: the `HashString("CameraVault") | 0x5_00000000` quoted below
//        is a Hex-Rays artifact -- the `li r10, 5` is the SEPARATE miPoolId store at record
//        +0x08 and the resource id is a plain zero-extended 32-bit CRC. Same artifact on the
//        RefSpec key: there is no OR, the `mr r11, r3` just copies the whole register.)
//   2:   pump the receiver queue (returns 0 = "not prepared yet" while it holds < 2 events):
//          event 4  -> mAttribsysVaultResourceHandle = payload words 6/7
//          event 52 -> mpVehicleList        = payload word 8   (asserts payload[0] == 1)
//          event 64 -> mpICEDictionaryList  = payload word 8   (asserts payload[0] == 2)
//          default  -> assert "Invalid event id received"      (cpp:110/:121/:132/:139)
//        then Clear, and AttribSysRequestInterface<512>::RegisterVault(this,
//        mAttribsysVaultResourceHandle, 1, 0).                                   -> stage 4
//   4:   once the queue reports ANY event (its type and id are never checked), construct the
//        65 attrib instances (below) plus mAfterTouchCam, each as
//        `mX = Attrib::Gen::shotgroup(Attrib::StringToKey("<key>"), 0)` -- one 16-byte stack
//        temp per slot, assigned through Attrib::Instance::operator= @0x8280DE08 and then
//        destructed -- with the 101 IsValid()/Num_ShotList() asserts the console bakes.
//                                                                                 -> stage 5
//   5:   return true.   (default: return true without touching the stage.)
//   Stage 3 is an ENTRY POINT only -- Prepare never stores 3; it only ever stores 2, 4 and 5.
//
// IT IS 65 INSTANCES, NOT 66 (64 shotgroup + 1 cameradefaults), plus one RefSpec assignment.
//    The construction ORDER is not the declaration order: it starts with mCameraDefaults
//    (1496), then mRoadRageStartGroup (584), mRaceStartGroup (568), ... and ends
//    mRankUpGroup (968), mOnlineCarSelect (1480), mTestbed (1576), mTestbed2 (1592), then
//    mAfterTouchCam. (Full ordered list recovered 2026-07-31; the offsets and keys in the
//    table below are correct for all 65.)
//
// Construct() IS NOT A NO-OP. The `inline void Construct() {}` below is an empty stub, and
//    Prepare switches on mePrepareStage, which nothing else initialises. The real Construct
//    (DWARF BrnDirectorResourceManager.cpp:37) is fully inlined into
//    DirectorModule::Construct @0x8225C590 and does:
//        mReceiverQueue.miCapacity = 512; mReceiverQueue.mpBuffer = this + 24;
//        mReceiverQueue.miAlignment = 16; BaseEventReceiverQueue::Clear(this);
//        mePrepareStage = 0; mICEResourceMgr.mpResourceManager = this;
//        mpVehicleList = 0; mpICEDictionaryList = 0;
//    (+536 mAttribsysVaultResourceHandle and +560 mpICEWrapper are left uninitialised.)
//
// STAGE 2 RELEASES ON `miCount >= 2` WHILE THREE REPLIES ARE OUTSTANDING (ICE list id 2,
//    vehicle list id 1, CameraVault acquire id 0). If the third has not landed the queue is
//    Clear()ed, the late reply is dropped, mAttribsysVaultResourceHandle stays uninitialised
//    and RegisterVault runs on garbage. Console loader latency made that benign; on PC it
//    will not be. Carry it forward as-is and expect to have to gate around it.
//
// THE 65-SLOT TABLE. Byte offset, DWARF member, and the vault name key Prepare constructs it
// over. Every one of the 26 slots marked (A) is independently confirmed by its own
// IsValid()/Num_ShotList() assert string in Prepare; the rest follow from DWARF declaration
// order, which those 26 pin exactly (no gaps, no reordering).
//    568 mRaceStartGroup 424409 (A)            584 mRoadRageStartGroup 474399 (A)
//    600 mRaceStartRivalInFrontGroup 428119(A) 616 mOnlineRaceStart 428118 (A)
//    632 mBurningRouteStartGroup 475199        648 mSurvivorStartGroup 474394
//    664 mEliminatorStartGroup 474388          680 mTrafficAttackStartGroup 475200
//    696 mOnlineLobbyStartGroup 480584         712 mPursuitStartGroup 474389 (A)
//    728 mStuntRaceStartGroup 559418 (A)       744 mMarkedManStartGroup 559367 (A)
//    760 mBurningRouteFinishGroup 480563 (A)   776 mMarkedManFinishGroup 480562 (A)
//    792 mRaceFinishGroup 476830 (A)           808 mRoadRageFinishGroup 561160 (A)
//    824 mStuntFinishGroup 480560 (A)          840 mRaceFinishNorth 561973 (A)
//    856 mRaceFinishNorthEast 561960 (A)       872 mRaceFinishEast 561961 (A)
//    888 mRaceFinishSouthEast 561962 (A)       904 mRaceFinishSouth 561963 (A)
//    920 mRaceFinishSouthWest 561964 (A)       936 mRaceFinishWest 561965 (A)
//    952 mRaceFinishNorthWest 561972 (A)       968 mRankUpGroup 544056 (A)
//    984 mDriveThruGasStationGroup 428141     1000 mDriveThruBodyShopGroup 428144
//   1016 mDriveThruTyreShopGroup 428142       1032 mDriveThruAutoPartsGroup 428143
//   1048 mDriveThruTuningShopGroup 428140     1064 mCarSelectMotorCity 432577 (A)
//   1080 mCarSelectMotorCityRivalUnlock 558663 (A)
//   1096 mCarSelectWestAcres 450907 (A)       1112 mCarSelectWestAcresRivalUnlock 558657 (A)
//   1128 mCarSelectSouthBay 450916 (A)        1144 mCarSelectSouthBayRivalUnlock 558658 (A)
//   1160 mCarSelectHeartbreak 451080 (A)      1176 mCarSelectHeartbreakRivalUnlock 558660 (A)
//   1192 mCarSelectLowerPeaks 450948 (A)      1208 mCarSelectLowerPeaksRivalUnlock 558659 (A)
//   1224 mCarSelectIdle 611284 (A)            1240 mCarSelectOutro 611285 (A)
//   1256 mCarUnlock 553098 (A)          ⭐   1272 mGameIntroGroup 606002 (A)
//   1288 mBurnoutLicense 605835 (A)           1304 mShakeAnimsGroup 428114
//   1320 mJumpRig 440805 (A)                  1336 mHardStopWorldLeft 461063
//   1352 mHardStopWorldRight 461057           1368 mHardStopCarLeft 466945
//   1384 mHardStopCarRight 466946             1400 mFastCrashShotGroup 494628 (A)
//   1416 mNormalCrashShotGroup 543590 (A)     1432 mSlowCrashShotGroup 542963 (A)
//   1448 mAfterCrash 461719 (A)               1464 mAfterCrashSafe 466949 (A)
//   1480 mOnlineCarSelect 613970              1496 mCameraDefaults 430819  <- cameradefaults,
//                                                    the ONLY non-shotgroup slot, and the ctor
//                                                    builds exactly this one with a different
//                                                    element ctor (sub_827DC8C8). Independent
//                                                    confirmation of the whole ordering.
//   1512 mFailsafe 467917 (A)                 1528 mTakedown 475241
//   1544 mCrashbreaker 478055 (A)             1560 mTakendown 575796 (A)
//   1576 mTestbed 535488 (A)                  1592 mTestbed2 568320 (A)
//
// ONE CONSOLE BUG, DO NOT "FIX" IT -- CONFIRMED 2026-07-31 from the asm: r30 is loaded with
//    `this + 0x3C8` (== 968, mRankUpGroup) for the :325/:326 asserts and is NEVER reloaded
//    before :329/:330, even though the intervening operator= targets `this + 0x5C8` (== 1480,
//    mOnlineCarSelect) through a scratch r3. The two message-string registers are reused
//    unchanged too. So mOnlineCarSelect is constructed at cpp:328 and then never validated,
//    and cpp:329/:330 duplicate cpp:325/:326 verbatim. Reproduce it.
//
// THE 101 ASSERTS, in emission order. File string
//    "..DIR..DIR..DIRGameSourceDIRDirector/BrnDirectorResourceManager.cpp" (the console mixes
//    escaped backslashes and forward slashes; DIR here stands for the escaped backslash pair):
//    :181/:182 mStuntRaceStartGroup IsValid + Num_ShotList>0 - :185/:186 mMarkedManStartGroup -
//    :194-:198 mBurningRouteFinishGroup / mMarkedManFinishGroup / mRaceFinishGroup /
//    mRoadRageFinishGroup / mStuntFinishGroup IsValid - :200-:204 the same five Num_ShotList>0 -
//    :215-:222 the eight mRaceFinish{North,NorthEast,East,SouthEast,South,SouthWest,West,
//    NorthWest} IsValid - :233-:282 the car-select bank in pairs, with PER-GROUP MINIMUM shot
//    counts: the five district groups >= 4, their five RivalUnlock siblings >= 2,
//    mCarSelectIdle and mCarSelectOutro >= 1, mCarUnlock > 0 - :285/:286 mGameIntroGroup (the
//    retail intro) - :289/:290 mBurnoutLicense - :309/:310 mTakendown - :313-:322 the three
//    crash banks - :325/:326 and :329/:330 mRankUpGroup (twice: the bug) - :337/:338
//    mTestbed / mTestbed2 IsValid - :342-:368 twenty-five more IsValid - :371-:379 nine
//    Num_ShotList>0. Plus four "Invalid event id received" asserts in the drain loop at :110
//    (event 64), :121 (event 52), :132 (event 4) and :139 (default).
//    Never asserted at all: mTakedown (+1528), mCameraDefaults (+1496), mAfterTouchCam (+1608).
//
// THE CTOR @0x827DEB98, VERIFIED 2026-07-31: 0x238 -> 0x638 at stride 0x10 is exactly 65
//    calls, 64 x sub_827DC838 and ONE sub_827DC8C8 at 0x5D8 (== byte 1496 == mCameraDefaults).
//    sub_827DC838 is the shotgroup element ctor (Attrib::Instance::Instance then an
//    AssertOnClassCheck against 0x38ED2D37_3887CBC7 == KI_SHOTGROUP_CLASS), called with
//    (nullptr, nullptr). sub_827DC8C8 was INFERRED to be the cameradefaults sibling and is
//    now VERIFIED to be exactly that (2026-07-31, read from the IDA database: the same
//    shape, AssertOnClassCheck against 0x095B375E_5F206F31, plus a DefaultDataArea(0x38)
//    the shotgroup ctor does not have). The ctor then clears bytes 1608..1627 as
//    std/std/stw (8+8+4 == the whole 20-byte Attrib::RefSpec mAfterTouchCam) -- the committed
//    BrnDirectorResourceManager.cpp writes only 3 x 4 bytes there and leaves 1612..1615 and
//    1620..1623 uninitialised.
//
// ============================================================================
// ⭐ STATUS UPDATE 2026-07-31 (attrib-chain wave, b5-decomp bbf43771 + follow-up).
// Blockers 1, 2 and 3 below are now CLEARED. Blocker 4 (the 65 members) stands, the fork
// retirement stands, and ONE NEW BLOCKER was found that is not in the list below.
//
//  * BLOCKER 2 IS WRONG AS WRITTEN. `Attrib::Instance::operator=` @0x8280DE08 does NOT
//    "exist nowhere" -- it is missing from .ida-exports/BURNOUT_X360_ARTIST.XEX/ but is
//    present and named in the IDA database, in the gap between Database::GetExportPolicies
//    (ends 0x8280DE04) and the next export at 0x8280DE58. Twenty instructions:
//        Change(rhs.mpCollection); mpOwner = rhs.mpOwner; muFlags = rhs.muFlags; return this
//    Landed. ⚠️ The flag word is copied AFTER Change, overwriting the bit0 Change just
//    recomputed -- reproduce that. GENERAL LESSON: the JSON export set has holes, and two
//    of them were load-bearing this wave (Node::GetPointer @0x828045B0 was the other).
//    Before declaring a symbol unrecoverable, check the IDA database.
//
//  * ⭐ NEW BLOCKER, NOW FIXED: `Attrib::StringToKey` WAS TRUNCATING TO 32 BITS. Every one
//    of the 65 slots is built as `shotgroup(Attrib::StringToKey("606002"), 0)` and that
//    result passes through the generated ctor's r4 straight into FindCollection's
//    COLLECTION KEY, which the class table hashes as a whole doubleword. AttributeKey.h
//    declared StringToKey returning the 32-bit ::Attribute::Key and attribhash64.cpp
//    implemented it with an explicit `static_cast`; the X360 body @0x82805828 has NO
//    truncation (it tail-calls the 64-bit lookup8 hash and returns r3 whole, and the PS3
//    DWARF spells it `return Attrib::StringHash64(str)`). shotgroup.h and codegen.cpp each
//    ALSO carried a local `u32 StringToKey(const char*)` re-declaration, which links fine
//    on MSVC (return types are not mangled) and then reads only EAX. All corrected; both
//    generated ctors now take a u64 key. Without this every one of the 65 lookups missed.
//
//  * VERIFIED, replacing an INFERENCE below: `sub_827DC8C8` IS the cameradefaults element
//    ctor. Its body (read from the IDA database) is Instance::Instance + an
//    AssertOnClassCheck against 0x095B375E_5F206F31 == KI_CAMERADEFAULTS_CLASS, then
//    DefaultDataArea(0x38) if the instance has no data area. Its sibling sub_827DC838 is
//    the shotgroup element ctor (same shape, class key 0x38ED2D37_3887CBC7, and NO default
//    data area). Both take (this, collection, owner) -- these are the (Collection*, owner)
//    ctors, not the (key, owner) ones the Prepare stage-4 loop uses.
//
//  * STAGE 4 CONFIRMED INSTRUCTION-BY-INSTRUCTION at 0x8225CD08..0x8225CD70. The pattern is
//        bl StringToKey("430819") ; mr r4,r3 ; addi r3,sp,temp ; li r5,0
//        bl cameradefaults::cameradefaults
//        mr r4,r3 ; addi r3,r31,0x5D8 ; bl Instance::operator=
//        addi r3,sp,temp ; bl Instance::~Instance
//    then "474399" -> +0x248, then "424409" -> ... . That is mCameraDefaults at 1496,
//    mRoadRageStartGroup at 584, mRaceStartGroup at 568 -- the table below and its
//    "construction order is NOT declaration order" note are both exactly right.
//    Independently, Instance::operator='s xref list is 65 call sites inside Prepare.
// ============================================================================
// ⭐⭐ STATUS 2026-08-01: **Prepare IS LANDED**. Body in BrnDirectorResourceManager.cpp;
// the DirectorLinkStubs `return true` is deleted and the TU is in the exe source list.
// Boot-verified: 0 asserts, all 65 slots BIND their collection (64/64 shotgroups +
// mCameraDefaults, mGameIntroGroup("606002") included), the CameraVault registers as
// AttribSys slot 0, and the loading flow still reaches the fly-by in ~52 s.
//
// THREE THINGS HAD TO LAND WITH IT, none of them in this class:
//   * GameDataModule::PrepareICEList @0x8266CEB0 (Prepare stage 11) -- the ONLY loader of
//     "Cameras.bundle" in the image, and therefore the only thing that makes the CameraVault
//     resident in pool 5 where Prepare's AcquireResource can find it. Its two string
//     literals came off the asm's own string comments and both check out by hash:
//     HashString("StandardICETakes") == 0x0DC0EE8F and HashString("CameraVault") ==
//     0x28FE4576 are exactly the bundle's two resources.
//   * The OutputBuffer +0x510 re-home (see the note further down, now ACTED ON) plus the
//     AttribSys half of LoadDirectorModule's append -- without which RegisterVault could
//     never reach the module.
//   * THREE latent x64 defects in the AttribSys SDK that only this path could expose, all
//     the same species (a CONSOLE byte offset used on a host-widened object):
//       - Attrib::FindCollection / RefSpec::GetClass read the class registry at
//         `DatabasePrivate + 8`; on x64 it is +16. Symptom: an unbounded storm of
//         "table invariant is broken".
//       - Attrib::HashMap::Release decremented `*(u16*)(this+8)`, which on x64 is
//         muCapacity, not muRefCount. Symptom: "Too many releases of object!" on the first
//         Instance destructor that ever ran over a real collection.
//       - Attrib::Node (attribute.h) and Attrib::HashMap::Node (attribhashmap.h) are ONE
//         console type that Collection::GetNode casts between, and they had DIFFERENT x64
//         layouts (u32 payload vs machine word), so every cursor read the flags byte out of
//         the payload pointer. Symptom: every generated Num_<array>() reported 0 elements.
//
// ⛔ ONE THING PREPARE CANNOT FIX, measured and left armed-but-gated: every ported vault's
// serialised attribute entries lost their type-index/flag bytes in the PC port (the
// transcoder widens the +0x08 value slot to a host pointer inside a 16-byte record), so
// every array attribute reports exactly ONE element. The 37 Num_ShotList() asserts are kept
// verbatim behind KB_PC_ATTRIB_ARRAY_LENGTHS_VALID in the .cpp; the 63 IsValid() asserts are
// live and all pass. Full evidence at that constant.
//
// WHAT BLOCKED A REAL Prepare (historical; the RECOVERY was complete as of 2026-07-31):
//    1. (CLEARED) `Attrib::Gen::shotgroup::shotgroup` (shotgroup.h:105) THROWS THE KEY AWAY. The real
//       Attrib::FindCollection @0x82808378 is FindCollection(u64 luClassKey /*r3*/,
//       u32 luCollectionKey /*r4*/) -- asm-verified -- and the shotgroup ctor sets only r3,
//       letting the caller's StringToKey result pass through in r4. This repo declares
//       FindCollection(int, void* = nullptr) and the ctor body has the key parameter
//       COMMENTED OUT, so all 65 constructions would resolve the same wrong collection. Same
//       defect in cameradefaults.h, which additionally declares a ONE-argument ctor where the
//       X360 symbol takes (this, key, owner).
//    2. (CLEARED -- and the claim was false; see the STATUS UPDATE above)
//       `Attrib::Instance::operator=` @0x8280DE08 EXISTS NOWHERE -- not in b5-decomp/src and
//       not in the IDA export. All 65 slot writes go through it.
//    3. (CLEARED 2026-07-31) build/game/CAMERAS.BUNDLE is now PORTED to platform 4 by
//       tools/assets/bundles/attribsys_transcode.py + the new ice_transcode.py (X360 original
//       staged at build/game_x360_world/CAMERAS.BUNDLE). Both resources decode; the vault
//       walks with zero unattested fields; the ICE dictionary reports its 549 takes; and the
//       intro chain resolves end to end -- hash64("606002") -> classHash 0x38ED2D373887CBC7
//       -> item 0x7533C0E215246B49 -> exactly 3 iceanim shots, guids 610132 / 605855 / 605858
//       == Intro_FlyCam_Loop (40.02 s), DMV_IntroA, DMV_IntroB, all three present in the
//       dictionary. NOTE: the DictEntry stride widened 16 -> 24 for the host pointer, so the
//       bundle is a RE-LAYOUT, not an endian flip; if the real bTNode base ever lands as two
//       host pointers the take head grows 100 -> 112 B and the bundle must be REGENERATED
//       (ice_transcode.py's HEAD_SIZE carries that warning). Nothing loads it yet: ICEData.cpp
//       / ICEList.cpp / CgsDictionaryResourceType.cpp are not in the exe source list and
//       DictionaryBase::FixUp / ICETakeData::FixUp have no bodies -- so the port is
//       format-verified, not runtime-verified.
//    4. (CLEARED 2026-07-31, shot-group wave) The 65 members are now DECLARED -- the two
//       padding arrays (maPaddingBeforeICEResourceMgr[552] / maPaddingAfterICEWrapper[1064])
//       are gone and every DWARF member is named, with the whole public accessor bank
//       bodied as header inlines over them. What is left for a real Prepare is only the
//       BODY: the EventReceiverQueue drain + GameDataIO::RequestInterface<512>::
//       {GetICEList,GetVehicleList,AcquireResource} + AttribSysRequestInterface<512>::
//       RegisterVault plumbing, which is a different sub-system, plus the 101 asserts.
//       Two things the shot-group wave had to fix on the way in, both of which any
//       Prepare rewrite inherits:
//         * shotgroup / cameradefaults needed their (Collection*, owner) ELEMENT ctors
//           -- the X360's sub_827DC838 / sub_827DC8C8 -- and the key ctors had to LOSE
//           their default argument. A DirectorResourceManager is reached from the
//           file-scope static gGameModule (BrnMain.cpp:43), so all 65 slot ctors run
//           PRE-MAIN; if `shotgroup()` had stayed bound to the key ctor every slot would
//           have called Attrib::FindCollection before Attrib::Database exists.
//         * Construct() is no longer an empty stub (see the class below).
//    x64 hazards for when it IS written: Attrib::Instance is 16 B on console and 32 B here,
//       Attrib::RefSpec 20 -> 24, EventReceiverQueue<512,16> 536 -> 544. The 16-byte slot
//       stride, the 568/1592 bounds, the +0x18/+0x20 reply-payload offsets and the 8-byte
//       ResourceHandle `ld` are all console-only. And Prepare passes raw `this` to
//       Clear/GetNextEvent/GetICEList/GetVehicleList/RegisterVault, so mReceiverQueue MUST
//       stay the first member and the class MUST stay non-polymorphic (the vptr at +552
//       belongs to the embedded ICEResourceMgr, not to this class).
//
// ✅ ACCESSOR NAMES + THE FORK -- BOTH RESOLVED 2026-07-31 (shot-group wave).
//    The accessors below carry the DWARF names (GetCarSelect_MotorCity() / GetGameIntro() /
//    GetDriveThruTyreShopGroup() / GetAfterCrash() / GetJumpRig() / ...), and the SECOND
//    definition of BrnDirector::DirectorResourceManager that lived in
//    Behaviours/BrnBehaviourIceAnim.h -- ~32 declaration-only accessors, no data members,
//    mangling identically to this class and disagreeing with it about every offset -- is
//    DELETED. That header now includes this one. Every call site was migrated in the same
//    commit; the rename table is recorded at the excision site.
//    (The sibling KeyAnimController fork in that same header WAS retired, 2026-07-31.)
//
// ✅ THE OUTPUTBUFFER NAME -- FIXED 2026-08-01. The accessor Prepare calls for the vault
//    request interface (X360 @0x822069B0) was reconstructed as
//    `OutputBuffer::GetTimerRequestSubInterfaceW()` returning &mTimerRequestInterface[0x10].
//    It is now `DirectorIO::OutputBuffer::GetVaultRequestInterface()` (the DWARF spelling)
//    over a real `AttribSysRequestInterface<512> mVaultRequestInterface` member, with the
//    16-byte `CgsSystem::TimerRequestInterface` split back out ahead of it: 16 + 528 == the
//    0x220 the opaque span used to model, which is the split's own confirmation.
//
// ⚠️ DATA -- RESOLVED 2026-07-31 (was: "CAMERAS.BUNDLE is still un-ported X360"). Prepare's
//    whole point is the CameraVault, and build/game/CAMERAS.BUNDLE is now ported to platform
//    4; see blocker 3 above for what was done, what is verified and the one condition that
//    would force a regeneration.
// ============================================================================

// DWARF BrnDirectorResourceManager.h:40 / :46 -- the two selectors GetShots() takes.
// Namespace scope in BrnDirector, exactly as the DWARF has them.
enum EShotContext
{
    E_HARDSTOP_WORLD = 0,
    E_HARDSTOP_CAR   = 1
};

enum EShotDirection
{
    E_SHOTDIRECTION_LEFT  = 0,
    E_SHOTDIRECTION_RIGHT = 1
};

class DirectorResourceManager
{
public:
    // DWARF BrnDirectorResourceManager.h:245. Prepare's own switch variable; the stage
    // machine falls through 0/1 -> 2 -> 4 -> 5. Stage 3 is an ENTRY POINT only (Prepare
    // never stores it).
    enum EPrepareStage
    {
        E_PREPARESTAGE_CONSTRUCTED                 = 0,
        E_PREPARESTAGE_REQUEST_RESOURCES           = 1,
        E_PREPARESTAGE_ACQUIRE_RESOURCES           = 2,
        E_PREPARESTAGE_REGISTER_ATTRIBSYSVAULT     = 3,
        E_PREPARESTAGE_REGISTER_ATTRIBSYSVAULT_WAIT = 4,
        E_PREPARESTAGE_PREPARED                    = 5
    };

    // DWARF BrnDirectorResourceManager.h:40 / :46 -- the two GetShots() selectors.
    // (Namespace-scope on the console; nested here so they travel with the accessor.)

    // ⭐ THE REAL Construct (DWARF BrnDirectorResourceManager.cpp:37). It is FULLY INLINED
    // into DirectorModule::Construct @0x8225C590 on the console, which is why it had no
    // out-of-line symbol and sat here as `inline void Construct() {}` for so long. That
    // empty stub was NOT harmless: Prepare switches on mePrepareStage and nothing else
    // initialises it, so Prepare would have switched on uninitialised memory.
    // The console does exactly:
    //     mReceiverQueue.miCapacity = 512; mReceiverQueue.mpBuffer = this + 24;
    //     mReceiverQueue.miAlignment = 16; BaseEventReceiverQueue::Clear(this);
    //     mePrepareStage = 0; mICEResourceMgr.mpResourceManager = this;
    //     mpVehicleList = 0; mpICEDictionaryList = 0;
    // (`this + 24` is the console's own 24-byte BaseEventReceiverQueue head; the
    // EventReceiverQueue<512,16>::Construct() below is that same bind expressed by name.)
    // ⚠️ +536 mAttribsysVaultResourceHandle and +560 mpICEWrapper are deliberately LEFT
    // UNINITIALISED by the console -- reproduced.
    inline void Construct()
    {
        mReceiverQueue.Construct();
        mePrepareStage = E_PREPARESTAGE_CONSTRUCTED;
        mICEResourceMgr.Construct(this);
        mpVehicleList = 0;
        mpICEDictionaryList = 0;
    }

    // The REAL X360 signature, recovered from the only call site. DirectorModule::Prepare
    // @0x822712D8 stage 2 calls
    //     BrnDirector::DirectorResourceManager::Prepare(this+584, a2, this+2896)
    // == Prepare( &mDirectorResourceManager, <the director OUTPUT buffer>, <the ICE wrapper> ).
    // Body @0x8225CA08 -- LANDED 2026-08-01 in BrnDirectorResourceManager.cpp.
    bool Prepare(DirectorIO::OutputBuffer* lpOutputBuffer, ICEWrapper* lpHACKIceWrapper);

    // @0x821F6948. ⚠️ MOVED OUT OF LINE 2026-07-31 (shot-group wave). These three used to
    // be inline BODIES here, which forced ICEWrapper to be COMPLETE at this class's closing
    // brace -- and it is only forward-declared (its home BrnDirectorICEWrapper.h drags the
    // whole ICE manager/camera/editor cone, and forward-declares this class in turn). That
    // was invisible while the only includers happened to have pulled the wrapper's home in
    // first; the moment BrnBehaviourIceAnim.h started including this header it broke seven
    // TUs at once. GetKeyAnim IS an out-of-line X360 symbol anyway, and the DWARF's public
    // list has exactly one of it (`GetKeyAnim(ID)`), so out-of-line is also the faithful
    // shape. Bodies: BrnDirectorResourceManagerInline.cpp.
    ICE::ICETakeData* GetKeyAnim(int64_t liKeyAnimID) const;      // @0x821F6948
    ICE::ICETakeData* GetKeyAnim(const char* lpacKeyAnimName) const;
    ICE::ICEGroup*    GetShakeTakes() const;

    // @0x821F69A8 (own ledger fn -- declared for the dev-tools GameTalk handler
    // @0x822095A0). Resolve an ICE take GUID to its take data: the wrapper editor's
    // edited-take list first (ICE::ICEAuthor::FindEditedTakeFromGuid on the X360
    // rm+560 wrapper's editor), else the resource take list
    // (BrnResource::ICEList::GetICETakeDataFromGuid on the rm+544 list pointer).
    // DECLARATION-ONLY; the body lands with this manager's own TU. Both members it
    // reaches are now DECLARED below, so the body is a mechanical two-liner once that
    // TU exists.
    ICE::ICETakeData* GetKeyAnimFromGuid(s32 liGuid) const;

    const ICE::IResourceManager* GetIceResourceManager() const
    {
        return &mICEResourceMgr;
    }

    // The runtime ICE take dictionary the behaviours resolve guids through (rm+544).
    // FLAG: not in the DWARF's public accessor list -- the console reaches the member
    // directly from BehaviourIceAnim. Named after the member; ADDITIVE GROW so the
    // ICE-anim behaviour can stop carrying its own DirectorResourceManager fork.
    const BrnResource::ICEList& GetICEList() const { return *mpICEDictionaryList; }

    // The in-game ICE editor's author/edit store (rm+560 -> the ICE wrapper's author).
    // FLAG: not in the DWARF's public accessor list, same provenance as GetICEList.
    // DECLARATION-ONLY: ICEWrapper is only forward-declared here (its home is
    // BrnDirectorICEWrapper.h, where GetAuthor() lives), and this accessor's one caller
    // -- BehaviourIceAnim -- includes that home. Body lands with this manager's TU.
    ICE::ICEAuthor& GetICEAuthor() const;

    // ------------------------------------------------------------------------------
    // THE SHOT-GROUP ACCESSOR BANK.  Every one of these is a HEADER INLINE on the
    // console: the X360 image has exactly SEVEN out-of-line DirectorResourceManager
    // symbols (GetKeyAnim, GetKeyAnimFromGuid, GetEventIntroShots,
    // GetEventCompletionShots, GetVehicleInfoRef, Prepare, the ctor) and none of the
    // accessors below is among them -- checked against the export set AND against the
    // export-address gaps either side of the cluster (0x821F6300..0x821F6E00 is
    // contiguous to within its 4-byte alignment padding, so no unexported body hides
    // there). The DWARF independently declares them all in the .h at :108..:231.
    // The NAMES are the DWARF's.
    // ------------------------------------------------------------------------------
    const Attrib::Gen::shotgroup& GetDriveThruGasStationsGroup() const { return mDriveThruGasStationGroup; }  // +984
    const Attrib::Gen::shotgroup& GetDriveThruBodyShopGroup()    const { return mDriveThruBodyShopGroup; }    // +1000
    const Attrib::Gen::shotgroup& GetDriveThruTyreShopGroup()    const { return mDriveThruTyreShopGroup; }    // +1016
    const Attrib::Gen::shotgroup& GetDriveThruAutoPartsGroup()   const { return mDriveThruAutoPartsGroup; }   // +1032
    const Attrib::Gen::shotgroup& GetDriveThruTuningShopGroup()  const { return mDriveThruTuningShopGroup; }  // +1048

    const Attrib::Gen::shotgroup&    GetShakeAnimGroup()  const { return mShakeAnimsGroup; }   // +1304
    const Attrib::RefSpec&           GetAfterTouchCam()   const { return mAfterTouchCam; }     // +1608
    const Attrib::Gen::cameradefaults& GetCameraDefaults() const { return mCameraDefaults; }   // +1496

    // ⭐ THE JUNKYARD / GAME-INTRO BANK -- the fourteen the retail intro and the offline
    // car select are gated on (ArbStateCarSelect::SetupJunkyardShotgroup @0x821F64A0 and
    // the GAME_INTRO_PART_ONE/TWO/THREE states @0x8226F5D0).
    const Attrib::Gen::shotgroup& GetCarSelect_MotorCity()              const { return mCarSelectMotorCity; }              // +1064
    const Attrib::Gen::shotgroup& GetCarSelect_MotorCity_RivalUnlock()  const { return mCarSelectMotorCityRivalUnlock; }   // +1080
    const Attrib::Gen::shotgroup& GetCarSelect_WestAcres()              const { return mCarSelectWestAcres; }              // +1096
    const Attrib::Gen::shotgroup& GetCarSelect_WestAcres_RivalUnlock()  const { return mCarSelectWestAcresRivalUnlock; }   // +1112
    const Attrib::Gen::shotgroup& GetCarSelect_SouthBay()               const { return mCarSelectSouthBay; }               // +1128
    const Attrib::Gen::shotgroup& GetCarSelect_SouthBay_RivalUnlock()   const { return mCarSelectSouthBayRivalUnlock; }    // +1144
    const Attrib::Gen::shotgroup& GetCarSelect_Heartbreak()             const { return mCarSelectHeartbreak; }             // +1160
    const Attrib::Gen::shotgroup& GetCarSelect_Heartbreak_RivalUnlock() const { return mCarSelectHeartbreakRivalUnlock; }  // +1176
    const Attrib::Gen::shotgroup& GetCarSelect_LowerPeaks()             const { return mCarSelectLowerPeaks; }             // +1192
    const Attrib::Gen::shotgroup& GetCarSelect_LowerPeaks_RivalUnlock() const { return mCarSelectLowerPeaksRivalUnlock; }  // +1208
    const Attrib::Gen::shotgroup& GetCarSelect_Idle()                   const { return mCarSelectIdle; }                   // +1224
    const Attrib::Gen::shotgroup& GetCarSelect_Outro()                  const { return mCarSelectOutro; }                  // +1240
    const Attrib::Gen::shotgroup& GetCarUnlock()                        const { return mCarUnlock; }                       // +1256
    const Attrib::Gen::shotgroup& GetGameIntro()                        const { return mGameIntroGroup; }                  // +1272  ⭐ vault key "606002"

    const Attrib::Gen::shotgroup& GetBurnoutLicense() const { return mBurnoutLicense; }   // +1288
    const Attrib::Gen::shotgroup& GetJumpRig()        const { return mJumpRig; }          // +1320
    const Attrib::Gen::shotgroup& GetOnlineCarSelect() const { return mOnlineCarSelect; } // +1480

    // The online RACE-START group (+616). FLAG: the DWARF's public list has no accessor
    // for it -- the console reaches it either directly or through GetEventIntroShots
    // (whose cases 10-14 and 17, the online modes, all return this exact +0x268). Kept as
    // a named ADDITIVE GROW so ArbStateOnlineRaceIntro does not have to poke the member.
    const Attrib::Gen::shotgroup& GetOnlineRaceStart() const { return mOnlineRaceStart; } // +616

    // The four hard-stop groups, selected by context x direction. @0x8226?? is inlined;
    // the mapping below is DERIVED FROM THE MEMBER NAMES against the DWARF's own
    // EShotContext {E_HARDSTOP_WORLD=0, E_HARDSTOP_CAR=1} and EShotDirection
    // {LEFT=0, RIGHT=1}, which determines it completely. FLAG: name-derived, not
    // asm-derived -- no consumer is committed yet.
    const Attrib::Gen::shotgroup& GetShots(EShotContext leContext,
                                           EShotDirection leDirection) const
    {
        if (leContext == E_HARDSTOP_WORLD)
            return (leDirection == E_SHOTDIRECTION_LEFT) ? mHardStopWorldLeft : mHardStopWorldRight;
        return (leDirection == E_SHOTDIRECTION_LEFT) ? mHardStopCarLeft : mHardStopCarRight;
    }

    // The three crash-energy shotgroup banks (DWARF h:195/h:198/h:201; ShotSelector::
    // GetCrashShot @0x82239708.. inlines these to the direct field addresses).
    const Attrib::Gen::shotgroup& GetFastCrashShots()   const { return mFastCrashShotGroup; }    // +1400
    const Attrib::Gen::shotgroup& GetNormalCrashShots() const { return mNormalCrashShotGroup; }  // +1416
    const Attrib::Gen::shotgroup& GetSlowCrashShots()   const { return mSlowCrashShotGroup; }    // +1432

    // The two crash-MOMENT banks right after the trio above (MomentStationaryCrash::Update
    // @0x82272EA8 picks between them by its tumbling latch).
    // ⚠️ RENAMED 2026-07-31 to the DWARF spelling. They were `GetTumblingCrashShots` /
    // `GetStationaryCrashShots` here AND on the ICE-anim fork -- both invented from the
    // consumer's role. The DWARF names the members mAfterCrash / mAfterCrashSafe and the
    // accessors GetAfterCrash / GetAfterCrashSafe, which is also the pairing Prepare's own
    // assert strings use.
    const Attrib::Gen::shotgroup& GetAfterCrash()     const { return mAfterCrash; }      // +1448 (was GetTumblingCrashShots)
    const Attrib::Gen::shotgroup& GetAfterCrashSafe() const { return mAfterCrashSafe; }  // +1464 (was GetStationaryCrashShots)

    const Attrib::Gen::shotgroup& GetFailsafe()     const { return mFailsafe; }      // +1512
    const Attrib::Gen::shotgroup& GetTakedown()     const { return mTakedown; }      // +1528
    const Attrib::Gen::shotgroup& GetCrashbreaker() const { return mCrashbreaker; }  // +1544
    const Attrib::Gen::shotgroup& GetTakendown()    const { return mTakendown; }     // +1560
    const Attrib::Gen::shotgroup& GetTestbed()      const { return mTestbed; }       // +1576
    const Attrib::Gen::shotgroup& GetTestbed2()     const { return mTestbed2; }      // +1592
    const Attrib::Gen::shotgroup& GetRankUp()       const { return mRankUpGroup; }   // +968

    // @0x821F6AB8 -- the ONE genuinely out-of-line group selector: a jump-table switch on
    // the event mode. VERIFIED instruction-by-instruction from the export (18 cases,
    // jpt_821F6AF0), and it independently re-confirms nine of the offsets in the table
    // above: cases 2,16 -> +568 (0x238 mRaceStartGroup); 3 -> +584 (0x248
    // mRoadRageStartGroup); 5 -> +632 (0x278 mBurningRouteStartGroup); 7 -> +728 (0x2D8
    // mStuntRaceStartGroup); 8 -> +744 (0x2E8 mMarkedManStartGroup); 10-14,17 -> +616
    // (0x268 mOnlineRaceStart); 15 -> +696 (0x2B8 mOnlineLobbyStartGroup); 0,1,4,9 and the
    // "Invalid Event Requested" default (cpp:615) -> +600 mRaceStartRivalInFrontGroup when
    // lbCarInFront else +568. Body lands with this manager's TU (declaration-only here;
    // the mode enum is BrnGameState::GameStateModuleIO::EGameModeType, taken as s32 so
    // this header stays free of the game-state cone).
    const Attrib::Gen::shotgroup& GetEventIntroShots(s32 liEventMode, bool lbCarInFront) const;

    // @0x821F6BB8 -- the post-event completion group, selected by mode + finish line.
    // DECLARATION-ONLY (body lands with this manager's TU). liFinishLineID is the
    // GameState mFinishLineID (an 8-byte CgsID), taken as s64 for the same reason.
    const Attrib::Gen::shotgroup& GetEventCompletionShots(s32 liEventMode,
                                                          s64 liFinishLineID) const;

    // @0x82239620 -- the vehicle-info RefSpec for a car id, resolved through mpVehicleList.
    // Returned BY VALUE (the DWARF's own signature). DECLARATION-ONLY.
    Attrib::RefSpec GetVehicleInfoRef(s64 lVehicleID) const;

private:
    // [PC diagnostic] one-shot report of how many of the 65 slots bound a collection, fired
    // at the end of Prepare's stage 4. NOT an X360 function -- see the body.
    void LogShotGroupBankState() const;

    // ============================ THE REAL LAYOUT ============================
    // Member set + ORDER are the DecFIGS DWARF's (which is the console memory order);
    // the console byte offsets quoted are PROVENANCE ONLY -- every one of these types
    // widens on x64 (Attrib::Instance 16 -> 32, Attrib::RefSpec 20 -> 24,
    // EventReceiverQueue<512,16> 536 -> 544), so this is semantic parity by named
    // member, not a byte-for-byte replay.
    //
    // ⚠️ mReceiverQueue MUST STAY FIRST and this class MUST STAY NON-POLYMORPHIC:
    // Prepare passes raw `this` to Clear / GetNextEvent / GetICEList / GetVehicleList /
    // RegisterVault, i.e. it relies on `this == &mReceiverQueue`. (The vptr the console
    // shows at +552 belongs to the embedded ICEResourceMgr, not to this class.)
    CgsModule::EventReceiverQueue<512, 16> mReceiverQueue;               // +0     (:257)
    CgsResource::ResourceHandle            mAttribsysVaultResourceHandle;// +536   (:259)
    const BrnResource::ICEList*            mpICEDictionaryList;          // +544   (:262)
    EPrepareStage                          mePrepareStage;               // +548   (:263)
    ICEResourceMgr                         mICEResourceMgr;              // +552   (:266)
    ICEWrapper*                            mpICEWrapper;                 // +560   (:267)
    const BrnResource::VehicleList*        mpVehicleList;                // +564   (:269)

    // ---- the 65-slot shot-group bank (console bytes 568..1592, stride 16) ----------
    // 64 x Attrib::Gen::shotgroup + ONE Attrib::Gen::cameradefaults (mCameraDefaults,
    // console +1496) -- the ctor @0x827DEB98 builds exactly 65 sub-objects and uses a
    // DIFFERENT element ctor for that one slot, which is what pins the whole ordering.
    // The vault name key each slot is constructed over in Prepare's stage 4 is quoted;
    // the 26 slots marked (A) are additionally confirmed by their own IsValid() /
    // Num_ShotList() assert strings inside Prepare.
    Attrib::Gen::shotgroup mRaceStartGroup;                  // +568  "424409" (A)
    Attrib::Gen::shotgroup mRoadRageStartGroup;              // +584  "474399" (A)
    Attrib::Gen::shotgroup mRaceStartRivalInFrontGroup;      // +600  "428119" (A)
    Attrib::Gen::shotgroup mOnlineRaceStart;                 // +616  "428118" (A)
    Attrib::Gen::shotgroup mBurningRouteStartGroup;          // +632  "475199"
    Attrib::Gen::shotgroup mSurvivorStartGroup;              // +648  "474394"
    Attrib::Gen::shotgroup mEliminatorStartGroup;            // +664  "474388"
    Attrib::Gen::shotgroup mTrafficAttackStartGroup;         // +680  "475200"
    Attrib::Gen::shotgroup mOnlineLobbyStartGroup;           // +696  "480584"
    Attrib::Gen::shotgroup mPursuitStartGroup;               // +712  "474389" (A)
    Attrib::Gen::shotgroup mStuntRaceStartGroup;             // +728  "559418" (A)
    Attrib::Gen::shotgroup mMarkedManStartGroup;             // +744  "559367" (A)
    Attrib::Gen::shotgroup mBurningRouteFinishGroup;         // +760  "480563" (A)
    Attrib::Gen::shotgroup mMarkedManFinishGroup;            // +776  "480562" (A)
    Attrib::Gen::shotgroup mRaceFinishGroup;                 // +792  "476830" (A)
    Attrib::Gen::shotgroup mRoadRageFinishGroup;             // +808  "561160" (A)
    Attrib::Gen::shotgroup mStuntFinishGroup;                // +824  "480560" (A)
    Attrib::Gen::shotgroup mRaceFinishNorth;                 // +840  "561973" (A)
    Attrib::Gen::shotgroup mRaceFinishNorthEast;             // +856  "561960" (A)
    Attrib::Gen::shotgroup mRaceFinishEast;                  // +872  "561961" (A)
    Attrib::Gen::shotgroup mRaceFinishSouthEast;             // +888  "561962" (A)
    Attrib::Gen::shotgroup mRaceFinishSouth;                 // +904  "561963" (A)
    Attrib::Gen::shotgroup mRaceFinishSouthWest;             // +920  "561964" (A)
    Attrib::Gen::shotgroup mRaceFinishWest;                  // +936  "561965" (A)
    Attrib::Gen::shotgroup mRaceFinishNorthWest;             // +952  "561972" (A)
    Attrib::Gen::shotgroup mRankUpGroup;                     // +968  "544056" (A)
    Attrib::Gen::shotgroup mDriveThruGasStationGroup;        // +984  "428141"
    Attrib::Gen::shotgroup mDriveThruBodyShopGroup;          // +1000 "428144"
    Attrib::Gen::shotgroup mDriveThruTyreShopGroup;          // +1016 "428142"
    Attrib::Gen::shotgroup mDriveThruAutoPartsGroup;         // +1032 "428143"
    Attrib::Gen::shotgroup mDriveThruTuningShopGroup;        // +1048 "428140"
    Attrib::Gen::shotgroup mCarSelectMotorCity;              // +1064 "432577" (A)
    Attrib::Gen::shotgroup mCarSelectMotorCityRivalUnlock;   // +1080 "558663" (A)
    Attrib::Gen::shotgroup mCarSelectWestAcres;              // +1096 "450907" (A)
    Attrib::Gen::shotgroup mCarSelectWestAcresRivalUnlock;   // +1112 "558657" (A)
    Attrib::Gen::shotgroup mCarSelectSouthBay;               // +1128 "450916" (A)
    Attrib::Gen::shotgroup mCarSelectSouthBayRivalUnlock;    // +1144 "558658" (A)
    Attrib::Gen::shotgroup mCarSelectHeartbreak;             // +1160 "451080" (A)
    Attrib::Gen::shotgroup mCarSelectHeartbreakRivalUnlock;  // +1176 "558660" (A)
    Attrib::Gen::shotgroup mCarSelectLowerPeaks;             // +1192 "450948" (A)
    Attrib::Gen::shotgroup mCarSelectLowerPeaksRivalUnlock;  // +1208 "558659" (A)
    Attrib::Gen::shotgroup mCarSelectIdle;                   // +1224 "611284" (A)
    Attrib::Gen::shotgroup mCarSelectOutro;                  // +1240 "611285" (A)
    Attrib::Gen::shotgroup mCarUnlock;                       // +1256 "553098" (A)
    Attrib::Gen::shotgroup mGameIntroGroup;                  // +1272 "606002" (A)  <- the retail intro
    Attrib::Gen::shotgroup mBurnoutLicense;                  // +1288 "605835" (A)
    Attrib::Gen::shotgroup mShakeAnimsGroup;                 // +1304 "428114"
    Attrib::Gen::shotgroup mJumpRig;                         // +1320 "440805" (A)
    Attrib::Gen::shotgroup mHardStopWorldLeft;               // +1336 "461063"
    Attrib::Gen::shotgroup mHardStopWorldRight;              // +1352 "461057"
    Attrib::Gen::shotgroup mHardStopCarLeft;                 // +1368 "466945"
    Attrib::Gen::shotgroup mHardStopCarRight;                // +1384 "466946"
    Attrib::Gen::shotgroup mFastCrashShotGroup;              // +1400 "494628" (A)
    Attrib::Gen::shotgroup mNormalCrashShotGroup;            // +1416 "543590" (A)
    Attrib::Gen::shotgroup mSlowCrashShotGroup;              // +1432 "542963" (A)
    Attrib::Gen::shotgroup mAfterCrash;                      // +1448 "461719" (A)
    Attrib::Gen::shotgroup mAfterCrashSafe;                  // +1464 "466949" (A)
    Attrib::Gen::shotgroup mOnlineCarSelect;                 // +1480 "613970"
    Attrib::Gen::cameradefaults mCameraDefaults;             // +1496 "430819"  <- the 65th, and
                                                             //   the ONLY non-shotgroup slot
    Attrib::Gen::shotgroup mFailsafe;                        // +1512 "467917" (A)
    Attrib::Gen::shotgroup mTakedown;                        // +1528 "475241"
    Attrib::Gen::shotgroup mCrashbreaker;                    // +1544 "478055" (A)
    Attrib::Gen::shotgroup mTakendown;                       // +1560 "575796" (A)
    Attrib::Gen::shotgroup mTestbed;                         // +1576 "535488" (A)
    Attrib::Gen::shotgroup mTestbed2;                        // +1592 "568320" (A)

    // +1608 (:357). The console ctor clears the WHOLE 20-byte record (std/std/stw); the
    // default RefSpec ctor below does the same three fields by name.
    Attrib::RefSpec        mAfterTouchCam;                   // +1608 "428410"
};

// ICEResourceMgr::Construct -- DWARF BrnDirectorResourceManager.h:64, i.e. declared in the
// header, and there is no out-of-line X360 symbol for it (the image's only ICEResourceMgr
// function is GetTakeData @0x821F6A00): the console inlines the single store into
// DirectorModule::Construct @0x8225C590, where it reads `mICEResourceMgr.mpResourceManager
// = this`. Defined out-of-class here only because DirectorResourceManager is incomplete at
// the point ICEResourceMgr is declared.
//
// It became LINK-REQUIRED on 2026-07-31: DirectorResourceManager::Construct stopped being
// an empty stub, and calling it is what surfaces this member.
inline void ICEResourceMgr::Construct(DirectorResourceManager* lpResourceManager)
{
    mpResourceManager = lpResourceManager;
}

}

#endif
