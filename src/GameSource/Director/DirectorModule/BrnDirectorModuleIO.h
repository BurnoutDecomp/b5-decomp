#pragma once

#include "types.hpp"

#include "GameShared/GameClasses/Module/CgsIOBuffer.h"            // CgsModule::IOBuffer base + read/write lock-state queries
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"  // CgsModule::VariableEventQueue<13312,16> (mGameActionQueue @0x3340)
#include "GameShared/GameClasses/Containers/CgsBitArray.h"        // CgsContainers::BitArray<N> (mUsedRaceCars)
#include "GameSource/BurnoutConstants.h"                          // EActiveRaceCarIndex
#include "GameSource/Director/Camera/SharedIO/BrnPlayerInfo.h"    // BrnDirector::Camera::VehicleInfo (committed, 1264 bytes)
#include "GameSource/Director/Camera/Utils/BrnDebugController.h"
#include "GameShared/GameClasses/System/Timer/CgsTimerStatusInterface.h" // CgsSystem::TimerStatusInterface (mTimerInterface @0x6750, exactly 48B)
#include "GameSource/Director/SharedIO/BrnDirectorVehicleInputInterface.h" // BrnDirector::BrnDirectorVehicleInputInterface (mVehicleDriverInputInterface @0x6780)

// BrnDirector::DirectorIO::InputBuffer -- the Director module's per-frame INPUT payload buffer.
// Like every CgsModule IO buffer it derives the shared CgsModule::IOBuffer (status-flag-guarded
// read/write locking; bit 4 = read lock, bit 3 = write lock) at offset +0, then embeds a large
// aggregate of producer-published state the world/game-state/controller bridges fill each frame
// and the director consumes.
//
// LAYOUT PROVENANCE. The member ORDER + member TYPES are the DecFIGS DWARF for
// GameSource/Director/DirectorModule/BrnDirectorModuleIO.h (struct InputBuffer, decl lines
// 86/324-372). The exact BYTE OFFSETS below are recovered directly from the X360 accessor
// bodies in BURNOUT_X360_ARTIST.XEX (the getters `return this + <off>`; the setters store at
// `this + <off>`):
//
//     mUsedRaceCars              @0x0980 (2432)    GetUsedRaceCars / SetRaceCarInfo bit-set
//     mRaceCarInfo[8]            @0x0990 (2448)    SetRaceCarInfo:  VehicleInfo stride 0x4F0 (1264),
//                                                  SetCrashingCentreOfMass: 1264*idx + 0xDF0/0xE75
//     maVehicleInfoArray[8]      @0x3238 (12856)   GetVehicleInfoArray / SetVehicleTeam (4*idx)
//     mControllerInfo            @0x3260 (12896)   SetControllerInfo memcpy 224 bytes
//     mTimerInterface            @0x6750 (26448)   GetTimerStatusInterface
//     mVehicleDriverInputIface   @0x6780 (26496)   GetVehicleInputInterface
//     mContacts                  @0x6AB8 (27320)   GetContacts / AppendContacts (stores word [6830])
//     mHookEnumeration           @0x7910 (30992)   GetHookEnumeration / SetHookEnumeration memcpy 404
//     mePlayerCarIndex           @0x7AA8 (31400)   GetPlayerCarIndex (lwz, s32)
//     mbHasGotHookEnumeration    @0x7AC1 (31425)   SetHookEnumeration sets =1; HasGotHookEnumeration
//     mbGotCrashNavShownEvent    @0x7AC3 (31427)
//     mbGotCrashNavHiddenEvent   @0x7AC4 (31428)
//     mbGotColourCalibrationShownEvent  @0x7AC5 (31429)
//     mbGotColourCalibrationHiddenEvent @0x7AC6 (31430)
//     mbGotShortcutMenuEvent     @0x7ACD (31437)   SetShortcutMenuEvent sets =1; HasGotShortcutMenuEvent
//     mbShortcutMenuState        @0x7ACE (31438)   SetShortcutMenuEvent stores arg; GetShortcutMenuState
//
// The rest of the scalar/flag tail is attested by the two functions that are the ONLY producer
// and the ONLY consumer of it -- BrnGameModule::BridgeGuiToDirector @0x823CBF70 (which stores
// the literal 1 at each flag as it walks the GUI out-queue) and MainDirector::PostGuiUpdate
// @0x82236F88 (which reads every one of them back). Anchors:
//
//     miDirectorProfileData      @0x7AA4 (31396)   bridge 475 stores the payload word
//     miRankUpNewRank            @0x7AB4 (31412)   bridge 303 `stw r11, 0x7AB4(r31)`
//     miCameraType               @0x7AB8 (31416)   bridge 591; DirectorModule::PostGuiUpdate reads it
//     mbRankUpThisFrame          @0x7ABC (31420)   bridge 303
//     mbStartNewProfileIntro     @0x7ABD (31421)   bridge 476  -> GameState::mbNewProfileIntroActive
//     mbStartGameIntroFlyby      @0x7ABE (31422)   bridge 477  -> GameState::mbGameIntroFlybyActive
//     mbStopGameIntroFlyby       @0x7ABF (31423)   bridge 478  -> clears BOTH of the above
//     mbEndOfCarSelect           @0x7AC2 (31426)   bridge 192
//     mbSimPaused                @0x7AC8 (31432)   MainDirector::UpdateArbitrator's lbPaused
//     mbHasNewDirectorProfileData@0x7AC9 (31433)   bridge 475
//     mbCarSelectionChanged...   @0x7ACB (31435)   bridge 415
//     mbCarSelectTickerClosed... @0x7ACC (31436)   bridge 77
//     mbLeftOnlinePostEvent      @0x7ACF (31439)   bridge 294
//     mbEnteredOnlinePostEvent   @0x7AD0 (31440)   bridge 290
//     mbFinishedOnlineEventLoad  @0x7AD1 (31441)   bridge 480
//     mbStartedOnlineEventLoad   @0x7AD2 (31442)   bridge 479
//     mbStarting100PercentSeq    @0x7AD3 (31443)   bridge 469/470 with flag != 0
//     mbFinished100PercentSeq    @0x7AD4 (31444)   bridge 469/470 with flag == 0
//
// The three flags with no store site of their own (mbPlayerTakenDown @0x7AC0,
// mbWorldWantsDebugControllerFocus @0x7AC7, mbPlayerCrashbreakerFired @0x7ACA) sit at the
// offsets the DWARF member order puts them at BETWEEN two attested neighbours, so their
// placement is forced rather than guessed.
//
// VehicleInfo's own internal offsets are independently confirmed by SetCrashingCentreOfMass:
// element base this+1264*idx, mCrashingCentreOfMass at element+0x460 (BrnPlayerInfo.h), the
// mbHasCrashingCenterOfMass flag at element+0x4E5 -- exactly the committed VehicleInfo layout.
//
// HONEST PLACEHOLDERS. Several embedded members are large interface aggregates whose full byte
// layouts are not yet reconstructed (RCEntityGlobalRaceCarOutputInterface, ControllerInfo,
// TimerStatusInterface, BrnDirectorVehicleInputInterface, the world StatusInterface, the
// ContactSpyInterface, TrafficDirectorOutputInterface, PlayerCrashInfo, GuiPFXHookEnumeration,
// CarScoreData, DirectorProfileData, and the GameActionQueue). Rather than fork those homes
// with guessed members, they are modelled here as correctly-SIZED, byte-addressable opaque
// storage members carrying their DWARF names and offsets. This preserves the exact object
// layout the accessors index into (every recovered offset is asserted below) while being honest
// that the interiors are not yet known. Grow each into its real type, additively, when its home
// is reconstructed. The members whose types ARE committed (the IOBuffer base, BitArray<8u>,
// VehicleInfo[8], EActiveRaceCarIndex, and the trailing bool flags) are modelled by their real
// types/names.

