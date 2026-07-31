#ifndef BRN_DIRECTOR_RESOURCE_MANAGER_H
#define BRN_DIRECTOR_RESOURCE_MANAGER_H

#include "SDKs/Packages/ICE/ICEData.hpp"
#include "types.hpp"
#include "GameShared/GameClasses/System/Resource/CgsResourceID.h"   // CgsResource::ID (GetICETakeData arg / MakeICEMovieId return)
#include <cstdio>                                                   // snprintf (GetKeyAnim name formatting)

namespace Attrib { namespace Gen { class shotgroup; } }   // GameSource/AttribSys/Generated/classes/shotgroup.h

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
//    (nullptr, nullptr). sub_827DC8C8 has no IDA export and its body could not be read; it is
//    INFERRED to be the cameradefaults sibling. The ctor then clears bytes 1608..1627 as
//    std/std/stw (8+8+4 == the whole 20-byte Attrib::RefSpec mAfterTouchCam) -- the committed
//    BrnDirectorResourceManager.cpp writes only 3 x 4 bytes there and leaves 1612..1615 and
//    1620..1623 uninitialised.
//
// WHAT BLOCKS A REAL Prepare TODAY (the RECOVERY is complete as of 2026-07-31; these four are
// what stop it being written):
//    1. `Attrib::Gen::shotgroup::shotgroup` (shotgroup.h:105) THROWS THE KEY AWAY. The real
//       Attrib::FindCollection @0x82808378 is FindCollection(u64 luClassKey /*r3*/,
//       u32 luCollectionKey /*r4*/) -- asm-verified -- and the shotgroup ctor sets only r3,
//       letting the caller's StringToKey result pass through in r4. This repo declares
//       FindCollection(int, void* = nullptr) and the ctor body has the key parameter
//       COMMENTED OUT, so all 65 constructions would resolve the same wrong collection. Same
//       defect in cameradefaults.h, which additionally declares a ONE-argument ctor where the
//       X360 symbol takes (this, key, owner).
//    2. `Attrib::Instance::operator=` @0x8280DE08 EXISTS NOWHERE -- not in b5-decomp/src and
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
//    4. The 65 members do not EXIST as declared members here -- the class below is still
//       maPaddingBeforeICEResourceMgr[552] / mICEResourceMgr / mpICEWrapper /
//       maPaddingAfterICEWrapper[1064]. Only +552 and +560 are named.
//    x64 hazards for when it IS written: Attrib::Instance is 16 B on console and 32 B here,
//       Attrib::RefSpec 20 -> 24, EventReceiverQueue<512,16> 536 -> 544. The 16-byte slot
//       stride, the 568/1592 bounds, the +0x18/+0x20 reply-payload offsets and the 8-byte
//       ResourceHandle `ld` are all console-only. And Prepare passes raw `this` to
//       Clear/GetNextEvent/GetICEList/GetVehicleList/RegisterVault, so mReceiverQueue MUST
//       stay the first member and the class MUST stay non-polymorphic (the vptr at +552
//       belongs to the embedded ICEResourceMgr, not to this class).
//
// ⚠️ ACCESSOR NAMES. The DWARF spells them GetCarSelect_MotorCity() / GetGameIntro() /
//    GetDriveThruTyreShopGroup() etc. The committed CALLERS (the seven ICE-anim arbitrator
//    states) currently call the names invented by the minimal DirectorResourceManager slice in
//    Behaviours/BrnBehaviourIceAnim.h -- GetCarSelectMotorCityShots(), GetGameIntroShots(), ...
//    The rebuild must reconcile to the DWARF names AND retire that slice: it is a SECOND
//    definition of BrnDirector::DirectorResourceManager with a completely different layout,
//    and both are already in the same program (this header via DirectorLinkStubs.cpp, the fork
//    via every arbitrator state). They mangle identically, so they will link and then disagree
//    about where every member is. Nothing calls across the two today only because the ICE-anim
//    states are all unmounted -- and because every accessor on the fork is declaration-only,
//    so the disagreement surfaces as an unresolved external rather than as silent corruption.
//    Landing a real Prepare turns it from latent into live.
//    (The sibling KeyAnimController fork in that same header WAS retired, 2026-07-31.)
//
// ONE MORE NAME TO FIX ON THE WAY IN: the OutputBuffer accessor Prepare calls for the vault
//    request interface (X360 @0x822069B0) is reconstructed as
//    `OutputBuffer::GetTimerRequestSubInterfaceW()` returning &mTimerRequestInterface[0x10]
//    (BrnDirectorModuleIOOutputBuffer.cpp:136). The DWARF for this TU spells it
//    `DirectorIO::OutputBuffer::GetVaultRequestInterface`, and its result feeds
//    AttribSysRequestInterface<512>::RegisterVault directly -- so the member at OutputBuffer
//    +0x510 is the ATTRIBSYS VAULT request interface, not a timer sub-interface.
//
// ⚠️ DATA -- RESOLVED 2026-07-31 (was: "CAMERAS.BUNDLE is still un-ported X360"). Prepare's
//    whole point is the CameraVault, and build/game/CAMERAS.BUNDLE is now ported to platform
//    4; see blocker 3 above for what was done, what is verified and the one condition that
//    would force a regeneration.
// ============================================================================

