// FX-AIPAD (crash parity 2026-09-25): the "AI PAD" harness seat (BRN_AI_PAD_PLAYER), run on the PRODUCTION
// bodies. run_fxaipad_seat.py extracts them from the revision under test and writes two includes:
//   fxaipad_ai.inc    -- BrnAIModule_Drive.cpp: gHarnessAIPad's initialiser, HarnessAIPadBeginPlayerSeat /
//                        HarnessAIPadEndPlayerSeat / HarnessAIPadNoteReset (namespace BrnAI)
//   fxaipad_world.inc -- BrnWorldModule.cpp: the seat's anonymous-namespace block (the stash, the trace
//                        budget, the mode parser) and WorldModule::HarnessArmAIPadPlayer /
//                        HarnessStashAIPadControls / HarnessApplyAIPad (namespace BrnWorld)
// against the small stand-ins below. What is pinned:
//   * OFF MEANS OFF. The initialiser is OFF / unarmed / no target / no route; with BRN_AI_PAD_PLAYER unset
//     the arm writes nothing and prints nothing, the scope never touches mbIsDrivenByPlayer, the stash
//     never locks the AI output and the pad is never written.
//   * THE SCOPE. Armed, only the player's AICar with a human marked as driving it (the byte
//     StoreDrivenCarData @0x827957F0 writes 1 for control word 1) is presented with the console seat's 0,
//     and End puts 1 back; a rival, a null car and a car the console's own seat holds (byte 0) are left
//     alone.
//   * THE READ SEAT. The stash reads the AI output's record queue through the READ accessor
//     (OutputBuffer::GetVehicleDriverInterface() const, 0x8279CA00 -- the one
//     BridgeAIModuleToPhysicsModule @0x827AAAA8 uses under LockForRead). The first live run of the seat
//     (fxaipad_cruise/20260925_100036, exe 7af8cd487056) called the non-const WRITE seat (0x8276D9C8)
//     under a read lock: 6655 x "Not locked for writing" (BrnAIModuleIO_OutputBuffer.cpp:98). The stand-in
//     counts write-seat calls, so that body fails here.
//   * THE RECORD. Only the E_DRIVER_TYPE_AI record whose miVehicleID is the player's active slot is taken
//     (a PLAYER record and another car's AI record are skipped).
//   * THE PAD. The AI's gas / brake / handbrake / steering / boost overwrite the real pad's
//     mfAcceleration / mfBraking / mfHandBrake / mfSteering / mbBoost, mbIsWheel is set (the AI record's
//     +0x41 mbIsSteeringWheel is 1, 0x82796020; ProcessPlayerVehicleInput @0x822FFE30 copies mbIsWheel
//     into that byte, 0x82300264..0x82300270), every other pad field is the real pad's; a held real BRAKE
//     passes the real pad through untouched (the runner's UNSTICK fallback); inside the slam window the
//     pad holds throttle 1, brake 0 and the boost button.
//   * THE RESET NOTE. Only the current target's reset is remembered, and only while armed.
#include "types.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <string>

#include "GameSource/World/AI/BrnAIHarnessPad.h"
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnPlayerVehicleControls.h"

// ---- the log sink (CgsDev::Log::gpDebugPrint) --------------------------------------------------------
namespace CgsDev
{
    namespace Log
    {
        struct Sink
        {
            std::string mText;
            Sink& operator<<(const char* lpc) { mText += (lpc != 0) ? lpc : "(null)"; return *this; }
            Sink& operator<<(int li)          { mText += std::to_string(li); return *this; }
            Sink& operator<<(unsigned lu)     { mText += std::to_string(lu); return *this; }
            Sink& operator<<(float lf)        { mText += std::to_string(lf); return *this; }
            Sink& operator<<(double lf)       { mText += std::to_string(lf); return *this; }
        };
        Sink  gSink;
        Sink* gpDebugPrint = &gSink;
    }
}

// ---- AI stand-ins ------------------------------------------------------------------------------------
namespace BrnAI
{
    struct AICar
    {
        static const u16 KI_INVALID_SECTION_INDEX = 0x7FFF;
        bool mbIsPlayer;
        bool mbIsDrivenByPlayer;
        bool mbIsInGameMode;
    };
}

// The production AI-side bodies (namespace BrnAI in BrnAIModule_Drive.cpp).
namespace BrnAI
{
#include "fxaipad_ai.inc"
}

namespace CgsModule
{
    struct Event
    {
        s32 miPad;
    };
}

