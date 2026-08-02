#include "GameSource/Director/DirectorModule/BrnDirectorModuleIO.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

#include <cstddef>   // offsetof
#include <cstring>   // std::memcpy

// BrnDirector::DirectorIO::InputBuffer member functions, reconstructed from
// BURNOUT_X360_ARTIST.XEX. This TU bodies the 27 X360-emitted InputBuffer functions:
//
//   getters @ 0x82206CF8 .. 0x82207628  (return &member; assert IsBufferLockedForReading)
//   mutators @ 0x823B2890 .. 0x823B3248 (write member; assert IsBufferLockedForWriting)
//
// Every accessor first checks the IOBuffer lock-state flag and asserts on violation, exactly as
// the X360 bodies do (the original streams the file/line via CgsDev::Assert; CGS_ASSERT carries
// the stringized condition + __FILE__/__LINE__). The bodies then read/write the member the
// X360 addresses by raw offset -- here named, with the offsets pinned in _AssertLayout().
//
// The "value" payloads of the opaque interface members are not interpreted: AppendContacts /
// SetControllerInfo / SetHookEnumeration copy the producer's bytes into the correctly-sized
// member storage exactly as the X360 memcpy/word-store bodies do.

namespace BrnDirector
{
namespace DirectorIO
{
    // ---- byte-offset pins (X360-recovered) --------------------------------------------------
    void InputBuffer::_AssertLayout()
    {
        static_assert(offsetof(InputBuffer, mUsedRaceCars)        == 0x0980, "mUsedRaceCars @0x0980");
        static_assert(offsetof(InputBuffer, mRaceCarInfo)         == 0x0990, "mRaceCarInfo @0x0990");
        static_assert(sizeof(BrnDirector::Camera::VehicleInfo)    == 1264,   "VehicleInfo stride 0x4F0");
        static_assert(offsetof(InputBuffer, maVehicleInfoArray)   == 0x3238, "maVehicleInfoArray @0x3238");
        static_assert(offsetof(InputBuffer, mControllerInfo)      == 0x3260, "mControllerInfo @0x3260");
        static_assert(offsetof(InputBuffer, mGameActionQueue)     == 0x3340, "mGameActionQueue @0x3340");
        static_assert(sizeof(ControlInput) <= 224,
                      "ControlInput is the view over the 224-byte controller snapshot @0x3260");
        static_assert(sizeof(CgsModule::VariableEventQueue<13312, 16>) == 0x6750 - 0x3340,
                      "the game-action queue fills the @0x3340 span exactly (0x3410 bytes)");
        static_assert(offsetof(InputBuffer, mTimerInterface)      == 0x6750, "mTimerInterface @0x6750");
        static_assert(sizeof(CgsSystem::TimerStatusInterface)     == 0x30,   "TimerStatusInterface fills the @0x6750 span exactly");
        static_assert(offsetof(InputBuffer, mVehicleDriverInputInterface) == 0x6780, "mVehicleDriverInputInterface @0x6780");
        static_assert(offsetof(InputBuffer, mContacts)            == 0x6AB8, "mContacts @0x6AB8");
        static_assert(offsetof(InputBuffer, mHookEnumeration)     == 0x7910, "mHookEnumeration @0x7910");
        static_assert(offsetof(InputBuffer, miDirectorProfileData) == 0x7AA4, "miDirectorProfileData @0x7AA4");
        static_assert(offsetof(InputBuffer, mePlayerCarIndex)     == 0x7AA8, "mePlayerCarIndex @0x7AA8");
        static_assert(offsetof(InputBuffer, miRankUpNewRank)      == 0x7AB4, "miRankUpNewRank @0x7AB4");
        static_assert(offsetof(InputBuffer, miCameraType)         == 0x7AB8, "miCameraType @0x7AB8");
        static_assert(offsetof(InputBuffer, mbRankUpThisFrame)      == 0x7ABC, "mbRankUpThisFrame @0x7ABC");
        static_assert(offsetof(InputBuffer, mbStartNewProfileIntro) == 0x7ABD, "mbStartNewProfileIntro @0x7ABD");
        static_assert(offsetof(InputBuffer, mbStartGameIntroFlyby)  == 0x7ABE, "mbStartGameIntroFlyby @0x7ABE");
        static_assert(offsetof(InputBuffer, mbStopGameIntroFlyby)   == 0x7ABF, "mbStopGameIntroFlyby @0x7ABF");
        static_assert(offsetof(InputBuffer, mbHasGotHookEnumeration)           == 0x7AC1, "mbHasGotHookEnumeration @0x7AC1");
        static_assert(offsetof(InputBuffer, mbEndOfCarSelect)                  == 0x7AC2, "mbEndOfCarSelect @0x7AC2");
        static_assert(offsetof(InputBuffer, mbGotCrashNavShownEvent)           == 0x7AC3, "mbGotCrashNavShownEvent @0x7AC3");
        static_assert(offsetof(InputBuffer, mbGotCrashNavHiddenEvent)          == 0x7AC4, "mbGotCrashNavHiddenEvent @0x7AC4");
        static_assert(offsetof(InputBuffer, mbGotColourCalibrationShownEvent)  == 0x7AC5, "mbGotColourCalibrationShownEvent @0x7AC5");
        static_assert(offsetof(InputBuffer, mbGotColourCalibrationHiddenEvent) == 0x7AC6, "mbGotColourCalibrationHiddenEvent @0x7AC6");
        static_assert(offsetof(InputBuffer, mbSimPaused)                       == 0x7AC8, "mbSimPaused @0x7AC8");
        static_assert(offsetof(InputBuffer, mbHasNewDirectorProfileData)       == 0x7AC9, "mbHasNewDirectorProfileData @0x7AC9");
        static_assert(offsetof(InputBuffer, mbCarSelectionChangedThisFrame)    == 0x7ACB, "mbCarSelectionChangedThisFrame @0x7ACB");
        static_assert(offsetof(InputBuffer, mbCarSelectTickerClosedThisFrame)  == 0x7ACC, "mbCarSelectTickerClosedThisFrame @0x7ACC");
        static_assert(offsetof(InputBuffer, mbGotShortcutMenuEvent)            == 0x7ACD, "mbGotShortcutMenuEvent @0x7ACD");
        static_assert(offsetof(InputBuffer, mbShortcutMenuState)              == 0x7ACE, "mbShortcutMenuState @0x7ACE");
        static_assert(offsetof(InputBuffer, mbLeftOnlinePostEvent)             == 0x7ACF, "mbLeftOnlinePostEvent @0x7ACF");
        static_assert(offsetof(InputBuffer, mbEnteredOnlinePostEvent)          == 0x7AD0, "mbEnteredOnlinePostEvent @0x7AD0");
        static_assert(offsetof(InputBuffer, mbFinishedOnlineEventLoading)      == 0x7AD1, "mbFinishedOnlineEventLoading @0x7AD1");
        static_assert(offsetof(InputBuffer, mbStartedOnlineEventLoading)       == 0x7AD2, "mbStartedOnlineEventLoading @0x7AD2");
        static_assert(offsetof(InputBuffer, mbStarting100PercentSequence)      == 0x7AD3, "mbStarting100PercentSequence @0x7AD3");
        static_assert(offsetof(InputBuffer, mbFinished100PercentSequence)      == 0x7AD4, "mbFinished100PercentSequence @0x7AD4");
    }