class DirectorResourceManager
{
public:
    inline void Construct() {}

    bool Prepare(ICEWrapper* lpHACKIceWrapper);

    // ADDITIVE (director wave): the REAL X360 signature, recovered from the only call site.
    // DirectorModule::Prepare @0x822712D8 stage 2 calls
    //     BrnDirector::DirectorResourceManager::Prepare(this+584, a2, this+2896)
    // == Prepare( &mDirectorResourceManager, <the director OUTPUT buffer>, <the ICE wrapper> ).
    // The manager pumps its own staged resource requests (GetICEList / GetVehicleList, the
    // AttribSys vault registration, then the cameradefaults + the five crash/jump shotgroup
    // banks) through the GameData + AttribSys request interfaces published in that output
    // buffer, which is why it needs it; body @0x8225CA08 (own TU, declaration-only here).
    // The pre-existing single-argument overload above is kept untouched.
    bool Prepare(DirectorIO::OutputBuffer* lpOutputBuffer, ICEWrapper* lpHACKIceWrapper);

    inline ICE::ICETakeData* GetKeyAnim( int64_t liKeyAnimID) const
    {
        char lacICEName[16];
        snprintf(lacICEName, 16, "ICE_WLDSIG%d", (int32_t)liKeyAnimID);
        return GetKeyAnim(lacICEName);
    }

    inline ICE::ICETakeData* GetKeyAnim( const char* lpacKeyAnimName) const
    {
        return mpICEWrapper->GetICETakeData(BrnResource::MakeICEMovieId( lpacKeyAnimName ));
    }

    inline ICE::ICEGroup* GetShakeTakes() const
    {
        return mpICEWrapper->GetShakeGroup();
    }

    // @0x821F69A8 (own ledger fn -- declared for the dev-tools GameTalk handler
    // @0x822095A0). Resolve an ICE take GUID to its take data: the wrapper editor's
    // edited-take list first (ICE::ICEAuthor::FindEditedTakeFromGuid on the X360
    // rm+560 wrapper's editor), else the resource take list
    // (BrnResource::ICEList::GetICETakeDataFromGuid on the rm+544 list pointer --
    // both interiors live in maPaddingBeforeICEResourceMgr here). DECLARATION-ONLY;
    // the body lands with this manager's own TU.
    ICE::ICETakeData* GetKeyAnimFromGuid(s32 liGuid) const;

    const ICE::IResourceManager* GetIceResourceManager() const
    {
        return &mICEResourceMgr;
    }

    // The three crash-energy shotgroup banks (DWARF h:351/h:357/h:354; members
    // mFastCrashShotGroup/mNormalCrashShotGroup/mSlowCrashShotGroup at X360
    // manager+1400/+1416/+1432 -- ShotSelector::GetCrashShot @0x82239708.. inlines
    // these accessors to those direct field addresses). ADDITIVE GROW:
    // declaration-only (the member bank + bodies land with this manager's own TU).
    const Attrib::Gen::shotgroup& GetFastCrashShots() const;
    const Attrib::Gen::shotgroup& GetNormalCrashShots() const;
    const Attrib::Gen::shotgroup& GetSlowCrashShots() const;

    // The two crash-moment shotgroup banks right after the trio above (X360
    // manager+1448/+1464, the same 16-byte stride -- MomentStationaryCrash::Update
    // @0x82272EA8 picks between them by its tumbling latch). ADDITIVE GROW:
    // declaration-only (the member bank + bodies land with this manager's own TU).
    const Attrib::Gen::shotgroup& GetTumblingCrashShots() const;     // +1448
    const Attrib::Gen::shotgroup& GetStationaryCrashShots() const;   // +1464

    // The player-jumping ICE shot-group instance (X360 manager+1320 / +0x528 --
    // MomentPlayerJumping::Prepare @0x82251048 reads *(behaviour-manager
    // mpDirectorResourceManager)+0x528 and resolves its ShotList attribute, key
    // 0x7533C0E2_15246B49, for up to five ice shots). ADDITIVE GROW:
    // declaration-only (the member + body land with this manager's own TU).
    // FLAG: accessor name inferred from the consumer's role.
    const Attrib::Gen::shotgroup& GetPlayerJumpingShots() const;      // +1320

private:
    // X360 members preceding mICEResourceMgr occupy 552 bytes. Their concrete
    // resource queue/handle types are owned by their respective TUs.
    u8 maPaddingBeforeICEResourceMgr[552];
    ICEResourceMgr mICEResourceMgr;
    ICEWrapper* mpICEWrapper;
    u8 maPaddingAfterICEWrapper[1064];
};

}

#endif