namespace BrnDirector
{
namespace DirectorIO
{
    struct ControlInput
    {
        // ---- ADDITIVE (MainDirector::UpdateArbitrator @0x82271120 / Arbitrator::Update
        //      @0x8226ADA0) --------------------------------------------------------------
        // Camera-control bytes the director reads out of this block. X360-attested as byte
        // loads at control-block +1 / +2 / +3 / +4:
        //   * UpdateArbitrator passes controller[3] as Arbitrator::Update's lbCycleCamera and
        //     controller[2] as its lbCycleCameraHeld (the "tap to cycle / hold for slow-mo"
        //     pair UpdateCameraCycleControl consumes);
        //   * Arbitrator::Update's NORMAL case stores controller[4] straight into
        //     SharedCameraContainer::mbLookbackOverride;
        //   * MainDirector::UpdateAttribSys @0x8221AFD0 gates its ENTIRE body on controller[1].
        //
        // ⭐⭐ THE NAMES ARE NO LONGER INFERRED (2026-08-02). The DecFIGS DWARF has this whole
        // block, at GameSource/Director/SharedIO/BrnDirectorControllerInfo.h:42 --
        // `struct BrnDirector::DirectorIO::ControllerInfo`, a run of bools in declaration
        // order starting at offset 0:
        //     :48 mbAnyInput               +0x00
        //     :49 mbGameTalkRefreshRequest +0x01
        //     :50 mbCameraButtonHeldDown   +0x02
        //     :51 mbCycleCameras           +0x03
        //     :52 mbLookback               +0x04
        //     :53 mbRequestSloMo           +0x05
        //     :54 mbTakeScreenshot         +0x06
        //     :55 mbTempBoredOfCamera      +0x07
        //     :56 mbTempBoosting           +0x08
        //     :57 mbHandbrake              +0x09
        //     :58 Vector2 mCarModifier / :59 Vector2 mCameraModifier / :61 DebugController
        // The three previously-inferred names land EXACTLY on the DWARF's +2/+3/+4 roles,
        // which is what makes this a retrofit rather than a rewrite; the accessors keep their
        // existing spellings (callers depend on them) with the DWARF member named alongside.
        // FLAG: the block interior below is still modelled as maPad -- only the byte offsets
        //   and these roles are pinned. Retype to the DWARF struct when its own home lands.
        bool IsCycleCameraPressed() const { return maPad[3] != 0; }   // +0x03 mbCycleCameras
        bool IsCycleCameraHeld() const    { return maPad[2] != 0; }   // +0x02 mbCameraButtonHeldDown
        bool IsLookbackHeld() const       { return maPad[4] != 0; }   // +0x04 mbLookback

