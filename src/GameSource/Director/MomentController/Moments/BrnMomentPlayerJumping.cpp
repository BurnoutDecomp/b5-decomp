#include "GameSource/Director/MomentController/Moments/BrnMomentPlayerJumping.h"

#include <math.h>                                                     // fabsf (the grounded test's sign clear)
#include <cstdlib>                                                    // [DIAG] getenv (BRN_CRASHCAM_DIAG)
#include <cstdio>                                                     // [DIAG] snprintf (the tick witness line)
#include "GameShared/GameClasses/Core/CgsAssert.h"                    // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"            // CgsDev::Log::WriteToLog (the [pjump] traps, NOT X360)
#include "GameSource/Director/Camera/BrnBehaviourParameterBank.h"     // the jump rig/bystander blocks
#include "GameSource/Director/BrnDirectorICEWrapper.h"                // ICEWrapper (the resource-manager header's inlines need it complete first)
#include "GameSource/Director/BrnDirectorResourceManager.h"           // the player-jumping shot group
#include "GameSource/AttribSys/Generated/classes/shotgroup.h"         // Attrib::Gen::shotgroup (Num_ShotList / ShotList)
#include "GameSource/Director/DirectorModule/BrnDirectorGameState.h"  // GameState (read by name)
#include "GameSource/Director/MomentController/BrnMomentSharedInfo.h" // MomentSharedInfo (read by name)

// ============================================================================
// GameSource/Director/MomentController/Moments/BrnMomentPlayerJumping.cpp
//
// BrnDirector::MomentPlayerJumping -- the "player jumping" moment (type 7): while the player is airborne off a jump
// with no authored camera, cut to an attached rig shot. ArbStateRoaming::Construct @0x82259C00 registers it ({7, 0}),
// so on the console it is allocated and ticked on every roaming frame.
//
// ⭐ INERT ON RETAIL. Its SEARCHING arm has three gates, and the third is lSharedInfo.mbAllowJumpMoment (`lbz r11,
// 0x524(r28)` @0x82275C44). That flag is MainDirector +0x3542C, a dev tweakable (DWARF BrnDirectorModule.h:296).
// MainDirector::Construct seeds it FALSE (`stbx r31(=0), r30, r7` @0x8225B97C), and nothing in the ARTIST image
// writes it again. The only other instruction naming the offset is UpdateMoments' copy into the record
// (`lbzx` @0x82250350), and no data word holds 0x3542C. So the console never leaves SEARCHING: every roaming frame
// integrates the landing cooldown, finds the gates shut, and reports not-met / can't-switch / head bit 18.
//
// [FX-DIRECTOR2 2026-09-25] The retail path is re-derived from the X360, and the moment is allocated (NewMoment case
// 7, un-gated):
//   * Construct @0x8225F1F0, Prepare @0x82251048, Update's SEARCHING arm and Release @0x8223AED0 -- the record is
//     read BY NAME. The five detail:: reach shims this TU went through are gone, and BrnMomentSharedInfo.cpp keeps
//     them only for PlayerStunt.
//   * The collection members retail reaches are bodied: Construct (inlined in the moment's Construct), AddShot
//     (0x8224B1B8 rig-6 / 0x8224B0A8 bystander / 0x8224B130 rig-4; the ice copy is inlined in Prepare) and Release
//     (0x8222DF20 / 0x8222DDB0 / 0x8222DE68 / 0x8222E030).
//   * [PC TRAP, NOT X360] What retail never reaches is a LOUD trap: announced in the game log once, then
//     CGS_ASSERT(false) every time. Never a silent no-op. The traps are the collections' Prepare / HasFailed /
//     CanSwitchToMeNow and MomentPlayerJumping::UpdateCamera @0x8223AB78. The only way in is the gate above.
//     DELETE-WHEN: the collection bodies and UpdateCamera are re-derived (the filming closure).
// ============================================================================

namespace BrnDirector
{

namespace
{
    // DWARF BrnMomentPlayerJumping.cpp:21..:23 (kfCooldownTime / kfMaxJumpTime / kfMinTimeInAir).
    const f32 KF_COOLDOWN_TIME   = 0.5f;   // flt_82001DA0 == 0x3F000000 (Construct's cooldown seed, 0x8225F2D8)
    const f32 KF_MAX_JUMP_TIME   = 2.0f;   // flt_82001D9C == 0x40000000 (VALID releases past it, 0x82275ACC)
    const f32 KF_MIN_TIME_IN_AIR = 0.1f;   // flt_82004014 == 0x3DCCCCCD (the Jump_Effect start-hook window, 0x82275A90)