    // ---- Construct @0x822393D0 --------------------------------------------------------------
    // The scalar/flag seeds of the console body, for the members this class names. The X360
    // additionally runs the embedded aggregates' own Constructs (ContactSpyInterface @+27320,
    // the NewVehicleEvent queue @+26496, TimerStatusInterface::Clear @+26448, CarScoreData::
    // ClearData @+12560, the vehicle-info pointer table clear, ...); each of those belongs to
    // its own home and stays there, so this seeds what it owns and leaves the honest-opaque
    // spans to the caller's zero-fill.
    void InputBuffer::Construct()
    {
        CgsModule::IOBuffer::Construct();

        // ⭐ ADDED 2026-08-02 (camera parameter-chain wave). One of the "aggregate sub-object
        // Constructs" the note below lists: the vehicle input interface's embedded
        // EventQueue<NewVehicleEvent,50>. It is no longer un-homed, and it is now written --
        // BridgeWorldToDirector step 6 Appends the world's queue into it every frame and
        // MainDirector::ProcessNewVehicleEvents drains it. Without this call mpEvents stays
        // NULL and the first Append/AddEvent writes through it.
        mVehicleDriverInputInterface.Construct();

        // The two "none" sentinels. Everything downstream tests them with `> -1` / `== -1`.
        mePlayerCarIndex       = static_cast<EActiveRaceCarIndex>(-1);   // 31400 = -1
        mePlayerKillerCarIndex = static_cast<EActiveRaceCarIndex>(-1);
        miCameraType           = -1;                                     // 31416 = -1

        miRankUpNewRank        = 0;
        miDirectorProfileData  = 0;

        mbRankUpThisFrame                = false;   // 31420
        mbStartNewProfileIntro           = false;   // 31421
        mbStartGameIntroFlyby            = false;   // 31422
        mbStopGameIntroFlyby             = false;   // 31423
        mbPlayerTakenDown                = false;   // 31424
        mbHasGotHookEnumeration          = false;   // 31425
        mbEndOfCarSelect                 = false;   // 31426
        mbGotCrashNavShownEvent          = false;   // 31427
        mbGotCrashNavHiddenEvent         = false;   // 31428
        mbGotColourCalibrationShownEvent = false;   // 31429
        mbGotColourCalibrationHiddenEvent= false;   // 31430
        mbWorldWantsDebugControllerFocus = false;   // 31431
        mbSimPaused                      = false;   // 31432
        mbHasNewDirectorProfileData      = false;   // 31433
        mbPlayerCrashbreakerFired        = false;   // 31434
        mbCarSelectionChangedThisFrame   = false;   // 31435
        mbCarSelectTickerClosedThisFrame = false;   // 31436
        mbGotShortcutMenuEvent           = false;   // 31437
        mbShortcutMenuState              = false;   // 31438
        mbLeftOnlinePostEvent            = false;   // 31439
        mbEnteredOnlinePostEvent         = false;   // 31440
        mbFinishedOnlineEventLoading     = false;   // 31441
        mbStartedOnlineEventLoading      = false;   // 31442
        mbStarting100PercentSequence     = false;   // 31443
        mbFinished100PercentSequence     = false;   // 31444
        maFlagTail[0]                    = 0;       // 31445 (0x7AD5)
        maFlagTail[1]                    = 0;       // 31446 (0x7AD6)

        miRankUpRivalInfo                = 0;       // 31408 (0x7AB0) -- `stw r30, 0x7AB0`

        mUsedRaceCars.UnSetAll();

        // ⭐ @0x82239440-0x82239448: `addi r3, r31, 0x3340; bl VariableEventQueue<13312,16>::
        // Construct`. Without this the queue's mbIsConstructed stays false and EVERY
        // GetFirstEvent / AddEvent on it fires the "Not Constructed" assert.
        mGameActionQueue.Construct();
    }