        // ⭐⭐ +0x01 mbGameTalkRefreshRequest -- the LIVE-TUNING refresh request. GameTalk is
        // the EA authoring tool that edits AttribSys values on a running build; this byte is
        // its "re-read the attribs" pulse. It is the ONLY gate on
        // MainDirector::UpdateAttribSys, which is why that function is NOT a per-frame
        // re-seed: on a build with no GameTalk connection it never runs its body at all.
        // ⇒ MainDirector::ProcessNewVehicleEvents is the ONLY thing that seeds the two
        // gameplay cameras' Parameters on this build. (Corrects the "per-frame re-seed"
        // reading that had been carried in the camera chain map.)
        bool IsGameTalkRefreshRequested() const { return maPad[1] != 0; }   // +0x01

        u8 maPad[48];
        BrnDirector::Camera::Utils::DebugController mDebugController;
    };

    struct InputBuffer : public CgsModule::IOBuffer
    {
        // X360 @0x822393D0 -- the buffer's OWN Construct, which the console's
        // CreateIOBuffer<InputBuffer> runs after the stack allocation -- and so does the PC
        // template: CreateIOBuffer<T> runs T::Construct (2026-08-15). Raises the IOBuffer base
        // status and seeds the published scalars.
        // ⚠️ IT IS NOT A ZERO-FILL: it seeds mePlayerCarIndex @0x7AA8 AND miCameraType @0x7AB8
        // to -1 (`*(a1 + 31400) = -1; *(a1 + 31416) = -1;`). Both are "none" sentinels that
        // consumers test with `> -1`, so a plain memset publishes a live request every frame.
        // Only the SEEDS whose members are named here are reproduced; the aggregate sub-object
        // Constructs the X360 also runs (the contact spy, the vehicle-event queue, the score
        // data, ...) belong to their own un-homed homes and stay with them.
        void Construct();

        // --- queries (getters): all const, all read-lock-asserted ---
        // (GetContacts/GetHookEnumeration/GetControllerInfo return the addresses of
        //  opaque-but-correctly-sized members, typed as void* until those interface homes are
        //  reconstructed. GetVehicleInputInterface is TYPED as of 2026-08-02 -- see the member.)
        const void*                                        GetContacts() const;
        const CgsContainers::BitArray<8u>*                 GetUsedRaceCars() const;
        // Address of the X360 VehicleInfo* pointer table (stored as pointer-width u32 slots).
        const u32*                                         GetVehicleInfoArray() const;
        const void*                                        GetControllerInfo() const;
        const CgsSystem::TimerStatusInterface*             GetTimerStatusInterface() const;
        // X360 @0x823B27E8 -- the WRITE-side overload BrnGameModule::BridgeTimers uses to
        // copy the game module's 48-byte snapshot in under the buffer's write lock.
        CgsSystem::TimerStatusInterface*                   GetTimerStatusInterface();
        const BrnDirector::BrnDirectorVehicleInputInterface* GetVehicleInputInterface() const;
        BrnDirector::BrnDirectorVehicleInputInterface*       GetVehicleInputInterface();
        const void*                                        GetHookEnumeration() const;
        EActiveRaceCarIndex                                GetPlayerCarIndex() const;
        bool                                               GetShortcutMenuState() const;
        const ControlInput*                                GetControll() const;
        ControlInput*                                      GetControll();

