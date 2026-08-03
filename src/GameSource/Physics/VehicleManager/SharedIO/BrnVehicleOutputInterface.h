#pragma once

// ============================================================================
// BrnPhysics::Vehicle vehicle-output interfaces
//   GameSource/Physics/VehicleManager/SharedIO/BrnVehicleOutputInterface.h
//   (DWARF home BrnVehicleOutputInterface.h)
//
// The per-frame output payloads the vehicle manager publishes to the world/game-state modules.
// Reconstructed from BURNOUT_X360_ARTIST.XEX + the DecFIGS DWARF. Member names/types/order are
// verbatim from the DWARF; the reconstructed member offsets match the X360 accessor/operator=
// stores. These aggregates are embedded BY VALUE in the RaceCarEntityModuleIO / PhysicsModuleIO
// output buffers, so the layouts are the real full member sets (the previous NOMINAL 256-byte
// blobs are replaced).
//
// GameEventQueue is modelled as an opaque, size-correct storage span (== the committed
// GameStateModuleIO mGameEventQueueStorage pattern) because the CgsModule::VariableEventQueue<1536,16>
// class it aliases is not yet reconstructed; the operator= copies it as a raw block, matching the
// X360. WheelFFSpring / GameEventQueue members are only ever block-copied here.
// ============================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Module/CgsEventQueue.h"                          // CgsModule::EventQueue<T,N>
#include "GameShared/GameClasses/Containers/CgsBitArray.h"                        // CgsContainers::BitArray<N>
#include "GameShared/GameClasses/System/Input/CgsInputTypes.h"                    // CgsInput::Device::WheelFFSpring
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h"          // RaceCarState, ImpactEvent, PhysicalTrafficState + traffic/crash/reset events

namespace BrnPhysics
{
namespace Vehicle
{
    // The per-frame aggressive-driving summary published to the GUI/scoring (5 bools).
    // DWARF BrnVehicleOutputInterface.h:283.
    struct AggressiveDrivingFlags
    {
        bool mbPlayerWonSlamThisFrame;      // +0
        bool mbPlayerLostSlamThisFrame;     // +1
        bool mbPlayerWonGrindingThisFrame;  // +2
        bool mbPlayerLostGrindingThisFrame; // +3
        bool mbRubbingThisFrame;            // +4

        void Clear();
    };

    // Three per-frame GUI grinding/rubbing flags. DWARF BrnVehicleOutputInterface.h:66.
    struct VehicleGuiOutputMessages
    {
        bool mbPlayerGrindingOther;  // +0
        bool mbOtherGrindingPlayer;  // +1
        bool mbRubbing;              // +2
    };

    // ------------------------------------------------------------------------
    // VehicleOutputInterface  (DWARF BrnVehicleOutputInterface.h:306)
    //   The per-race-car physics snapshot bundle: an 8-entry used-cars mask, the 8 RaceCarState
    //   snapshots, the impact/traffic-state event queues, the game-event queue and the
    //   aggressive-driving flags. sizeof matches the X360 operator= walk (mUsedRaceCars @0,
    //   maRaceCarStates @0x10, mImpactEventQueue @0x2310, mTrafficStateQueue @0x2620,
    //   mGameEventQueue @0x65F0 (0x610 bytes), mAggressiveDrivingFlags @0x6C00).
    // ------------------------------------------------------------------------
    struct alignas(16) VehicleOutputInterface
    {
        typedef CgsModule::EventQueue<ImpactEvent, 16>          ImpactEventQueue;           // BrnVehicleEvents.h:575
        typedef CgsModule::EventQueue<PhysicalTrafficState, 20> PhysicalTrafficStateQueue;  // BrnVehicleEvents.h:574

        // @0x825EC390 (declaration-only in this ledger; see BrnPhysicsVehicle_FlaggedUnhomed.cpp /
        // the .cpp FLAG): a deep VMX128 per-wheel projection routine reaching SimpleVehiclePhysics
        // internals not homed in a committed header, so it is intentionally NOT bodied here.
        void AddTrafficState(EntityId lEntityID, const void* lpPhysicalTrafficVehicle);

        RaceCarState*       GetRaceCarState(s32 liRaceCarIndex);         // @0x822B4860 (non-const)
        const RaceCarState* GetRaceCarState(s32 liRaceCarIndex) const;  // @0x825C08D0 (const)

        // The X360 asm-called symbol distinct from GetRaceCarState -- returns the same &maRaceCarStates[i].
        const RaceCarState* GetRaceCar(u32 luRaceCarIndex) const;