namespace BrnPhysics
{
    namespace Vehicle
    {
        // BrnVehicleDriverControls.h:26 -- E_DRIVER_TYPE_AI = 1.
        enum EDriverTypeStandIn { E_DRIVER_TYPE_PLAYER = 0, E_DRIVER_TYPE_AI = 1 };

        struct BrnAIDriverControls : CgsModule::Event
        {
            s32  miVehicleID;
            f32  mfGas;
            f32  mfBrake;
            f32  mfHandBrake;
            f32  mfSteering;
            bool mbBoost;
        };

        struct VehicleDriverInputInterface
        {
            struct UpdateDriverEventQueue
            {
                const CgsModule::Event* mapEvents[8];
                s32                     maiTypes[8];
                s32                     miCount;

                s32 GetFirstEvent(const CgsModule::Event** lppEvent, s32* lpiSize) const
                {
                    if (miCount <= 0) { *lppEvent = 0; return -1; }
                    *lppEvent = mapEvents[0];
                    *lpiSize  = static_cast<s32>(sizeof(BrnAIDriverControls));
                    return maiTypes[0];
                }
                s32 GetNextEvent(const CgsModule::Event* lpPrevious, const CgsModule::Event** lppEvent, s32* lpiSize) const
                {
                    for (s32 li = 0; li + 1 < miCount; ++li)
                    {
                        if (mapEvents[li] == lpPrevious)
                        {
                            *lppEvent = mapEvents[li + 1];
                            *lpiSize  = static_cast<s32>(sizeof(BrnAIDriverControls));
                            return maiTypes[li + 1];
                        }
                    }
                    *lppEvent = 0;
                    return -1;
                }
            };

            UpdateDriverEventQueue mQueue;
            const UpdateDriverEventQueue* GetUpdateDriverQueue() const { return &mQueue; }
        };
    }
}

namespace BrnAI
{
    struct AIDriver
    {
        bool   mbActive;
        AICar* mpCar;
        bool   IsActive() const { return mbActive; }
        AICar* GetCar() const { return mpCar; }
    };

    namespace AIModuleIO
    {
        // The AI output buffer's two seats. The non-const accessor is the WRITE seat (0x8276D9C8), which on
        // the console and in the tree asserts "Not locked for writing" under a read lock; the const one is
        // the READ seat (0x8279CA00).
        struct OutputBuffer
        {
            BrnPhysics::Vehicle::VehicleDriverInputInterface mInterface;
            s32  miLocksForRead;
            s32  miUnlocksForRead;
            bool mbLockedForRead;
            mutable s32 miReadSeatCalls;
            s32  miWriteSeatCalls;
            s32  miWriteSeatCallsUnderReadLock;

            void LockForRead()   { ++miLocksForRead; mbLockedForRead = true; }
            void UnlockForRead() { ++miUnlocksForRead; mbLockedForRead = false; }
            BrnPhysics::Vehicle::VehicleDriverInputInterface* GetVehicleDriverInterface()
            {
                ++miWriteSeatCalls;
                if (mbLockedForRead) ++miWriteSeatCallsUnderReadLock;
                return &mInterface;
            }
            const BrnPhysics::Vehicle::VehicleDriverInputInterface* GetVehicleDriverInterface() const
            {
                ++miReadSeatCalls;
                return &mInterface;
            }
        };
    }

    struct AIModule
    {
        AIDriver maDrivers[8];
        const AIDriver* GetAIDriver(s32 liSlot) const { return &maDrivers[liSlot]; }
    };
}

// ---- world stand-ins ---------------------------------------------------------------------------------
namespace BrnWorld
{
    enum EActiveRaceCarIndex
    {
        E_ACTIVE_RACE_CAR_INDEX_INVALID = -1,
        E_ACTIVE_RACE_CAR_INDEX_0       = 0,
    };
    enum ECarControlStandIn
    {
        E_CAR_CONTROL_NONE          = 0,
        E_CAR_CONTROL_ENTITY_MODULE = 1,
        E_CAR_CONTROL_AI_MODULE     = 2,
    };

    namespace BrnWorldIO
    {
        typedef BrnWorld::PlayerVehicleControls PlayerVehicleControls;
        struct UpdateInputBuffer
        {
            const PlayerVehicleControls* mpPad;
            const PlayerVehicleControls* GetPlayerVehicleControls() const { return mpPad; }
        };
    }

    namespace RaceCarEntityModuleIO
    {
        struct InputBuffer_PreScene
        {
            s32                   miWrites;
            PlayerVehicleControls mLast;
            void SetPlayerVehicleControls(const PlayerVehicleControls* lpControls) { ++miWrites; mLast = *lpControls; }
        };
    }