        // ---- ADDITIVE (MainDirector::UpdateArbitrator @0x82271120) ------------------------
        // The base of the published per-active-car VehicleInfo array (@0x0990). The X360
        // reaches it through the de-inlined accessor sub_82207040 and then indexes it as
        // `1264 * playerCarIndex + base` to fill ArbStateSharedInfo::mpRaceCars /
        // ::mpPlayerCar. Named here so no caller re-derives the stride.
        const BrnDirector::Camera::VehicleInfo*            GetRaceCarInfo() const;

        // The player's crash-analysis record (ArbStateSharedInfo::mpPlayerCrashInfo; the X360
        // fills that slot with `lpInputBuffer + 30944` == @0x78E0). It lands INSIDE the
        // honest-opaque contacts span, so the address is taken off that named member.
        // FLAG: BrnDirector::PlayerCrashInfo has no reconstructed home -- returned as void*,
        // and the DWARF slot name is the only evidence for the role.
        const void*                                        GetPlayerCrashInfo() const;

        // The simulation-paused flag (@0x7AC8, the second byte of the mid-flag block).
        // MainDirector::UpdateArbitrator passes it to Arbitrator::Update as lbPaused, and
        // MainDirector::Update's gameplay middle tests it too.
        bool                                               IsSimPaused() const;

        // ⭐ X360 @0x82206C50 (`addi r3, r28, 0x3340`, guarded by the "Not locked for reading"
        // assert at BrnDirectorModuleIO.h:583). THE GAME-ACTION QUEUE -- the per-frame stream
        // of BrnGameState game actions the director drains in
        // MainDirector::ProcessInputQueue @0x822372F8. Its producer is
        // BrnGameModule::BridgeGameStateToDirector @0x823CD170, which bulk-Appends the
        // GameStateModule's own <13312,16> queue into this one once per frame.
        const CgsModule::VariableEventQueue<13312, 16>*    GetGameActionQueue() const;
        CgsModule::VariableEventQueue<13312, 16>*          GetGameActionQueue();

        // @0x7AC0 / @0x7AAC -- the takedown pair BridgeGameStateToDirector @0x823CD170
        // publishes together (`*(input + 31424) = 1; *(input + 31404) = killerIndex;`) and
        // MainDirector::ProcessInputQueue's prologue reads back in the same order (it asserts
        // "mbPlayerTakenDown" at BrnMainDirector.cpp:232 before taking the killer index).
        bool                                               GetPlayerTakenDown() const;
        EActiveRaceCarIndex                                GetPlayerKillerCarIndex() const;

        // @0x7AB0 -- copied verbatim into GameState +0x1CC (mRankUpInfo's 4-byte head) by
        // ProcessInputQueue's prologue (`lwz r11, 0x7AB0(r30); stwx r11, r31, 0x339AC`).
        // InputBuffer::Construct seeds it to 0 (`stw r30, 0x7AB0`).
        s32                                                GetRankUpRivalInfo() const;

        bool HasGotHookEnumeration() const;
        bool HasGotShortcutMenuEvent() const;
        bool HasGotCrashNavShownEvent() const;
        bool HasGotCrashNavHiddenEvent() const;
        bool HasGotColourCalibrationShownEvent() const;
        bool HasGotColourCalibrationHiddenEvent() const;