        const CgsContainers::BitArray<8u>& GetUsedCarsBitArray() const { return mUsedRaceCars; } // DWARF :370

        // @0x823C89C8: hand-written copy assignment (ADDITIVE GROW: a real ledger func not in the
        // DWARF member set; no field reordered/retyped).
        VehicleOutputInterface& operator=(const VehicleOutputInterface& lOther);

    private:
        CgsContainers::BitArray<8u> mUsedRaceCars;            // @0x0000  (DWARF :382)
        RaceCarState                maRaceCarStates[8];       // @0x0010  (DWARF :383, stride 1120)
        ImpactEventQueue            mImpactEventQueue;        // @0x2310  (DWARF :384)
        PhysicalTrafficStateQueue   mTrafficStateQueue;       // @0x2620  (DWARF :385)
        // GameEventQueue == CgsModule::VariableEventQueue<1536,16> (not yet reconstructed); modelled
        // as opaque size-correct storage (0x610 == 1552 bytes) -- only ever block-copied here.
        u8                          mGameEventQueueStorage[0x610]; // @0x65F0 (DWARF :386, GameEventQueue)
        AggressiveDrivingFlags      mAggressiveDrivingFlags;  // @0x6C00  (DWARF :387)
    };

    // ------------------------------------------------------------------------
    // CrashingRaceCarInterface  (DWARF BrnVehicleOutputInterface.h:399)
    //   An 8-entry scratch array of "is this race car crashing" flags, populated from a
    //   VehicleOutputInterface by SetFromVehicleOutputInterface.
    // ------------------------------------------------------------------------
    struct CrashingRaceCarInterface
    {
        void Clear();
        // @0x823625C0: copy each in-use car's RaceCarState::mbResetCarTransform flag into the array.
        void SetFromVehicleOutputInterface(const VehicleOutputInterface* lpOutput);
        bool IsCrashing(s32 liIndex) const;

    private:
        bool mabCrashingRaceCars[8];   // @0
    };

    // ------------------------------------------------------------------------
    // VehicleManagerOutputInterface  (DWARF BrnVehicleOutputInterface.h:82)
    //   The vehicle-manager's traffic/race-car event-queue bundle plus the traffic-type request
    //   queue, GUI messages and the wheel FFB spring. The full member set is laid out (all element
    //   types are committed); mCrashedTrafficEventQueue is the first member at offset 0.
    // ------------------------------------------------------------------------
    struct alignas(16) VehicleManagerOutputInterface
    {
        // ---- ADDITIVE (moved 2026-07-24 from the retired BrnVehicleManager.h shell;
        //      declare-only surface, bodied by this interface's own TU) ----

        // sink+26096: the IO event queue the crash record + takedown/grind events push onto.
        // Returned by reference so the bodies can call .AddEvent / .AddEventSafe by name.
        CgsModule::VariableEventQueue<1536, 16>& GetEventQueue();

        // The cross-module "a race car crashed" event (X360 AddRaceCarCrashEvent). FLAG: the X360
        // call passes nine positional args (a leading 0, the victim entity id, a byte-offset/flag, a
        // local-vs-remote flag, an optional crash matrix, the was-crash-state-1 bool, a 0, and the
        // splatted crash position). The load-bearing args are kept; the rest are MODELLED as a single
        // packed call. Declare-only -- bodied by the interface's own TU.
        // ⭐⭐ TWO ARGUMENT ROLES CORRECTED 2026-08-03 (VehiclePhysics own-block wave). This used to
        // take `const Matrix44Affine& lCrashMatrix` and `Vector3 lvCrashPosition`. BOTH were wrong,
        // and both errors came from the same mis-based reading of VehicleManager::SetRaceCarCrashing:
        //   * the "crash matrix" was a single 16-byte `stvx128 v127, r30, 0x1440` -- a VECTOR store
        //     into RaceCarPhysics::mCrashNormal -- and the value passed to this call is that same
        //     v127 register (`vmr128 v1, v127` @0x82635484). Not a 64-byte matrix.
        //   * the "crash position" is a SCALAR FLOAT. Both call sites (@0x826354BC and @0x82635530)
        //     do `lvx128 v0, record+0xEF0 ; vspltw v0,v0,0 ; stvx128 ; lfs f1` -- lane .x of
        //     mvSpeedOnLastCrashMPH_TimeCrashing_CounterSteerSideMag_Spare, i.e. the crash SPEED.
        // FLAG (unchanged): the console call still passes more registers than this models -- a
        // second vector v2 (== v126) and three more integer args. Those are not recovered; the
        // simplification is deliberate and is not made worse by this correction.
        void AddRaceCarCrashEvent(EntityId lVictimEntityId,
                                  bool lbLocalPhysicalCrash,
                                  Vector3 lvCrashNormal,
                                  bool lbWasInCrashState1,
                                  f32 lfCrashSpeedMPH);