    class WorldModule
    {
    public:
        void HarnessArmAIPadPlayer();
        void HarnessStashAIPadControls( BrnAI::AIModuleIO::OutputBuffer* lpAIOutput );
        void HarnessApplyAIPad( RaceCarEntityModuleIO::InputBuffer_PreScene* lpRaceCarInput_PreScene,
                                const BrnWorldIO::UpdateInputBuffer* lpUpdateInputBuffer );

        EActiveRaceCarIndex meLocalPlayerActiveRaceCarIndex;
        s32                 maeCarControls[8];
        BrnAI::AIModule     mAIModule;
    };

// The production world-side bodies (namespace BrnWorld in BrnWorldModule.cpp).
#include "fxaipad_world.inc"
}

// ---- the checks --------------------------------------------------------------------------------------
static unsigned gChecks = 0;
static unsigned gFailures = 0;

static void Check(bool lbPassed, const char* lpcWhat)
{
    ++gChecks;
    if (!lbPassed)
    {
        ++gFailures;
        std::printf("FAIL  %s\n", lpcWhat);
    }
    else
    {
        std::printf("pass  %s\n", lpcWhat);
    }
}

static bool Near(f32 a, f32 b) { return std::fabs(a - b) < 1e-6f; }

int main()
{
    using namespace BrnAI;
    using BrnWorld::PlayerVehicleControls;

    // ---- 1. the initialiser: OFF / unarmed / no target / no route -----------------------------------
    Check(gHarnessAIPad.meMode == E_HARNESS_AI_PAD_OFF, "initialiser: meMode OFF");
    Check(!gHarnessAIPad.mbArmed, "initialiser: unarmed");
    Check(gHarnessAIPad.miTarget == -1 && !gHarnessAIPad.mbRamming && !gHarnessAIPad.mbTargetReset,
          "initialiser: no target, not ramming, no reset mark");
    Check(!gHarnessAIPad.mbRouteOwned
          && gHarnessAIPad.muSavedDestinationSection == AICar::KI_INVALID_SECTION_INDEX
          && gHarnessAIPad.muOwnedDestinationSection == AICar::KI_INVALID_SECTION_INDEX,
          "initialiser: no route held, sections invalid");

    // A world that WOULD arm (player slot 0 attached, AI driver slot active, control word 1).
    static AICar lPlayer = { true, true, false };
    static AICar lRival  = { false, false, false };
    static BrnWorld::WorldModule lWorld;
    lWorld.meLocalPlayerActiveRaceCarIndex = BrnWorld::E_ACTIVE_RACE_CAR_INDEX_0;
    for (s32 li = 0; li < 8; ++li) { lWorld.maeCarControls[li] = 2; lWorld.mAIModule.maDrivers[li].mbActive = false; lWorld.mAIModule.maDrivers[li].mpCar = 0; }
    lWorld.maeCarControls[0] = BrnWorld::E_CAR_CONTROL_ENTITY_MODULE;
    lWorld.mAIModule.maDrivers[0].mbActive = true;
    lWorld.mAIModule.maDrivers[0].mpCar    = &lPlayer;

    // ---- 2. OFF means OFF: the variable unset -> the arm writes and prints nothing ------------------
    _putenv("BRN_AI_PAD_PLAYER=");
    CgsDev::Log::gSink.mText.clear();
    lWorld.HarnessArmAIPadPlayer();
    lWorld.HarnessArmAIPadPlayer();
    Check(gHarnessAIPad.meMode == E_HARNESS_AI_PAD_OFF && !gHarnessAIPad.mbArmed,
          "OFF: the arm leaves gHarnessAIPad OFF and unarmed on an armable world");
    Check(CgsDev::Log::gSink.mText.empty(), "OFF: the arm prints nothing");

    // ---- 3. OFF: the scope, the reset note, the stash and the pad are inert --------------------------
    Check(!HarnessAIPadBeginPlayerSeat(&lPlayer) && lPlayer.mbIsDrivenByPlayer,
          "OFF: Begin refuses the player's car and leaves mbIsDrivenByPlayer 1");
    HarnessAIPadEndPlayerSeat(&lPlayer, false);
    Check(lPlayer.mbIsDrivenByPlayer, "OFF: End(false) leaves the byte alone");
    gHarnessAIPad.miTarget = 3;   // (a stale target must still not be marked while unarmed)
    HarnessAIPadNoteReset(3);
    Check(!gHarnessAIPad.mbTargetReset, "OFF: a reset is never noted while unarmed");
    gHarnessAIPad.miTarget = -1;

    static AIModuleIO::OutputBuffer lOutput = {};
    static BrnPhysics::Vehicle::BrnAIDriverControls lPlayerAIRecord = {};
    lPlayerAIRecord.miVehicleID = 0;
    lPlayerAIRecord.mfGas = 0.8f; lPlayerAIRecord.mfBrake = 0.1f; lPlayerAIRecord.mfHandBrake = 0.2f;
    lPlayerAIRecord.mfSteering = -0.4f; lPlayerAIRecord.mbBoost = true;
    static BrnPhysics::Vehicle::BrnAIDriverControls lPadRecord = {};      // a PLAYER-type record for slot 0
    lPadRecord.miVehicleID = 0; lPadRecord.mfGas = 0.05f;
    static BrnPhysics::Vehicle::BrnAIDriverControls lRivalAIRecord = {};  // another car's AI record
    lRivalAIRecord.miVehicleID = 2; lRivalAIRecord.mfGas = 0.3f; lRivalAIRecord.mfSteering = 0.7f;
    lOutput.mInterface.mQueue.miCount = 3;
    lOutput.mInterface.mQueue.mapEvents[0] = &lPadRecord;      lOutput.mInterface.mQueue.maiTypes[0] = BrnPhysics::Vehicle::E_DRIVER_TYPE_PLAYER;
    lOutput.mInterface.mQueue.mapEvents[1] = &lRivalAIRecord;  lOutput.mInterface.mQueue.maiTypes[1] = BrnPhysics::Vehicle::E_DRIVER_TYPE_AI;
    lOutput.mInterface.mQueue.mapEvents[2] = &lPlayerAIRecord; lOutput.mInterface.mQueue.maiTypes[2] = BrnPhysics::Vehicle::E_DRIVER_TYPE_AI;

    static PlayerVehicleControls lRealPad = {};
    lRealPad.mfXAxis0 = 0.25f; lRealPad.mfAcceleration = 1.0f; lRealPad.mfBraking = 0.0f; lRealPad.mfSteering = 0.9f;
    lRealPad.mbHorn = true; lRealPad.mbReset = false; lRealPad.mbIsWheel = false; lRealPad.mbBoost = false;
    static BrnWorld::BrnWorldIO::UpdateInputBuffer lUpdateInput = { &lRealPad };
    static BrnWorld::RaceCarEntityModuleIO::InputBuffer_PreScene lPreScene = {};

    lWorld.HarnessStashAIPadControls(&lOutput);
    lWorld.HarnessApplyAIPad(&lPreScene, &lUpdateInput);
    Check(lOutput.miLocksForRead == 0 && lOutput.miReadSeatCalls == 0 && lOutput.miWriteSeatCalls == 0,
          "OFF: the stash never locks or reads the AI output");
    Check(lPreScene.miWrites == 0, "OFF: the pad is never written");

    // ---- 4. armed: the scope ------------------------------------------------------------------------
    gHarnessAIPad.mbArmed = true;
    gHarnessAIPad.meMode  = E_HARNESS_AI_PAD_CRUISE;
    const bool lbTaken = HarnessAIPadBeginPlayerSeat(&lPlayer);
    Check(lbTaken && !lPlayer.mbIsDrivenByPlayer,
          "armed: Begin gives the player's own update the console seat's mbIsDrivenByPlayer 0");
    HarnessAIPadEndPlayerSeat(&lPlayer, lbTaken);
    Check(lPlayer.mbIsDrivenByPlayer, "armed: End puts the frame's 1 back");
    lRival.mbIsDrivenByPlayer = false;
    Check(!HarnessAIPadBeginPlayerSeat(&lRival) && !lRival.mbIsDrivenByPlayer, "armed: a rival's car is never taken");
    Check(!HarnessAIPadBeginPlayerSeat(0), "armed: a null car is never taken");
    static AICar lConsoleSeat = { true, false, false };   // the console's own seat holds the car (byte 0)
    const bool lbConsole = HarnessAIPadBeginPlayerSeat(&lConsoleSeat);
    HarnessAIPadEndPlayerSeat(&lConsoleSeat, lbConsole);
    Check(!lbConsole && !lConsoleSeat.mbIsDrivenByPlayer,
          "armed: a player car the console seat already holds (byte 0) is left at 0");

    // ---- 5. armed: the reset note -------------------------------------------------------------------
    gHarnessAIPad.miTarget = 3;
    HarnessAIPadNoteReset(4);
    Check(!gHarnessAIPad.mbTargetReset, "armed: another car's reset is not the target's");
    HarnessAIPadNoteReset(3);
    Check(gHarnessAIPad.mbTargetReset, "armed: the target's reset is noted");
    gHarnessAIPad.mbTargetReset = false;
    gHarnessAIPad.miTarget = -1;
    HarnessAIPadNoteReset(-1);
    Check(!gHarnessAIPad.mbTargetReset, "armed: no target -> nothing is noted");

    // ---- 6. armed: the stash reads the player's AI record through the READ seat -----------------------
    lWorld.HarnessStashAIPadControls(&lOutput);
    Check(lOutput.miWriteSeatCalls == 0,
          "stash: the WRITE seat (0x8276D9C8) is never called -- the live run's 6655 'Not locked for writing'");
    Check(lOutput.miReadSeatCalls == 1, "stash: the READ seat (0x8279CA00) is used once");
    Check(lOutput.miLocksForRead == 1 && lOutput.miUnlocksForRead == 1 && !lOutput.mbLockedForRead,
          "stash: one LockForRead, balanced by one UnlockForRead");

    lWorld.HarnessApplyAIPad(&lPreScene, &lUpdateInput);
    Check(lPreScene.miWrites == 1, "pad: written once");
    const PlayerVehicleControls& lrWritten = lPreScene.mLast;
    Check(Near(lrWritten.mfAcceleration, 0.8f) && Near(lrWritten.mfBraking, 0.1f) && Near(lrWritten.mfHandBrake, 0.2f)
          && Near(lrWritten.mfSteering, -0.4f) && lrWritten.mbBoost,
          "pad: the player's AI record (not the PLAYER record, not car 2's) -- gas/brake/handbrake/steer/boost");
    Check(lrWritten.mbIsWheel, "pad: mbIsWheel set (the AI record's +0x41 mbIsSteeringWheel is 1)");
    Check(lrWritten.mbHorn && Near(lrWritten.mfXAxis0, 0.25f) && !lrWritten.mbReset,
          "pad: every other field is the real pad's");

    // ---- 7. ramming: throttle, no brake, the boost button ---------------------------------------------
    lPlayerAIRecord.mbBoost = false;
    lPlayerAIRecord.mfBrake = 0.6f;
    gHarnessAIPad.mbRamming = true;
    lWorld.HarnessStashAIPadControls(&lOutput);
    lWorld.HarnessApplyAIPad(&lPreScene, &lUpdateInput);
    Check(lPreScene.miWrites == 2 && Near(lPreScene.mLast.mfAcceleration, 1.0f) && Near(lPreScene.mLast.mfBraking, 0.0f)
          && lPreScene.mLast.mbBoost && Near(lPreScene.mLast.mfSteering, -0.4f),
          "ram: throttle 1, brake 0, boost held; the steering is still the AI's (the Slam fan)");
    gHarnessAIPad.mbRamming = false;

    // ---- 8. a held real BRAKE passes the real pad through ---------------------------------------------
    lRealPad.mfBraking = 1.0f;
    CgsDev::Log::gSink.mText.clear();
    lWorld.HarnessStashAIPadControls(&lOutput);
    lWorld.HarnessApplyAIPad(&lPreScene, &lUpdateInput);
    Check(lPreScene.miWrites == 2, "manual: a held real brake -> the pad is not overwritten");
    Check(CgsDev::Log::gSink.mText.find("[ai-pad] manual override") != std::string::npos,
          "manual: the override is logged");
    lRealPad.mfBraking = 0.0f;

    // ---- 9. no record for the player -> nothing is applied ------------------------------------------
    lOutput.mInterface.mQueue.miCount = 2;   // the PLAYER record and car 2's AI record only
    lWorld.HarnessStashAIPadControls(&lOutput);
    lWorld.HarnessApplyAIPad(&lPreScene, &lUpdateInput);
    Check(lPreScene.miWrites == 2, "no AI record for the player's slot -> the pad is not written");

    // ---- 10. disarmed again: the stash is invalidated -------------------------------------------------
    lOutput.mInterface.mQueue.miCount = 3;
    lWorld.HarnessStashAIPadControls(&lOutput);
    gHarnessAIPad.mbArmed = false;
    lWorld.HarnessStashAIPadControls(&lOutput);
    lWorld.HarnessApplyAIPad(&lPreScene, &lUpdateInput);
    Check(lPreScene.miWrites == 2, "disarmed: the next stash is invalid and the pad is not written");

    std::printf("FxAiPadSeat: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