        // ---- the GUI->director command flags BrnGameModule::BridgeGuiToDirector @0x823CBF70
        //      raises and MainDirector::PostGuiUpdate @0x82236F88 consumes -------------------
        // Every one of these is a 1-byte published flag at an X360-attested offset (pinned in
        // _AssertLayout()); the NAMES are the DecFIGS DWARF's for this header (decl lines
        // 346-372), matched to the offsets by walking the DWARF member order back from
        // mbHasGotHookEnumeration @0x7AC1 -- which lands mbRankUpThisFrame on @0x7ABC, the
        // exact byte PostGuiUpdate pairs with the @0x7AB4 rank word, so the run is anchored at
        // both ends.
        bool GetStartNewProfileIntro() const;    // @0x7ABD  (GUI command 476)
        bool GetStartGameIntroFlyby() const;     // @0x7ABE  (GUI command 477)
        bool GetStopGameIntroFlyby() const;      // @0x7ABF  (GUI command 478)
        bool GetRankUpThisFrame() const;         // @0x7ABC  (GUI command 303)
        s32  GetRankUpNewRank() const;           // @0x7AB4  (GUI command 303 payload)
        s32  GetCameraType() const;              // @0x7AB8  (GUI command 591; -1 == none)
        bool GetEndOfCarSelect() const;          // @0x7AC2  (GUI command 192)
        bool HasNewDirectorProfileData() const;  // @0x7AC9  (GUI command 475)
        s32  GetDirectorProfileData() const;     // @0x7AA4  (GUI command 475 payload)
        bool GetCarSelectionChangedThisFrame() const;      // @0x7ACB  (GUI command 415)
        bool GetCarSelectTickerClosedThisFrame() const;    // @0x7ACC  (GUI command 77)
        bool GetLeftOnlinePostEvent() const;               // @0x7ACF  (GUI command 294)
        bool GetEnteredOnlinePostEvent() const;            // @0x7AD0  (GUI command 290)
        bool GetFinishedOnlineEventLoading() const;        // @0x7AD1  (GUI command 480)
        bool GetStartedOnlineEventLoading() const;         // @0x7AD2  (GUI command 479)
        bool GetStarting100PercentSequence() const;        // @0x7AD3  (GUI command 469/470, flag != 0)
        bool GetFinished100PercentSequence() const;        // @0x7AD4  (GUI command 469/470, flag == 0)

        // --- mutators: all write-lock-asserted ---
        void AppendContacts(const void* lpContacts);
        void SetControllerInfo(const void* lpControllerInfo);
        void SetHookEnumeration(const void* lpHookEnumeration);
        void SetRaceCarInfo(u32 luIndex, const BrnDirector::Camera::VehicleInfo& lrInfo);
        void SetCrashingCentreOfMass(u32 luIndex, const Matrix44Affine& lrCentreOfMass);
        void SetVehicleTeam(EActiveRaceCarIndex leIndex, s32 liTeam);

        // ADDITIVE (write-lock-asserted): the two published fields that decide whether the
        // director sees a LIVE PLAYER CAR. The X360 fills them from the race-car entity
        // module's global output interface during the per-frame input staging (which is not
        // threaded on the PC yet); MainDirector::GetLivePlayerCarIndex reads exactly this pair,
        // and its answer is what gates the ENTIRE gameplay/arbitrator middle of
        // MainDirector::Update. Named setters so no caller pokes mUsedRaceCars / mePlayerCarIndex
        // by offset. FLAG: additive accessors -- the fields and their roles are the console's;
        // only the entry points are ours.
        void SetPlayerCarIndex(EActiveRaceCarIndex leIndex);
        void SetRaceCarInUse(u32 luIndex, bool lbInUse);

        // @0x325C -- the player's boost fill fraction, published by
        // BrnGameModule::BridgeWorldToDirector (the console stores it inline; there is no
        // console setter symbol, so this entry point is ours and the field is the console's).
        void SetPlayerBoostPercentage(f32 lfPercentage);
        f32  GetPlayerBoostPercentage() const;

        void SetShortcutMenuEvent(bool lbState);
        void SetGotCrashNavShownEvent();
        void SetGotCrashNavHiddenEvent();
        void SetGotColourCalibrationShownEvent();
        void SetGotColourCalibrationHiddenEvent();

        // The BridgeGuiToDirector setters (one per GUI command arm; the X360 stores the
        // literal 1 / the payload word inline, so these carry no extra behaviour).
        void SetStartNewProfileIntro();          // 476
        void SetStartGameIntroFlyby();           // 477
        void SetStopGameIntroFlyby();            // 478
        void SetRankUp(s32 liNewRank);           // 303
        void SetCameraType(s32 liCameraType);    // 591
        void SetEndOfCarSelect();                // 192
        void SetDirectorProfileData(s32 liData); // 475
        void SetCarSelectionChangedThisFrame();  // 415
        void SetCarSelectTickerClosedThisFrame();// 77
        void SetLeftOnlinePostEvent();           // 294
        void SetEnteredOnlinePostEvent();        // 290
        void SetFinishedOnlineEventLoading();    // 480
        void SetStartedOnlineEventLoading();     // 479
        void SetStarting100PercentSequence();    // 469/470 (flag != 0)
        void SetFinished100PercentSequence();    // 469/470 (flag == 0)

