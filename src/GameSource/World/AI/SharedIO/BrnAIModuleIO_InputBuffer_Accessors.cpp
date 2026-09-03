#include "GameSource/World/AI/SharedIO/BrnAIModuleIO.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"                     // CGS_ASSERT

#include <cstring>   // std::memcpy (the timer block copy; see SetTimerInterface)

// Out-of-line bodies of BrnAI::AIModuleIO::InputBuffer's Construct and its lock-guarded
// Get/Set/Append accessors. NAMED LAYOUT since 2026-09-03 (aiwave lane A4): every accessor
// returns / copies the member it names; the console byte offsets quoted in the comments are
// identity evidence, not host addressing. Lock bits per the asm -- read (bit 4) for the
// const getters, write (bit 3) for the mutable getters, the setters and the appends. The
// lock-assert strings carry the trailing "\n" of the X360 rodata; the non-null asserts do
// not. Reproduced verbatim, not "fixed".

namespace BrnAI
{
namespace AIModuleIO
{
    // ---------------------------------------------------------------------------------------------
    // InputBuffer::Construct @0x8278AB80 -- COMPLETE. (Until 2026-09-03 this built ONE member,
    // the reset-on-track request queue, and named the other legs as parked: they had no named
    // homes inside the image blob. They have homes now.)
    //
    // The console body in its own order (offsets this-relative, documentation only):
    //   0x8278ABA4  stb 1                                    IOBuffer::Construct (status byte)
    //   0x8278ABA8  sth 0 @+0x43E0                           } TrafficAIInterface::Construct, INLINED:
    //   0x8278ABAC  bl RivalInTrafficUpdateEvent,34>::Construct(+0xF3F0)  }   mu16EntityCount / mUpdateRivalQueue /
    //   0x8278ABC8  stw 0 @+0xFAE8 ; stw 0 @+0xFB74          }   mAddedRivals.miCount / mRemovedRivals.miCount
    //   0x8278ABD0  bl CgsSystem::TimerStatusInterface::Clear(+0xFB80)
    //   0x8278ABDC  nine `std 0` @+0x2C0..+0x300             RaceCarAIInterface's nine BitArray<8>
    //                                                        (interface +0x2B0..+0x2F0) -- BitArray::Prepare inlined
    //   0x8278AC00  bl VariableEventQueue<16384,16>::Construct(+0x308)    RaceCarAIInterface::mManagementQueue (+0x2F8)
    //   0x8278AC08  stb 0 @+0x4344                           RaceCarAIInterface::mbPlayerDataSet (+0x4334)
    //   0x8278AC10  bl ResetOnTrackRequest,128>::Construct(+0xFBB0)       mAIModuleRequestInterface (its only member)
    //   0x8278AC1C  bl GameStateModuleIO::RaceCarRaceDistanceInterface::Clear(+0x13860)
    //   0x8278AC28  bl VariableEventQueue<13312,16>::Construct(+0x103BC)  mGameActionQueue
    //   0x8278AC34  bl VariableEventQueue<32768,16>::Construct(+0x13888)  mSceneResultQueue
    //   0x8278AC40  bl TakedownEvent,8>::Construct(+0x1B898)              mTakedownEventQueue
    //   0x8278AC5C  thirteen `stfs 0.0f` @+0x1B9E8..+0x1BA18 and seven `stb 0` @+0x1BA1C..+0x1BA22
    //                                                        mPlayerVehicleControls: the 13 floats and the
    //                                                        FIRST SEVEN bools; mbBoostBounce (+0x3B) is NOT written
    //   0x8278ACAC  bl RaceRouteRequest,1>::Construct(+0x137D0)           mRaceRouteRequestQueue (tail position)
    //
    // RaceCarAIInterface declares no Construct in the DWARF (its three legs are written here by
    // name, exactly as the console inlines them). TrafficAIInterface::Construct (DWARF :137) is
    // bodied in BrnTrafficAIInterfaces.cpp from this very sequence (cross-witnessed by
    // BrnTrafficIO::OutputBuffer_PostScene::Construct @0x82761830, which inlines the same four
    // stores). PlayerVehicleControls::Construct (BrnPlayerVehicleControls.h:63) has no body in
    // the tree and the console writes 7 of its 8 bools, so those stores are spelled by name.
    // ---------------------------------------------------------------------------------------------
    void InputBuffer::Construct()
    {
        CgsModule::IOBuffer::Construct();

        mTrafficAIInterface.Construct();
        mTimerInterface.Clear();

        mRaceCarAIInterface.mInAirBits.Prepare();
        mRaceCarAIInterface.mCrashingBits.Prepare();
        mRaceCarAIInterface.mShowtimeBits.Prepare();
        mRaceCarAIInterface.mOnStartLineBits.Prepare();
        mRaceCarAIInterface.mDriftingBits.Prepare();
        mRaceCarAIInterface.mRaceCarContactBits.Prepare();
        mRaceCarAIInterface.mPlayerContactBits.Prepare();
        mRaceCarAIInterface.mSetActiveRaceCars.Prepare();
        mRaceCarAIInterface.mFrontRayOccluded.Prepare();
        mRaceCarAIInterface.mManagementQueue.Construct();
        mRaceCarAIInterface.mbPlayerDataSet = false;

        mAIModuleRequestInterface.GetResetOnTrackRequestQueue()->Construct();
        mRaceCarRaceDistanceInterface.Clear();
        mGameActionQueue.Construct();
        mSceneResultQueue.Construct();
        mTakedownEventQueue.Construct();

        mPlayerVehicleControls.mfXAxis1       = 0.0f;
        mPlayerVehicleControls.mfXAxis0       = 0.0f;
        mPlayerVehicleControls.mfYAxis1       = 0.0f;
        mPlayerVehicleControls.mfYAxis0       = 0.0f;
        mPlayerVehicleControls.mfXSensor      = 0.0f;
        mPlayerVehicleControls.mfYSensor      = 0.0f;
        mPlayerVehicleControls.mfZSensor      = 0.0f;
        mPlayerVehicleControls.mfGSensor      = 0.0f;
        mPlayerVehicleControls.mfAcceleration = 0.0f;
        mPlayerVehicleControls.mfBraking      = 0.0f;
        mPlayerVehicleControls.mfHandBrake    = 0.0f;
        mPlayerVehicleControls.mfSteering     = 0.0f;
        mPlayerVehicleControls.mfSpin         = 0.0f;
        mPlayerVehicleControls.mbHorn         = false;
        mPlayerVehicleControls.mbChangeView   = false;
        mPlayerVehicleControls.mbStart        = false;
        mPlayerVehicleControls.mbReset        = false;
        mPlayerVehicleControls.mbToggle       = false;
        mPlayerVehicleControls.mbBoost        = false;
        mPlayerVehicleControls.mbIsWheel      = false;

        mRaceRouteRequestQueue.Construct();
    }