        // sink+1872: the secondary "remapped entity id" sub-event the type-2-id path fires. Declare-only.
        void AddRemappedEntityIdEvent(u32 luRemappedActiveRaceCarIndex);

        // sink+27648 / sink+27649: the two driver-feedback bytes HandleRaceCarRaceCarContact OR-sets
        // when a takedown is scored (the asm `*(a32+27648) |= ...; *(a32+27649) = ...`). Declare-only.
        void FlagTakedownScoredForDriver(bool lbVictimIsHighSlot);
    

        typedef CgsModule::EventQueue<TrafficCrashedEvent, 20> TrafficCrashedEventQueue;      // :55
        typedef CgsModule::EventQueue<TrafficSlammedEvent, 20> TrafficSlammedEventQueue;      // :56
        typedef CgsModule::EventQueue<TrafficCrashedEvent, 10> FineTrafficCrashedEventQueue;  // :57
        typedef CgsModule::EventQueue<RaceCarCrashEvent, 8>    RaceCarCrashEventQueue;        // :58
        typedef CgsModule::EventQueue<RaceCarResetEvent, 8>    RaceCarResetEventQueue;        // :59
        typedef CgsModule::EventQueue<CreateVehicleResult, 8>  CreateVehicleResultQueue;      // :54
        typedef CgsModule::EventQueue<u16, 32>                 TrafficTypeRequestQueue;       // BrnTrafficTypeInterface.h:50
        typedef CgsModule::EventQueue<TrafficRemovedEvent, 25> RemovedTrafficEventQueue;      // :60

        // @0x825C0658: queue a "physical-traffic vehicle crashed" event and return its slot index.
        s32 AddCrashedTrafficEvent(VolumeInstanceId lVolumeInstanceID, EntityId lCrasherEntityID);

        // @0x827A9B20: hand-written copy assignment. Clears each event queue member (miLength = 0,
        // the same store-8-zero the generic BaseEventQueue<T>::Clear() does) then Append()s the
        // matching member from lOther onto it -- i.e. "become a copy of lOther's live events" rather
        // than a raw memberwise copy (a plain-copy operator= would also duplicate mpEvents/miMaxLength,
        // which are per-instance backing-buffer bookkeeping the X360 body deliberately leaves alone).
        // The trailing VehicleGuiOutputMessages (@0x79C, 3 bools) and WheelFFSpring (@0x874, 2 floats)
        // are plain block copies (no Append -- they are not queues), reconstructed as named-member
        // assignment. Called by *::InputBuffer_PostPhysics::SetVehicleManagerOutputInterface and
        // WorldModule::BridgePhysicsToOutput.
        VehicleManagerOutputInterface& operator=(const VehicleManagerOutputInterface& lOther);

    private:
        TrafficCrashedEventQueue     mCrashedTrafficEventQueue;     // @0x0000  (DWARF :176)
        TrafficSlammedEventQueue     mSlammedTrafficEventQueue;     // @0x0150  (DWARF :177)
        FineTrafficCrashedEventQueue mFineTrafficCrashedEventQueue; // @0x02F0  (DWARF :178)
        RaceCarCrashEventQueue       mRaceCarCrashEventQueue;       // @0x03A0  (DWARF :179)
        RaceCarResetEventQueue       mRaceCarResetEventQueue;       // @0x05B0  (DWARF :180)
        CreateVehicleResultQueue     mCreateVehicleResultQueue;     // @0x06C0  (DWARF :181)
        TrafficTypeRequestQueue      mTrafficTypeRequestQueue;      // @0x0750  (DWARF :182)
        VehicleGuiOutputMessages     mVehicleGuiOutputMessages;     // @0x079C  (DWARF :183)
        RemovedTrafficEventQueue     mRemovedTrafficEventQueue;     // @0x07A0  (DWARF :184)
        CgsInput::Device::WheelFFSpring mWheelFFSpring;             // @0x0874  (DWARF :185)
    };
}
}