    private:
        // Recovered byte offsets are pinned by _AssertLayout() below.

        // @0x0001 .. : the IOBuffer base is 1 byte (FlagSet8). The first published member,
        // mGlobalRaceCarInterface (RCEntityGlobalRaceCarOutputInterface), spans from just past the
        // base up to mUsedRaceCars @0x0980. HONEST opaque storage (type home not yet reconstructed).
        u8  mGlobalRaceCarInterface[0x0980 - 0x0001];   // RCEntityGlobalRaceCarOutputInterface @ ~0x0001

        // @0x0980 (2432): the active-race-car bitmask. BitArray<8u> is one u64 field (8 bytes);
        // VehicleInfo is alignas(16) so the array slot is 16-byte aligned -> 8 bytes trailing pad.
        CgsContainers::BitArray<8u> mUsedRaceCars;       // @0x0980
        u8  mUsedRaceCarsPad[0x0990 - 0x0980 - sizeof(CgsContainers::BitArray<8u>)];

        // @0x0990 (2448): per-active-car published vehicle state. Stride 0x4F0 (1264) confirmed.
        BrnDirector::Camera::VehicleInfo mRaceCarInfo[8]; // @0x0990 .. 0x3110

        // @0x3110 .. : mPlayerScoreData (CarScoreData) + mbPlayerComboWarningActive +
        // mfPlayerBoostPercentage, ending at the vehicle-info pointer array @0x3238. HONEST opaque.
        u8  mScoreAndBoostBlock[0x3238 - (0x0990 + 8 * 1264)]; // CarScoreData + combo flag + boost f32

        // @0x3238 (12856): the parallel array of VehicleInfo pointers GetVehicleInfoArray() returns;
        // SetVehicleTeam writes the team id (a VehicleInfo*-typed slot, stored as s32) per index.
        // NOTE: the X360 is a 32-bit target (4-byte pointers), but the compile gate builds for the
        // 64-bit host. To preserve the exact 32-byte X360 layout (4 bytes/slot) the pointer table is
        // stored as 8 X360-pointer-width u32 slots; the accessors reinterpret them as needed.
        u32 maVehicleInfoArray[8];                       // @0x3238 (32 bytes -> 0x3258)
        u8  maVehicleInfoArrayPad[0x325C - (0x3238 + 8 * sizeof(u32))]; // 4-byte gap
        // @0x325C (12892): the player's boost fill fraction. The producer is
        // BrnGameModule::BridgeWorldToDirector @0x823E3AB0 -- `*(input + 0x325C) =
        // (fabs(boost.mfMaxBoost) > eps) ? boost.mfBoostAmount / boost.mfMaxBoost : 0`.
        // ⚠️ FINDING (2026-08-01): the file header above places mfPlayerBoostPercentage in
        // the mScoreAndBoostBlock span BEFORE maVehicleInfoArray. The only writer proves it
        // is AFTER it, in what was modelled as an 8-byte pad. Named here without moving any
        // offset (_AssertLayout still pins 0x3238 / 0x3260); the header prose is corrected
        // rather than the layout.
        f32 mfPlayerBoostPercentage;                     // @0x325C

        // @0x3260 (12896): controller snapshot, 224 (0xE0) bytes (SetControllerInfo memcpy). HONEST.
        u8  mControllerInfo[224];                        // @0x3260 .. 0x3340

        // ⭐ @0x3340 (13120): THE GAME-ACTION QUEUE. RETYPED (2026-08-01) from an opaque
        // `u8 mMidInterfaceBlock[0x6750 - 0x3340]` labelled "StatusInterface / driver-input /
        // score / etc." -- that prose was wrong and the span is ONE object:
        //   * GetGameActionQueue @0x82206C50 returns `this + 0x3340`;
        //   * InputBuffer::Construct @0x82239440-0x82239448 runs
        //     `VariableEventQueue<13312,16>::Construct(this + 0x3340)`;
        //   * the span is 0x6750 - 0x3340 == 0x3410 == 13328 bytes, and
        //     sizeof(VariableEventQueue<13312,16>) is EXACTLY 13328 on the host
        //     (1 + 13312 + 3 pad + 3*4). The fit is exact at both ends, so nothing else
        //     can live in there.
        // The offsets did not move; only the type and the prose did (_AssertLayout still
        // pins both 0x3340 and the following 0x6750).
        CgsModule::VariableEventQueue<13312, 16> mGameActionQueue;   // @0x3340