    // ---- getters (read-lock bit 4) ----------------------------------------------

    // X360 0x8276D728 (R, :124) -- the pre-scene race-car AI view (console +0x10).
    const RaceCarAIInterface* InputBuffer::GetRaceCarAIInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return &mRaceCarAIInterface;
    }

    // X360 0x8276D7D0 (R, :127) -- the traffic-AI interface (console +0x43E0).
    const InputBuffer::TrafficAIInterface* InputBuffer::GetTrafficAIInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return &mTrafficAIInterface;
    }

    // Pre-wave spelling of the accessor above (kept for its callers); same seat, same lock.
    const InputBuffer::TrafficAIInterface* InputBuffer::GetTrafficAI() const
    {
        return GetTrafficAIInterface();
    }

    // X360 0x8276D920 (R, :130) -- the timer-status interface (console +0xFB80).
    const CgsSystem::TimerStatusInterface* InputBuffer::GetTimerInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return &mTimerInterface;
    }

    // X360 0x8276D878 (R, :133) -- the AI-module request interface (console +0xFBB0).
    const AIModuleRequestInterface* InputBuffer::GetAIModuleRequestInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return &mAIModuleRequestInterface;
    }

    // DWARF :136 (W) -- the mutable twin. No out-of-line X360 symbol (inlined); the write
    // bit is what every mutable accessor of this family tests.
    AIModuleRequestInterface* InputBuffer::GetAIModuleRequestInterface()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        return &mAIModuleRequestInterface;
    }

    // X360 0x8276D488 (R, :138) -- the race-route request queue (console +0x137D0).
    // Callers: BrnAI::AIModule::PausedUpdate / Update.
    const InputBuffer::RaceRouteRequestQueue* InputBuffer::GetRaceRouteRequestQueue() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return &mRaceRouteRequestQueue;
    }

    // DWARF :139 (W) -- mutable twin (inlined on the console).
    InputBuffer::RaceRouteRequestQueue* InputBuffer::GetRaceRouteRequestQueue()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        return &mRaceRouteRequestQueue;
    }

    // DWARF :142 (R) -- the race-car race-distance block (console +0x13860); inlined on the console.
    const InputBuffer::RaceCarRaceDistanceInterface* InputBuffer::GetRaceCarRaceDistanceInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return &mRaceCarRaceDistanceInterface;
    }

    // DWARF :145 (R) / :146 (W) -- the scene-result queue (console +0x13888); inlined on the console.
    const InputBuffer::SceneResultQueue* InputBuffer::GetSceneResultQueue() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return &mSceneResultQueue;
    }

    InputBuffer::SceneResultQueue* InputBuffer::GetSceneResultQueue()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        return &mSceneResultQueue;
    }

    // X360 0x8276D530 (R, :148) -- the game-action queue (console +0x103BC).
    const InputBuffer::GameActionQueue* InputBuffer::GetGameActionQueue() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return &mGameActionQueue;
    }

    // X360 0x8279C4F8 (W, :149) -- the MUTABLE game-action queue. Asserts the WRITE lock
    // (`extrwi r11, r11, 1,28` == bit 3, "Not locked for writing\n"), reproduced verbatim.
    // IDA truncates this symbol to "InputBuffer::Get"; Get() below is that spelling.
    InputBuffer::GameActionQueue* InputBuffer::GetGameActionQueue()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        return &mGameActionQueue;
    }

    InputBuffer::GameActionQueue* InputBuffer::Get()
    {
        return GetGameActionQueue();
    }

    // DWARF :151 (R) -- the takedown event ring (console +0x1B898); inlined on the console.
    const InputBuffer::TakedownEventQueue* InputBuffer::GetTakedownEventQueue() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return &mTakedownEventQueue;
    }

    // X360 0x8276D5D8 (R, :154) -- the player vehicle controls block (console +0x1B9E8).
    const InputBuffer::PlayerVehicleControls* InputBuffer::GetPlayerVehicleControls() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return &mPlayerVehicleControls;
    }

    // ---- setters / appends (write-lock bit 3) -----------------------------------

    // X360 0x8279C700 (W, :108) -- copies a RaceCarAIInterface (console XMemCpy 0x43D0) into
    // the member. The type is pointer-free apart from its embedded VariableEventQueue, which
    // is itself an inline byte image, so the member copy IS the console block copy.
    void InputBuffer::SetRaceCarAIInterface(const RaceCarAIInterface* lpInterface)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        CGS_ASSERT(lpInterface != nullptr, "lpInterface != NULL");
        mRaceCarAIInterface = *lpInterface;
    }

    // X360 0x8279C7E0 (W, :112) -- copies the traffic-AI interface (console XMemCpy 0xB7A0).
    // NOTE the copied EventQueue<RivalInTrafficUpdateEvent,34> keeps the SOURCE's mpEvents,
    // exactly as the console's byte copy does; readers see the source buffer's ring.
    void InputBuffer::SetTrafficAIInterface(const TrafficAIInterface* lpInterface)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        CGS_ASSERT(lpInterface != nullptr, "lpInterface != NULL");
        mTrafficAIInterface = *lpInterface;
    }

    // X360 0x827AC960 (W, :116) -- `Clear(); Append(src);` on the 128-deep ResetOnTrackRequest
    // queue, the request interface's only member (DWARF BrnAIModuleRequestInterface.h:109).
    void InputBuffer::AppendAIModuleRequestInterface(const AIModuleRequestInterface* lpRequestInterface)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");

        typedef AIModuleRequestInterface::ResetOnTrackRequestQueue Queue;
        Queue& lDest = *mAIModuleRequestInterface.GetResetOnTrackRequestQueue();
        const Queue& lSource = *lpRequestInterface->GetResetOnTrackRequestQueue();

        lDest.Clear();
        lDest.Append(lSource);
    }

    // X360 0x8279C8C0 (W, :120) -- copies the 48-byte timer block (two 24-byte TimerStatus
    // runs: word/float/float/byte/word/float each, 0x8279C984..0x8279CA00). A block copy is
    // used because CgsSystem::TimerStatusInterface::operator= is declaration-only in the
    // tree; the type is pointer-free and its size is pinned to the console's.
    void InputBuffer::SetTimerInterface(const CgsSystem::TimerStatusInterface* lpTimerInterface)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        CGS_ASSERT(lpTimerInterface != nullptr, "lpTimerInterface != NULL");
        static_assert(sizeof(CgsSystem::TimerStatusInterface) == 48,
                      "TimerStatusInterface is the console's two 24-byte TimerStatus runs");
        std::memcpy(&mTimerInterface, lpTimerInterface, sizeof(mTimerInterface));
    }

    // X360 0x827A9560 (W, :140) -- Append the race-route request queue (console +0x137D0).
    void InputBuffer::AppendRaceRouteRequestQueue(const RaceRouteRequestQueue* lpQueue)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        mRaceRouteRequestQueue.Append(*lpQueue);
    }

    // X360 0x8279C428 (W, :143) -- the 10-word copy (`li r9, 0xA ; mtctr ; lwz/stw` loop,
    // 0x8279C4D0..0x8279C4E8) into the race-distance block (console +0x13860).
    void InputBuffer::SetRaceCarRaceDistanceInterface(const RaceCarRaceDistanceInterface* lpObject)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        static_assert(sizeof(RaceCarRaceDistanceInterface) == 40,
                      "RaceCarRaceDistanceInterface is the console's 10-word block");
        mRaceCarRaceDistanceInterface = *lpObject;
    }

    // X360 0x827A9618 (W, :152) -- `stw 0, 8(queue)` (the inlined Clear) then
    // EventQueue<TakedownEvent,8>::Append (console +0x1B898).
    void InputBuffer::SetTakedownEventQueue(const TakedownEventQueue* lpQueue)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        mTakedownEventQueue.Clear();
        mTakedownEventQueue.Append(*lpQueue);
    }

    // X360 0x8279C5A0 (W, :155) -- memcpy(60) into the player vehicle controls (console +0x1B9E8).
    void InputBuffer::SetPlayerVehicleControls(const PlayerVehicleControls* lpControls)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        static_assert(sizeof(PlayerVehicleControls) == 60,
                      "PlayerVehicleControls is the console's 60-byte block");
        mPlayerVehicleControls = *lpControls;
    }
}
}