    // flt_82001770 == 0x34000000 (FLT_EPSILON): SEARCHING's grounded test compares |time in air| against it (0x82275BB4).
    const f32 KF_GROUNDED_EPSILON = 1.1920928955078125e-7f;

    // flt_82001CC0 == 0: Release's cooldown, the jump timer's reset, VALID's "landed" compare.
    const f32 KF_ZERO = 0.0f;

    // flt_82001C98 == 0x3F800000: the Jump_Effect start hook's blend amount (0x82275ABC).
    const f32 KF_JUMP_EFFECT_BLEND = 1.0f;

    // flt_82CDA5E0 == 0x3E19999A (0.15f), a .data word: the border post-FX amount VALID stamps on the camera
    // (0x82275B30). All eight references in the image are loads (findinit), so the image value is the value.
    const f32 KF_JUMP_BORDER_POSTFX_AMOUNT = 0.15f;

    // GameState::miThisFramesActionFlags bit 3: "the player is jumping with no authored camera" (`rlwinm r10, r10,
    // 0,28,28` @0x82275C2C). The producer is MainDirector::ProcessInputQueue's jump arms (case 56 / 58, `|= 0x8`).
    const s32 KI_ACTION_FLAG_JUMP = 0x8;

    // The moment camera's head bits (`oris 4` / `oris 8` / `oris 0x80` on the head word at moment +0x148).
    const u32 KU_HEAD_FLAG_SEARCHING = 18;
    const u32 KU_HEAD_FLAG_PREPARING = 19;
    const u32 KU_HEAD_FLAG_INHIBITED = 23;

    // The produced camera's state bits VALID raises (`ori 0x80` / `ori 2` on the u64 at camera +0x140).
    const u32 KU_CAMERA_FLAG_1 = 1;
    const u32 KU_CAMERA_FLAG_7 = 7;

    // [PC TRAP, NOT X360] a step this host has no body for: announced in the game log the first time
    // (unconditionally -- no knob), then CGS_ASSERT(false) every time.
    void PlayerJumpingTrap(bool& arbAnnounced, const char* lpcAnnouncement, const char* lpcAssert)
    {
        if (!arbAnnounced)
        {
            arbAnnounced = true;
            CgsDev::Log::WriteToLog(lpcAnnouncement);
        }
        CGS_ASSERT(false, lpcAssert);
    }