        // @0x6750 (26448): the published game/sim timer snapshot. RETYPED from an opaque
        // u8[0x30] to the real CgsSystem::TimerStatusInterface -- the span was already exactly
        // sizeof(TimerStatusInterface) (48 == two 24-byte TimerStatus blocks), the type is
        // 4-aligned like the offset, and _AssertLayout() still pins 0x6750. The producer is
        // BrnGameModule::BridgeTimers @0x823BD150, which copies all 48 bytes of the game
        // module's own interface (gm+10095372) in here; the consumer is
        // MainDirector::UpdateCameraBehavioursPostScene, which reads the frame timesteps out
        // of it. Naming it is what lets both sides stop casting.
        CgsSystem::TimerStatusInterface mTimerInterface;  // @0x6750

        // @0x6780 (26496): the vehicle input interface (Get/SetVehicleInputInterface).
        // ⭐ RETYPED 2026-08-02 (camera parameter-chain wave) from an opaque u8[0x338] to the
        // real BrnDirector::BrnDirectorVehicleInputInterface. The type was ALREADY homed
        // (SharedIO/BrnDirectorVehicleInputInterface.h -- an EventQueue<NewVehicleEvent,50>);
        // it was only the DESTINATION that stayed opaque, so MainDirector::
        // ProcessNewVehicleEvents had nothing typed to read. X360-attested:
        // InputBuffer::GetVehicleInputInterface @0x82206DA0 returns `this + 0x6780` and
        // ProcessNewVehicleEvents @0x8221A6B0 immediately does `lwz r11, 8(r3)` on the result
        // -- BaseEventQueue::miLength -- then GetEvent(i) at a 16-byte stride, i.e. the
        // interface's FIRST member is the queue, which is exactly this type's only member.
        //
        // The trailing pad keeps the span exactly 0x338 so every _AssertLayout pin after it
        // (mContacts @0x6AB8 onward) still holds. On x64 the interface is 816 bytes
        // (16-byte queue head + 50 * 16 events) against the console's 824-byte span, so the
        // pad is 8 bytes; it is the un-modelled remainder of the console's own span, NOT
        // padding we invented for alignment.
        BrnDirector::BrnDirectorVehicleInputInterface mVehicleDriverInputInterface;   // @0x6780
        u8  maVehicleDriverInputInterfacePad[
                (0x6AB8 - 0x6780) - sizeof(BrnDirector::BrnDirectorVehicleInputInterface)];

        // @0x6AB8 (27320): contacts. AppendContacts stores one 32-bit word at member word [0]
        // (this[6830] == 0x6AB8). GetContacts returns its address. HONEST opaque storage spanning
        // to the hook-enumeration block @0x7910.
        u8  mContacts[0x7910 - 0x6AB8];                  // @0x6AB8

        // @0x7910 (30992): the GUI PFX hook enumeration, 404 (0x194) bytes (SetHookEnumeration
        // memcpy). HONEST opaque, padded out to the scalar/flag tail @0x7AA8.
        u8  mHookEnumeration[404];                       // @0x7910 .. 0x7AA4

        // @0x7AA4 (31396): the DWARF's mDirectorProfileData, which on this build is exactly the
        // four bytes between the hook enumeration and the car index. BridgeGuiToDirector's
        // command-475 arm stores the event's payload word here, and MainDirector::PostGuiUpdate
        // reads the same word back as a 32-bit value (and compares it against 1). Modelled as
        // the single s32 both sides address; the aggregate's interior is not otherwise known.
        s32 miDirectorProfileData;                       // @0x7AA4