    // ---- getters (read-lock asserted) -------------------------------------------------------

    const void* InputBuffer::GetContacts() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return mContacts;
    }

    const CgsContainers::BitArray<8u>* InputBuffer::GetUsedRaceCars() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return &mUsedRaceCars;
    }

    const u32* InputBuffer::GetVehicleInfoArray() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        // Returns the address of the X360 VehicleInfo* pointer table (pointer-width u32 slots).
        return maVehicleInfoArray;
    }

    const void* InputBuffer::GetControllerInfo() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return mControllerInfo;
    }

    // ---- ADDITIVE (MainDirector::UpdateArbitrator @0x82271120) ------------------------------

    // The X360 de-inlined accessor (sub_82207040) whose result UpdateArbitrator indexes as
    // `1264 * playerCarIndex + base` -- i.e. the published VehicleInfo array at @0x0990.
    const BrnDirector::Camera::VehicleInfo* InputBuffer::GetRaceCarInfo() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return mRaceCarInfo;
    }

    // ArbStateSharedInfo::mpPlayerCrashInfo == lpInputBuffer + 30944 (@0x78E0), which lands
    // 0xE28 into the honest-opaque contacts span. Addressed off that NAMED member -- this
    // type owns the offset, so no caller has to.
    // FLAG: BrnDirector::PlayerCrashInfo is un-homed; the DWARF slot name is the only
    //   evidence for the role, and the payload is not interpreted here.
    const void* InputBuffer::GetPlayerCrashInfo() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return &mContacts[0x78E0 - 0x6AB8];
    }

    // @0x7AC8. Read by MainDirector::UpdateArbitrator (as Arbitrator::Update's lbPaused) and
    // by the gameplay middle of MainDirector::Update.
    bool InputBuffer::IsSimPaused() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return mbSimPaused;
    }

    // ---- the game-action queue (X360 @0x82206C50) -------------------------------------------
    // The console body is exactly: assert the read lock, then `return this + 0x3340`.
    const CgsModule::VariableEventQueue<13312, 16>* InputBuffer::GetGameActionQueue() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return &mGameActionQueue;
    }

    // The WRITE-side overload BrnGameModule::BridgeGameStateToDirector @0x823CD170 uses to
    // Append the GameStateModule's own queue in under the buffer's write lock. (The X360
    // emits one accessor and lets the caller write through it; splitting it into a const/
    // non-const pair is the same shape every other member of this class already uses.)
    CgsModule::VariableEventQueue<13312, 16>* InputBuffer::GetGameActionQueue()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return &mGameActionQueue;
    }

    // @0x7AC0 / @0x7AAC -- the takedown pair (producer: BridgeGameStateToDirector @0x823CD170).
    bool InputBuffer::GetPlayerTakenDown() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return mbPlayerTakenDown;
    }

    EActiveRaceCarIndex InputBuffer::GetPlayerKillerCarIndex() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return mePlayerKillerCarIndex;
    }

    // @0x7AB0 -- see the header: copied verbatim into GameState +0x1CC by ProcessInputQueue.
    s32 InputBuffer::GetRankUpRivalInfo() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return miRankUpRivalInfo;
    }

    const CgsSystem::TimerStatusInterface* InputBuffer::GetTimerStatusInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return &mTimerInterface;
    }

    // X360 @0x823B27E8 -- the write-side overload (BrnGameModule::BridgeTimers takes the write
    // lock, calls this, and copies the game module's 48-byte snapshot over it).
    CgsSystem::TimerStatusInterface* InputBuffer::GetTimerStatusInterface()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return &mTimerInterface;
    }

    // X360 0x82206DA0 -- read-lock-asserted (:618, "Not locked for reading"), returns
    // `this + 0x6780`. Its only console caller is MainDirector::ProcessNewVehicleEvents.
    const BrnDirectorVehicleInputInterface* InputBuffer::GetVehicleInputInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return &mVehicleDriverInputInterface;
    }

    BrnDirectorVehicleInputInterface* InputBuffer::GetVehicleInputInterface()
    {
        // X360 0x823B2890: the non-const (write-side) handle tests bit 3 (write-lock),
        // unlike the const read-lock overload above.
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return &mVehicleDriverInputInterface;
    }

    const void* InputBuffer::GetHookEnumeration() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        CGS_ASSERT(mbHasGotHookEnumeration, "mbHasGotHookEnumeration");
        return mHookEnumeration;
    }

    EActiveRaceCarIndex InputBuffer::GetPlayerCarIndex() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return mePlayerCarIndex;
    }

    bool InputBuffer::GetShortcutMenuState() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return mbShortcutMenuState;
    }

    // ---- has-event queries (read-lock asserted) ---------------------------------------------

    bool InputBuffer::HasGotHookEnumeration() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return mbHasGotHookEnumeration;
    }

    bool InputBuffer::HasGotShortcutMenuEvent() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return mbGotShortcutMenuEvent;
    }

    bool InputBuffer::HasGotCrashNavShownEvent() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return mbGotCrashNavShownEvent;
    }

    bool InputBuffer::HasGotCrashNavHiddenEvent() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return mbGotCrashNavHiddenEvent;
    }

    bool InputBuffer::HasGotColourCalibrationShownEvent() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return mbGotColourCalibrationShownEvent;
    }

    bool InputBuffer::HasGotColourCalibrationHiddenEvent() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return mbGotColourCalibrationHiddenEvent;
    }

    // ---- the BridgeGuiToDirector command flags (read-lock asserted) --------------------------
    // MainDirector::PostGuiUpdate @0x82236F88 reads every one of these off the locked input
    // buffer; the X360 does it by raw offset because they are private data of a header-inline
    // accessor set the compiler folded away. Named here so neither side indexes the buffer.

    bool InputBuffer::GetStartNewProfileIntro() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return mbStartNewProfileIntro;
    }

    bool InputBuffer::GetStartGameIntroFlyby() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return mbStartGameIntroFlyby;
    }

    bool InputBuffer::GetStopGameIntroFlyby() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return mbStopGameIntroFlyby;
    }

    bool InputBuffer::GetRankUpThisFrame() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return mbRankUpThisFrame;
    }

    s32 InputBuffer::GetRankUpNewRank() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return miRankUpNewRank;
    }

    s32 InputBuffer::GetCameraType() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return miCameraType;
    }

    bool InputBuffer::GetEndOfCarSelect() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return mbEndOfCarSelect;
    }

    bool InputBuffer::HasNewDirectorProfileData() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return mbHasNewDirectorProfileData;
    }

    s32 InputBuffer::GetDirectorProfileData() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return miDirectorProfileData;
    }

    bool InputBuffer::GetCarSelectionChangedThisFrame() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return mbCarSelectionChangedThisFrame;
    }

    bool InputBuffer::GetCarSelectTickerClosedThisFrame() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return mbCarSelectTickerClosedThisFrame;
    }

    bool InputBuffer::GetLeftOnlinePostEvent() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return mbLeftOnlinePostEvent;
    }

    bool InputBuffer::GetEnteredOnlinePostEvent() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return mbEnteredOnlinePostEvent;
    }

    bool InputBuffer::GetFinishedOnlineEventLoading() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return mbFinishedOnlineEventLoading;
    }

    bool InputBuffer::GetStartedOnlineEventLoading() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return mbStartedOnlineEventLoading;
    }

    bool InputBuffer::GetStarting100PercentSequence() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return mbStarting100PercentSequence;
    }

    bool InputBuffer::GetFinished100PercentSequence() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return mbFinished100PercentSequence;
    }

    // ---- mutators (write-lock asserted) -----------------------------------------------------

    void InputBuffer::AppendContacts(const void* lpContacts)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        // X360: this[6830] = *(u32*)lpContacts; i.e. publish the first word of the producer's
        // contact-spy payload into the contacts member's leading word.
        *reinterpret_cast<u32*>(mContacts) = *static_cast<const u32*>(lpContacts);
    }

    void InputBuffer::SetControllerInfo(const void* lpControllerInfo)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        std::memcpy(mControllerInfo, lpControllerInfo, sizeof(mControllerInfo)); // 224 bytes
    }

    void InputBuffer::SetHookEnumeration(const void* lpHookEnumeration)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        std::memcpy(mHookEnumeration, lpHookEnumeration, sizeof(mHookEnumeration)); // 404 bytes
        mbHasGotHookEnumeration = true;
    }

    void InputBuffer::SetRaceCarInfo(u32 luIndex, const BrnDirector::Camera::VehicleInfo& lrInfo)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        CGS_ASSERT(luIndex < 8, "Index < Number of bits");
        mUsedRaceCars.SetBit(luIndex);
        mRaceCarInfo[luIndex] = lrInfo;
    }

    void InputBuffer::SetCrashingCentreOfMass(u32 luIndex, const Matrix44Affine& lrCentreOfMass)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        // X360 copies the 4-row affine (4 * 16-byte VMX stores) into the indexed VehicleInfo's
        // mCrashingCentreOfMass (element+0x460) then sets mbHasCrashingCenterOfMass (element+0x4E5).
        mRaceCarInfo[luIndex].mCrashingCentreOfMass     = lrCentreOfMass;
        mRaceCarInfo[luIndex].mbHasCrashingCenterOfMass = true;
    }

    void InputBuffer::SetVehicleTeam(EActiveRaceCarIndex leIndex, s32 liTeam)
    {
        CGS_ASSERT(leIndex >= E_ACTIVE_RACE_CAR_INDEX_INVALID, "leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_INVALID");
        CGS_ASSERT(leIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,    "leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        // X360: *(s32*)&this[4*idx + 12856] = team; the team id is stored into the idx'th slot of
        // the VehicleInfo pointer table (a pointer-width slot reused as the team id).
        maVehicleInfoArray[leIndex] = static_cast<u32>(liTeam);
    }

    // ADDITIVE setters for the two published fields MainDirector::GetLivePlayerCarIndex reads
    // (see the header). Same write-lock assert as every other mutator in this TU.
    void InputBuffer::SetPlayerCarIndex(EActiveRaceCarIndex leIndex)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        mePlayerCarIndex = leIndex;
    }

    void InputBuffer::SetRaceCarInUse(u32 luIndex, bool lbInUse)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        CGS_ASSERT(luIndex < 8u, "Index < Number of bits");   // CgsBitArray.h:222
        if (lbInUse)
            mUsedRaceCars.SetBit(luIndex);
        else
            mUsedRaceCars.UnSetBit(luIndex);
    }

    // @0x325C -- the player's boost fill fraction. BridgeWorldToDirector @0x823E3AB0 stores
    // it inline (`stfsx` at input+0x325C) under the same write lock as its neighbours; the
    // named entry point is ours, the field and the value are the console's.
    void InputBuffer::SetPlayerBoostPercentage(f32 lfPercentage)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        mfPlayerBoostPercentage = lfPercentage;
    }

    f32 InputBuffer::GetPlayerBoostPercentage() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return mfPlayerBoostPercentage;
    }

    void InputBuffer::SetShortcutMenuEvent(bool lbState)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        mbShortcutMenuState    = lbState;
        mbGotShortcutMenuEvent = true;
    }

    void InputBuffer::SetGotCrashNavShownEvent()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        mbGotCrashNavShownEvent = true;
    }

    void InputBuffer::SetGotCrashNavHiddenEvent()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        mbGotCrashNavHiddenEvent = true;
    }

    void InputBuffer::SetGotColourCalibrationShownEvent()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        mbGotColourCalibrationShownEvent = true;
    }

    void InputBuffer::SetGotColourCalibrationHiddenEvent()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        mbGotColourCalibrationHiddenEvent = true;
    }

    // ---- the BridgeGuiToDirector command setters (write-lock asserted) -----------------------
    // One per arm of BrnGameModule::BridgeGuiToDirector @0x823CBF70. The X360 stores the
    // literal 1 (or the event's payload word) straight into the buffer under the write lock the
    // bridge's caller DoUpdate_DirectorPostGUI @0x823DCE38 has already taken.

    void InputBuffer::SetStartNewProfileIntro()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        mbStartNewProfileIntro = true;
    }

    void InputBuffer::SetStartGameIntroFlyby()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        mbStartGameIntroFlyby = true;
    }

    void InputBuffer::SetStopGameIntroFlyby()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        mbStopGameIntroFlyby = true;
    }

    // X360 case 303 sets the flag, reads the payload word, sets the flag AGAIN and then stores
    // the rank -- the duplicated store is the compiler's, not a second observable.
    void InputBuffer::SetRankUp(s32 liNewRank)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        mbRankUpThisFrame = true;
        miRankUpNewRank   = liNewRank;
    }

    void InputBuffer::SetCameraType(s32 liCameraType)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        miCameraType = liCameraType;
    }

    void InputBuffer::SetEndOfCarSelect()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        mbEndOfCarSelect = true;
    }

    void InputBuffer::SetDirectorProfileData(s32 liData)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        mbHasNewDirectorProfileData = true;
        miDirectorProfileData       = liData;
    }

    void InputBuffer::SetCarSelectionChangedThisFrame()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        mbCarSelectionChangedThisFrame = true;
    }

    void InputBuffer::SetCarSelectTickerClosedThisFrame()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        mbCarSelectTickerClosedThisFrame = true;
    }

    void InputBuffer::SetLeftOnlinePostEvent()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        mbLeftOnlinePostEvent = true;
    }

    void InputBuffer::SetEnteredOnlinePostEvent()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        mbEnteredOnlinePostEvent = true;
    }

    void InputBuffer::SetFinishedOnlineEventLoading()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        mbFinishedOnlineEventLoading = true;
    }

    void InputBuffer::SetStartedOnlineEventLoading()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        mbStartedOnlineEventLoading = true;
    }

    void InputBuffer::SetStarting100PercentSequence()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        mbStarting100PercentSequence = true;
    }

    void InputBuffer::SetFinished100PercentSequence()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        mbFinished100PercentSequence = true;
    }

    // ---- control-input accessors (read/write) -----------------------------------------------
    // ⛔⛔ CORRECTED 2026-08-01 -- THIS PAIR WAS POINTING AT THE WRONG BLOCK.
    //
    // Both overloads used to be attributed to X360 0x82206C50 / 0x823B2740 and to
    // `return this + 0x3340`. Those two addresses are NOT GetControll: they are the const and
    // write-side halves of InputBuffer::GetGameActionQueue (0x82206C50 asserts at
    // BrnDirectorModuleIO.h:583 and 0x823B2740 at :574, and BOTH end `addi r3, r28, 0x3340` --
    // which is the game-action queue, as ProcessInputQueue proves by handing that exact result
    // to VariableEventQueue<13312,16>::GetFirstEvent).
    //
    // The REAL InputBuffer::GetControll is @0x822070E8 (assert line 0x2E6 == 742) and it ends
    //     0x82207180  addi  r3, r28, 0x3260
    // i.e. it returns the 224-byte CONTROLLER SNAPSHOT -- the same block SetControllerInfo
    // memcpy's 224 bytes into. sizeof(ControlInput) (48 + sizeof(DebugController) == 220) fits
    // that span and nothing else, which is a third independent confirmation.
    //
    // WHAT THE BUG DID: MainDirector::UpdateArbitrator's cycle-camera / look-back bits and
    // ArbStateSharedInfo::mpControllerInfo were read out of the event queue's first bytes
    // (mbIsConstructed and macData[0..3]) instead of the controller. It never crashed and never
    // asserted -- the classic silent wrong-data failure -- because the queue span is zeroed by
    // the PC bring-up's memset before Construct, so every control bit read as a plausible 0.
    // ⚠️ The X360 emits ONE accessor and lets the caller write through it; the const/non-const
    //    split (and therefore which lock bit each asserts) is this class's house style.

    // X360 0x822070E8 (BrnDirectorModuleIO.h:742): read-lock; return &mControllerInfo (this+0x3260).
    const ControlInput* InputBuffer::GetControll() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return reinterpret_cast<const ControlInput*>(mControllerInfo);
    }

    // Write-side overload of the same accessor: tests bit 3 (write-lock) instead of bit 4.
    ControlInput* InputBuffer::GetControll()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return reinterpret_cast<ControlInput*>(mControllerInfo);
    }
}
}