    // [DIAG] NOT IN THE console BINARY. The jump moment's tick witness, behind BRN_CRASHCAM_DIAG (the moment diag
    // gate, default off): the first SEARCHING tick, the 300th, and the first that sees the jump action flag -- each
    // with the three gates, so a log shows the moment allocated, ticked and (on retail) shut. Reads only.
    void JumpMomentTickDiag(const GameState& lrGameState, const MomentSharedInfo& lrSharedInfo)
    {
        static const bool sbDiag = (getenv("BRN_CRASHCAM_DIAG") != 0);
        static s32 siTicks = 0;
        static bool sbSawJump = false;
        if (!sbDiag)
        {
            return;
        }
        ++siTicks;
        const bool lbJump = (lrGameState.miThisFramesActionFlags & KI_ACTION_FLAG_JUMP) != 0;
        const bool lbFirstJump = lbJump && !sbSawJump;
        sbSawJump = sbSawJump || lbJump;
        if (siTicks == 1 || siTicks == 300 || lbFirstJump)
        {
            char lacLine[128];
            snprintf(lacLine, sizeof(lacLine),
                     "[pjump] SEARCHING tick %d: jumpFlag=%d canUseSlomo=%d allowJumpMoment=%d\n", siTicks,
                     lbJump ? 1 : 0, lrGameState.mbCanUseSlomo ? 1 : 0, lrSharedInfo.mbAllowJumpMoment ? 1 : 0);
            CgsDev::Log::WriteToLog(lacLine);
        }
    }
}

// ============================================================================
// BehaviourCollection<TBehaviour, TParameters, N> -- the members the retail path reaches.
// ============================================================================

// Construct (DWARF h:50), inlined four times in MomentPlayerJumping::Construct (rig-6 0x8225F238..0x8225F260,
// bystander ..0x8225F278, rig-4 ..0x8225F290, ice ..0x8225F2A8): the two owners, both latches, the current index
// and the shot array's count word (the Array's Construct -- `stw 0` at the collection's +N*0x18).
template <typename TBehaviour, typename TParameters, u32 N>
void BehaviourCollection<TBehaviour, TParameters, N>::Construct(const ArbitratorState* lpArbStateOwner,
                                                                const Moment* lpMomentOwner)
{
    mpArbStateOwner    = lpArbStateOwner;
    mpMomentOwner      = lpMomentOwner;
    mbAllocatedShots   = false;
    mbHasCurrentCamera = false;
    miCurrentCamera    = 0;
    maShots.Construct();
}

// AddShot (DWARF h:76; the body's assert is BrnMomentPlayerJumping.cpp:159): rig-6 @0x8224B1B8, bystander
// @0x8224B0A8, rig-4 @0x8224B130; the ice copy is inlined in Prepare (0x822511AC..0x822511F4).
//   assert !mbAllocatedShots; a stack Shot whose handle is cleared (`stb 0` +0x00, `stw 0` +0x10 / +0x0C / +0x08 /
//   +0x04) and whose mpParams (+0x14) is the argument; maShots.Append (0x8222F9E0 / 0x8222F740 / 0x8222F890 / the
//   ice 0x8222FBE8: the "used before Construct" and "out of space" asserts, the 24-byte copy, count + 1).
template <typename TBehaviour, typename TParameters, u32 N>
void BehaviourCollection<TBehaviour, TParameters, N>::AddShot(const TParameters* lpParams)
{
    CGS_ASSERT(!mbAllocatedShots,
               "can't add shots when the behaviours have been allocated (although this could be changed)");   // :159

    Shot lShot;                 // BehaviourHandle() clears the five handle words
    lShot.mpParams = lpParams;
    maShots.Append(lShot);
}

// Release (DWARF h:57): bystander @0x8222DDB0, rig-4 @0x8222DE68, rig-6 @0x8222DF20, ice @0x8222E030.
//   Only when mbAllocatedShots: every shot's handle released as BehaviourHandle::Release inlines it (allocated ->
//   UnSetBehaviourUsedByHandle(manager +0x0C, key +0x04), then +0x08 / +0x0C / +0x10 / +0x00 zeroed). The count
//   is re-read through the checked GetLength (CgsArray.h:336) every pass, and the element comes through the
//   checked operator[] (sub_82202550 ...). Then mbHasCurrentCamera = 0, mbAllocatedShots = 0. Always true.
template <typename TBehaviour, typename TParameters, u32 N>
bool BehaviourCollection<TBehaviour, TParameters, N>::Release()
{
    if (mbAllocatedShots)
    {
        for (u32 luShot = 0; luShot < maShots.GetLength(); ++luShot)
        {
            maShots[luShot].mHandle.Release();
        }
        mbHasCurrentCamera = false;
        mbAllocatedShots   = false;
    }
    return true;
}

// ============================================================================
// BehaviourCollection -- [PC TRAP, NOT X360] the members retail never reaches (only PrepareCameras and
// GetCameraStatus call them, behind the mbAllowJumpMoment gate).
// ============================================================================

// Prepare (DWARF h:54): bystander @0x82267C40, rig-4 @0x82267D88, rig-6 @0x82267ED0, ice @0x82273360. Reports
// failure, which PrepareCameras asserts on.
template <typename TBehaviour, typename TParameters, u32 N>
bool BehaviourCollection<TBehaviour, TParameters, N>::Prepare(Camera::BehaviourManager& /*lrBehaviourManager*/)
{
    static bool sbAnnounced = false;
    PlayerJumpingTrap(sbAnnounced,
                      "[pjump] TRAP (PC, NOT X360): BehaviourCollection<>::Prepare (X360 bystander 0x82267C40 / "
                      "rig-4 0x82267D88 / rig-6 0x82267ED0 / ice 0x82273360) has no host body: the jump shot's "
                      "behaviours are NOT allocated. Reached only with MainDirector::mbAllowJumpMoment set, which "
                      "retail never does.\n",
                      "BehaviourCollection::Prepare has no host body (the jump moment's filming closure)");
    return false;
}

// HasFailed (DWARF h:72): bystander @0x821FD118, rig-4 @0x821FD238, rig-6 @0x821FD358, ice @0x821FD5B8. Reports
// FAILED, so FOUND_PREPARING releases the moment back to SEARCHING.
template <typename TBehaviour, typename TParameters, u32 N>
bool BehaviourCollection<TBehaviour, TParameters, N>::HasFailed()
{
    static bool sbAnnounced = false;
    PlayerJumpingTrap(sbAnnounced,
                      "[pjump] TRAP (PC, NOT X360): BehaviourCollection<>::HasFailed (X360 bystander 0x821FD118 / "
                      "rig-4 0x821FD238 / rig-6 0x821FD358 / ice 0x821FD5B8) has no host body: reported FAILED.\n",
                      "BehaviourCollection::HasFailed has no host body (the jump moment's filming closure)");
    return true;
}

// CanSwitchToMeNow (DWARF h:66): bystander @0x821FD088, rig-4 @0x821FD1A8, rig-6 @0x821FD2C8, ice @0x821FD528.
template <typename TBehaviour, typename TParameters, u32 N>
bool BehaviourCollection<TBehaviour, TParameters, N>::CanSwitchToMeNow()
{
    static bool sbAnnounced = false;
    PlayerJumpingTrap(sbAnnounced,
                      "[pjump] TRAP (PC, NOT X360): BehaviourCollection<>::CanSwitchToMeNow (X360 bystander "
                      "0x821FD088 / rig-4 0x821FD1A8 / rig-6 0x821FD2C8 / ice 0x821FD528) has no host body: "
                      "reported NOT ready.\n",
                      "BehaviourCollection::CanSwitchToMeNow has no host body (the jump moment's filming closure)");
    return false;
}

// ============================================================================
// Construct @0x8225F1F0 (DWARF BrnMomentPlayerJumping.cpp:177)
//   The inlined Moment::Construct (meState 0, meType through vtable +0x1C, mbIsInhibited 0, Camera::Construct(+0x10));
//   the four collections' Constructs with no arbitrator-state owner and this moment as owner (rig-6, bystander,
//   rig-4, ice); the interpolate handle's five words zeroed (+0x37C..+0x38C); the interpolate parameters' inlined
//   Construct {8, no name, slerp, sinusoidal} with the method overwritten to rotate-about-player-car (`stw 1,
//   0x374`); the cooldown seed (+0x394), meType -1 (+0x39C), mbPrepared 0 (+0x398), mpParameters 0 (+0x180); and
//   LAST the mapping override (`stw 3, 0x378`, exponential-out-x-cubed).
// ============================================================================
void MomentPlayerJumping::Construct()
{
    Moment::Construct();
    mRigCollection.Construct(0, this);
    mBystanderCollection.Construct(0, this);
    mDroppedRigCollection.Construct(0, this);
    mIceCollection.Construct(0, this);
    mInterpolater.Clear();
    mInterpolaterParams.Construct();
    mInterpolaterParams.meInterpolationMethod =
        Camera::BehaviourInterpolate::E_METHOD_ROTATE_ABOUT_PLAYER_CAR;
    mfCooldownTimer = KF_COOLDOWN_TIME;
    meType          = E_INVALID_TYPE;
    mbPrepared      = false;
    mpParameters    = 0;
    mInterpolaterParams.meInterpolationMapping =
        Camera::BehaviourInterpolate::E_MAPPING_EXPONENTIAL_OUT_X_CUBED;
}

// ============================================================================
// Prepare @0x82251048 (DWARF :209)
//   meState = SEARCHING (`stw 1, 0x174`) before the once-only guard. First call only:
//     the attached-rig shots  -- bank rig slots 1, 2, 7, 10, 9, 5 (manager +0x12540 + 0x1270 / 0x1390 / 0x1930 /
//                                0x1C90 / 0x1B70 / 0x16F0), AddShot rig-6 @0x8224B1B8
//     the bystander shots     -- bystander slots 0, 2 (+0xD08 / +0xE40), AddShot @0x8224B0A8
//     the dropped-rig shots   -- rig slots 11, 12, 13 (+0x1DB0 / +0x1ED0 / +0x1FF0), AddShot rig-4 @0x8224B130
//     the ice shots           -- the resource manager's (manager +0x163BC) jump-rig shot group (+0x528): its ShotList
//                                length (Instance::Get + Attribute::GetLength, key 0x7533C0E2_15246B49), clamped
//                                to five (`cmplwi 5 ; bgt`, unsigned); per shot the element (GetAttributePointer,
//                                the 24-byte DefaultDataArea when null) through the inlined ice AddShot
//     mbPrepared = 1.
//   Returns mbPrepared, re-read.
// ============================================================================
bool MomentPlayerJumping::Prepare(void* lrBehaviourController)
{
    Camera::BehaviourManager& lrBehaviourManager = *static_cast<Camera::BehaviourManager*>(lrBehaviourController);

    SetState(E_STATE_INVALID_SEARCHING);
    if (!mbPrepared)
    {
        const Camera::BehaviourParameterBank& lrBank = lrBehaviourManager.GetBehaviourParameterBank();

        mRigCollection.AddShot(&lrBank.GetPlayerJumpingRigShotParams(1));    // mRigRearQFwd
        mRigCollection.AddShot(&lrBank.GetPlayerJumpingRigShotParams(2));    // mRigFrontQCuFwd
        mRigCollection.AddShot(&lrBank.GetPlayerJumpingRigShotParams(7));    // mRigRoofFwd
        mRigCollection.AddShot(&lrBank.GetPlayerJumpingRigShotParams(10));   // mRigUnderbelly
        mRigCollection.AddShot(&lrBank.GetPlayerJumpingRigShotParams(9));    // mRigFrontQCuFwd2
        mRigCollection.AddShot(&lrBank.GetPlayerJumpingRigShotParams(5));    // mRigBootViewFwd

        mBystanderCollection.AddShot(&lrBank.GetPlayerJumpingBystanderShotParams(0));   // mBystanderJumpLeftParameters
        mBystanderCollection.AddShot(&lrBank.GetPlayerJumpingBystanderShotParams(2));   // mBystanderJumpFromBehindParameters

        mDroppedRigCollection.AddShot(&lrBank.GetPlayerJumpingRigShotParams(11));   // mRigDropUnderbelly
        mDroppedRigCollection.AddShot(&lrBank.GetPlayerJumpingRigShotParams(12));   // mRigDropFrontQCuFwd
        mDroppedRigCollection.AddShot(&lrBank.GetPlayerJumpingRigShotParams(13));   // mRigDropBootViewFwd

        const Attrib::Gen::shotgroup& lrJumpShots = lrBehaviourManager.GetDirectorResourceManager()->GetJumpRig();
        u32 luNumShots = lrJumpShots.Num_ShotList();
        if (luNumShots > 5u)
        {
            luNumShots = 5u;
        }
        for (u32 luShot = 0; luShot < luNumShots; ++luShot)
        {
            mIceCollection.AddShot(lrJumpShots.ShotList(luShot));
        }

        mbPrepared = true;
    }
    return mbPrepared;
}

// ============================================================================
// Release @0x8223AED0 (DWARF :680)
//   mfCooldownTimer = 0; the four collections released (rig-6, bystander, rig-4, ice); the interpolate handle
//   released as BehaviourHandle::Release inlines it; mbConditionsMet (+0x17A) = 0, mbCanSwitchToMeNow (+0x178) = 0,
//   head bit 18, meState = SEARCHING (`stw 1, 0x174` -- not INACTIVE). Returns 1.
// ============================================================================
bool MomentPlayerJumping::Release()
{
    mfCooldownTimer = KF_ZERO;
    mRigCollection.Release();
    mBystanderCollection.Release();
    mDroppedRigCollection.Release();
    mIceCollection.Release();
    mInterpolater.Release();
    SetConditionsNotMet();
    SetCanSwitchToMeNow(false);
    GetNonConstCamera().mState.SetHeadFlag(KU_HEAD_FLAG_SEARCHING);
    SetState(E_STATE_INVALID_SEARCHING);
    return true;
}

// GetName @0x821F7630 (DWARF :745)
const char* MomentPlayerJumping::GetName() const
{
    return "MomentPlayerJumping";
}

// GetInstanceType @0x821F7628 (DWARF :731)
Moment::EType MomentPlayerJumping::GetInstanceType()
{
    return E_MOMENT_PLAYER_JUMPING;
}

// ============================================================================
// PrepareCameras @0x822755A0 (DWARF :410)
//   switch (meType), jpt_822755D4: 0 ice / 1 rig-4 then rig-6 / 2 bystander then rig-6 / 3 bystander / 4 rig-4 /
//   5 rig-6; each collection's Prepare asserted (:416 / :422 / :423 / :429 / :430 / :436 / :442 / :448), never
//   gating. default asserts "invalid type" (:454). Always returns 1.
// ============================================================================
bool MomentPlayerJumping::PrepareCameras(Camera::BehaviourManager& lrBehaviourManager)
{
    switch (meType)
    {
    case E_TYPE_ICE_RIG:
    {
        const bool lbIce = mIceCollection.Prepare(lrBehaviourManager);
        CGS_ASSERT(lbIce, "mIceCollection.Prepare(lBehaviourManager)");
        break;
    }
    case E_TYPE_DROPPED_RIG_THEN_ATTACHED_RIG_SEQUENCE:
    {
        const bool lbDropped = mDroppedRigCollection.Prepare(lrBehaviourManager);
        CGS_ASSERT(lbDropped, "mDroppedRigCollection.Prepare(lBehaviourManager)");
        const bool lbRig = mRigCollection.Prepare(lrBehaviourManager);
        CGS_ASSERT(lbRig, "mRigCollection.Prepare(lBehaviourManager)");
        break;
    }
    case E_TYPE_BYSTANDER_THEN_ATTACHED_RIG_SEQUENCE:
    {
        const bool lbBystander = mBystanderCollection.Prepare(lrBehaviourManager);
        CGS_ASSERT(lbBystander, "mBystanderCollection.Prepare(lBehaviourManager)");
        const bool lbRig = mRigCollection.Prepare(lrBehaviourManager);
        CGS_ASSERT(lbRig, "mRigCollection.Prepare(lBehaviourManager)");
        break;
    }
    case E_TYPE_BYSTANDER_SHOT:
    {
        const bool lbBystander = mBystanderCollection.Prepare(lrBehaviourManager);
        CGS_ASSERT(lbBystander, "mBystanderCollection.Prepare(lBehaviourManager)");
        break;
    }
    case E_TYPE_DROPPED_RIG_SHOT:
    {
        const bool lbDropped = mDroppedRigCollection.Prepare(lrBehaviourManager);
        CGS_ASSERT(lbDropped, "mDroppedRigCollection.Prepare(lBehaviourManager)");
        break;
    }
    case E_TYPE_ATTACHED_RIG_SHOT:
    {
        const bool lbRig = mRigCollection.Prepare(lrBehaviourManager);
        CGS_ASSERT(lbRig, "mRigCollection.Prepare(lBehaviourManager)");
        break;
    }
    default:
        CGS_ASSERT(false, "invalid type");
        break;
    }
    return true;
}

// ============================================================================
// GetCameraStatus @0x8220A358 (DWARF :465)
//   switch (meType), jpt_8220A390: the sequence's LEADING collection only -- HasFailed -> FAILED (2), else
//   CanSwitchToMeNow -> READY (1), else WAITING (0). 0 ice / 1, 4 rig-4 / 2, 3 bystander / 5 rig-6. default asserts
//   "invalid type" (:543) and reports WAITING.
// ============================================================================
MomentPlayerJumping::EStatus MomentPlayerJumping::GetCameraStatus()
{
    EStatus leStatus = E_STATUS_WAITING;
    switch (meType)
    {
    case E_TYPE_ICE_RIG:
        if (mIceCollection.HasFailed())
            leStatus = E_STATUS_FAILED;
        else if (mIceCollection.CanSwitchToMeNow())
            leStatus = E_STATUS_READY;
        break;
    case E_TYPE_DROPPED_RIG_THEN_ATTACHED_RIG_SEQUENCE:
    case E_TYPE_DROPPED_RIG_SHOT:
        if (mDroppedRigCollection.HasFailed())
            leStatus = E_STATUS_FAILED;
        else if (mDroppedRigCollection.CanSwitchToMeNow())
            leStatus = E_STATUS_READY;
        break;
    case E_TYPE_BYSTANDER_THEN_ATTACHED_RIG_SEQUENCE:
    case E_TYPE_BYSTANDER_SHOT:
        if (mBystanderCollection.HasFailed())
            leStatus = E_STATUS_FAILED;
        else if (mBystanderCollection.CanSwitchToMeNow())
            leStatus = E_STATUS_READY;
        break;
    case E_TYPE_ATTACHED_RIG_SHOT:
        if (mRigCollection.HasFailed())
            leStatus = E_STATUS_FAILED;
        else if (mRigCollection.CanSwitchToMeNow())
            leStatus = E_STATUS_READY;
        break;
    default:
        CGS_ASSERT(false, "invalid type");
        break;
    }
    return leStatus;
}

// ============================================================================
// Update @0x82275988 (DWARF :261)
//
//   0x822759A8  assert "mbPrepared" (:263)
//   0x822759D8  switch (meState): 1 SEARCHING, 2 FOUND_PREPARING, 3 VALID, else assert "unhandled case in switch"
//               (:400) and return
//
//   SEARCHING (0x82275BAC) -- THE RETAIL PATH
//     the player car's time in air (mpPlayerCar +0x50C -> RaceCarState::mfTimeInAir +0x404) is splatted, its sign
//     cleared (vandc with the `vspltisw -1 ; vslw` sign mask) and compared vcmpgtfp against FLT_EPSILON; when lane 0
//     is clear (grounded -- or a NaN, which fails the ordered compare) mfCooldownTimer += mfSimTimestep (+0x520).
//     The gates, in order (0x82275C24..0x82275C4C):
//       GameState::miThisFramesActionFlags & 8   (mpGameState +0x504 -> +0xE4)
//       GameState::mbCanUseSlomo                 (+0x100)
//       mbAllowJumpMoment                        (+0x524) -- FALSE on retail, so the arm always ends at "not met"
//     All three: inhibited (+0x17B) -> can't switch + head bit 23 (0x82275CCC). Else meType = ATTACHED_RIG_SHOT (5),
//       PrepareCameras (asserted, :285), mfJumpTimer = 0, can't switch, meState = FOUND_PREPARING, head bit 19.
//     Not met (0x82275CEC): mbConditionsMet = 0, can't switch, head bit 18.
//
//   FOUND_PREPARING (0x82275A1C)
//     GetCameraStatus: WAITING -> can't switch + head bit 19; READY -> meState = VALID, into its body this frame;
//     FAILED -> Release (vtable +0x10), meState = SEARCHING. Any other value asserts "Unhandled status" (:332), then
//     FAILED / WAITING return and anything else falls into the VALID body.
//
//   VALID (0x82275A68)
//     mfJumpTimer += lfTimeStep; UpdateCamera into a stack camera. Under kfMinTimeInAir: the "Jump_Effect" start hook
//     (blend 1). Past kfMaxJumpTime, on a camera failure, once landed (time in air == 0) or when slow-mo is no
//     longer allowed: Release (vtable +0x10) and meState = SEARCHING -- and the camera tail STILL runs (the console
//     falls through): the border post-FX amount, state bits 7 and 1, then the moment adopts the camera.
// ============================================================================
void MomentPlayerJumping::Update(f32 lfTimeStep, void* lrBehaviourController, const void* lSharedInfo)
{
    Camera::BehaviourManager& lrBehaviourManager = *static_cast<Camera::BehaviourManager*>(lrBehaviourController);
    const MomentSharedInfo&   lrSharedInfo       = *static_cast<const MomentSharedInfo*>(lSharedInfo);

    CGS_ASSERT(mbPrepared, "mbPrepared");   // :263

    switch (GetState())
    {
    case E_STATE_INVALID_SEARCHING:
    {
        if (!(fabsf(lrSharedInfo.mpPlayerCar->mRaceCarState.mfTimeInAir) > KF_GROUNDED_EPSILON))
        {
            mfCooldownTimer = lrSharedInfo.mfSimTimestep + mfCooldownTimer;
        }

        const GameState& lrGameState = *lrSharedInfo.mpGameState;
        JumpMomentTickDiag(lrGameState, lrSharedInfo);   // [DIAG] NOT IN THE console BINARY (BRN_CRASHCAM_DIAG)
        if ((lrGameState.miThisFramesActionFlags & KI_ACTION_FLAG_JUMP) != 0
            && lrGameState.mbCanUseSlomo
            && lrSharedInfo.mbAllowJumpMoment)
        {
            if (IsInhibited())
            {
                SetCanSwitchToMeNow(false);
                GetNonConstCamera().mState.SetHeadFlag(KU_HEAD_FLAG_INHIBITED);
                return;
            }
            meType = E_TYPE_ATTACHED_RIG_SHOT;
            const bool lbPrepared = PrepareCameras(lrBehaviourManager);
            CGS_ASSERT(lbPrepared, "PrepareCameras(lBehaviourManager)");   // :285
            mfJumpTimer = KF_ZERO;
            SetCanSwitchToMeNow(false);
            SetState(E_STATE_INVALID_FOUND_PREPARING);
            GetNonConstCamera().mState.SetHeadFlag(KU_HEAD_FLAG_PREPARING);
            return;
        }
        SetConditionsNotMet();
        SetCanSwitchToMeNow(false);
        GetNonConstCamera().mState.SetHeadFlag(KU_HEAD_FLAG_SEARCHING);
        return;
    }

    case E_STATE_INVALID_FOUND_PREPARING:
    {
        const EStatus leStatus = GetCameraStatus();
        if (leStatus == E_STATUS_WAITING)
        {
            SetCanSwitchToMeNow(false);
            GetNonConstCamera().mState.SetHeadFlag(KU_HEAD_FLAG_PREPARING);
            return;
        }
        if (leStatus == E_STATUS_READY)
        {
            SetState(E_STATE_VALID);
            break;
        }
        if (leStatus == E_STATUS_FAILED)
        {
            Release();
            SetState(E_STATE_INVALID_SEARCHING);
            return;
        }
        CGS_ASSERT(false, "Unhandled status");   // :332
        if (leStatus == E_STATUS_FAILED || leStatus == E_STATUS_WAITING)
        {
            return;
        }
        break;
    }

    case E_STATE_VALID:
        break;

    default:
        CGS_ASSERT(false, "unhandled case in switch");   // :400
        return;
    }

    // ---- the VALID body (0x82275A68) ----
    mfJumpTimer = lfTimeStep + mfJumpTimer;
    Camera::Camera lCamera;
    const bool lbCameraFailed = !UpdateCamera(lCamera);

    if (mfJumpTimer < KF_MIN_TIME_IN_AIR)
    {
        lCamera.GetEffects().SetStartHookName("Jump_Effect", KF_JUMP_EFFECT_BLEND);
    }

    if (mfJumpTimer > KF_MAX_JUMP_TIME
        || lbCameraFailed
        || lrSharedInfo.mpPlayerCar->mRaceCarState.mfTimeInAir == KF_ZERO
        || !lrSharedInfo.mpGameState->mbCanUseSlomo)
    {
        Release();
        SetState(E_STATE_INVALID_SEARCHING);
    }

    lCamera.SetRequestedBorderPostFX(KF_JUMP_BORDER_POSTFX_AMOUNT);
    lCamera.mState.SetFlag(KU_CAMERA_FLAG_7, true);
    lCamera.mState.SetFlag(KU_CAMERA_FLAG_1, true);
    SetCamera(lCamera);
}

// ============================================================================
// UpdateCamera @0x8223AB78 (DWARF :555) -- [PC TRAP, NOT X360]. The console walks the active sequence's
// collections (GetCamera / HasActiveCamera through the per-type accessors, BehaviourIceAnim::GetTimeRemaining, the
// interpolater) into lrCamera. Retail never gets here (VALID needs the mbAllowJumpMoment gate). Reports a camera
// failure, so VALID releases the moment.
// ============================================================================
bool MomentPlayerJumping::UpdateCamera(Camera::Camera& /*lrCamera*/)
{
    static bool sbAnnounced = false;
    PlayerJumpingTrap(sbAnnounced,
                      "[pjump] TRAP (PC, NOT X360): MomentPlayerJumping::UpdateCamera @0x8223AB78 has no host body: "
                      "the jump camera is NOT produced (reported as a camera failure). Reached only with "
                      "MainDirector::mbAllowJumpMoment set, which retail never does.\n",
                      "MomentPlayerJumping::UpdateCamera @0x8223AB78 has no host body (the jump moment's filming closure)");
    return false;
}

// BehaviourCollection<TBehaviour,TParameters,N>::HasActiveCamera (DWARF h:63, NON-const). The console asserts carry
// this .cpp's source path, so the body is defined here (unlike GetCamera, whose asserts carry the .h). One
// instantiation forced below (the N=4 dropped-rig collection).
//
// Console body: read mbAllocatedShots at +0x71 (assert), read mbHasCurrentCamera at +0x70 (assert), then return the
// literal true (the guarded reads prove both latches). Bools at +0x70/+0x71 => maShots size 0x64 => N*0x18+4=0x64
// => N=4.
template <typename TBehaviour, typename TParameters, u32 N>
bool BehaviourCollection<TBehaviour, TParameters, N>::HasActiveCamera()
{
    CGS_ASSERT(mbAllocatedShots, "mbAllocatedShots");
    CGS_ASSERT(mbHasCurrentCamera, "mbHasCurrentCamera");
    return true;
}

// Explicit instantiation forcing the console build's out-of-line copy (dropped-rig collection, N=4).
template bool BehaviourCollection<
    Camera::BehaviourRig,
    Camera::BehaviourRig::Parameters, 4>::HasActiveCamera();

// ---- the vtable one-liners (vftable off_8200A130; AllocateVoid<MomentPlayerJumping> @0x82252DB8 stores it at +0) --
//   slot 5 Destruct         0x8284CB38  `blr` (the one empty body all twelve moments share)
//   slot 3 SetParameters    0x821F7670  `stw r4, 0x180(r3)` -- mpParameters
void MomentPlayerJumping::Destruct()
{
    // 0x8284CB38 is a lone `blr`: nothing to tear down.
}

void MomentPlayerJumping::SetParameters(const Moment::Parameters* lpParameters)
{
    mpParameters = static_cast<const Parameters*>(lpParameters);   // stw r4, 0x180(r3)
}

}