        // @0x7AA8 (31400): the active player car index (GetPlayerCarIndex, read as a 32-bit word).
        EActiveRaceCarIndex mePlayerCarIndex;            // @0x7AA8
        // @0x7AAC (31404): the DWARF's mePlayerKillerCarIndex -- next in member order, same
        // 4-byte enum width. Not addressed by any recovered body; named, not opaque, so the
        // run down to the flag block is continuous.
        EActiveRaceCarIndex mePlayerKillerCarIndex;      // @0x7AAC
        // @0x7AB0 (31408): NAMED 2026-08-01 (it was `mUnknownScalar7AB0`, "no recovered body
        // touches it" -- that was a NAME search failing, not an absent writer).
        // MainDirector::ProcessInputQueue's prologue copies this word straight into
        // GameState +0x1CC, the 4-byte head of GameState::mRankUpInfo
        // (`0x8223740C lwz r11, 0x7AB0(r30)` -> `0x82237428 stwx r11, r31, 0x339AC`), and
        // InputBuffer::Construct @0x82239520 seeds it to 0 (`stw r30, 0x7AB0`).
        // FLAG: the ROLE (a rank-up / rival-team selector) is inferred from its destination,
        //   whose own DWARF layout the GameState header records as unreliable; the offset,
        //   the width and the copy are asm.
        s32 miRankUpRivalInfo;                           // @0x7AB0

        // @0x7AB4 (31412): the new rank BridgeGuiToDirector's command-303 arm publishes
        // alongside mbRankUpThisFrame (X360 `stw r11, 0x7AB4(r31)`), read straight back by
        // MainDirector::PostGuiUpdate.
        s32 miRankUpNewRank;                             // @0x7AB4
        // @0x7AB8 (31416): the requested camera type (command 591: 0 or 1, anything else fires
        // "Unhandled camera type"). A 32-bit word -- DirectorModule::PostGuiUpdate @0x82250DD0
        // reads it as `lwz r11, 0x7AB8(r30)` and forwards it when it is > -1, so the buffer's
        // cleared state (0) is a REQUEST and -1 would be "none". No DWARF name for this build;
        // named for the role its own assert text states.
        s32 miCameraType;                                // @0x7AB8

        // @0x7ABC (31420): the trailing 1-byte flag run. The order is the DecFIGS DWARF's;
        // the offsets are the X360 store/load sites, pinned in _AssertLayout().
        bool mbRankUpThisFrame;                          // @0x7ABC
        bool mbStartNewProfileIntro;                     // @0x7ABD
        bool mbStartGameIntroFlyby;                      // @0x7ABE
        bool mbStopGameIntroFlyby;                       // @0x7ABF
        bool mbPlayerTakenDown;                          // @0x7AC0 (DWARF order; not addressed here)
        bool mbHasGotHookEnumeration;                    // @0x7AC1
        bool mbEndOfCarSelect;                           // @0x7AC2
        bool mbGotCrashNavShownEvent;                    // @0x7AC3
        bool mbGotCrashNavHiddenEvent;                   // @0x7AC4
        bool mbGotColourCalibrationShownEvent;           // @0x7AC5
        bool mbGotColourCalibrationHiddenEvent;          // @0x7AC6
        bool mbWorldWantsDebugControllerFocus;           // @0x7AC7 (DWARF order; not addressed here)
        bool mbSimPaused;                                // @0x7AC8
        bool mbHasNewDirectorProfileData;                // @0x7AC9
        bool mbPlayerCrashbreakerFired;                  // @0x7ACA (DWARF order; not addressed here)
        bool mbCarSelectionChangedThisFrame;             // @0x7ACB
        bool mbCarSelectTickerClosedThisFrame;           // @0x7ACC
        bool mbGotShortcutMenuEvent;                     // @0x7ACD
        bool mbShortcutMenuState;                        // @0x7ACE
        bool mbLeftOnlinePostEvent;                      // @0x7ACF
        bool mbEnteredOnlinePostEvent;                   // @0x7AD0
        bool mbFinishedOnlineEventLoading;               // @0x7AD1
        bool mbStartedOnlineEventLoading;                // @0x7AD2
        bool mbStarting100PercentSequence;               // @0x7AD3
        bool mbFinished100PercentSequence;               // @0x7AD4
        // @0x7AD5 / @0x7AD6: TWO more flag bytes, not one (CORRECTED 2026-08-01).
        // InputBuffer::Construct @0x82239514/0x82239518 seeds BOTH to 0, and
        // MainDirector::ProcessInputQueue's prologue copies BOTH into the GameState
        // (`lbz 0x7AD6(r30)` -> GameState +0x1D1, `lbz 0x7AD5(r30)` -> GameState +0x1D0 --
        // the two RankUpInfo tail bytes). The DWARF member list for this header ends at
        // mbFinished100PercentSequence, so neither has a name; HONEST opaque tail.
        u8  maFlagTail[2];                               // @0x7AD5 .. @0x7AD6

        // Compile-time pin of every recovered offset (private members -> assert from a member fn).
        static void _AssertLayout();
    };
}
}
