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
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"                  // CgsModule::VariableEventQueue<BUFSIZE,ALIGN>
#include "GameShared/GameClasses/Containers/CgsBitArray.h"                        // CgsContainers::BitArray<N>
#include "GameShared/GameClasses/Physics/CgsPhysicsSimulationIO_Events.h"         // InAddJoint / InRemoveJoint / InAddRigidBody / InRemoveRigidBody / InChangeRigidBodyInertia
#include "GameShared/GameClasses/System/Input/CgsInputTypes.h"                    // CgsInput::Device::WheelFFSpring
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h"          // RaceCarState, ImpactEvent, PhysicalTrafficState + traffic/crash/reset events

namespace BrnPhysics
{
namespace Vehicle
{
    // Forward declarations, pointer-only (documented exception (b) in AGENTS.md): the publish
    // surface added below takes `const RaceCarPhysics*` / `const VehicleDriver*` and never
    // dereferences them in this header. Including RaceCarPhysics.h / BrnVehicleDriver.h here
    // would pull the whole vehicle-physics cascade into every IO buffer that embeds this
    // interface (PhysicsModuleIO, RaceCarEntityModuleIO, CrashModuleIO, ...). The real headers
    // are included by BrnVehicleOutputInterface_UpdateRaceCarState.cpp, which owns the bodies.
    class  RaceCarPhysics;   // RaceCarPhysics.h  (class)
    struct VehicleDriver;    // BrnVehicleDriver.h (struct)

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

        // ⭐ ADDED 2026-08-10 (create-path wave). DWARF BrnVehicleOutputInterface.h:312.
        // X360-ATTESTED as the INLINED body of PhysicsModuleIO::OutputBuffer::Construct
        // @0x825ABB10 over its +44128 seat -- see the .cpp for the instruction-by-instruction
        // decode. Constructs the two event queues and the game-event queue, then zeroes the
        // used-cars bitset and the aggressive-driving flags.
        void Construct();

        // @0x825EC390 (declaration-only in this ledger; see BrnPhysicsVehicle_FlaggedUnhomed.cpp /
        // the .cpp FLAG): a deep VMX128 per-wheel projection routine reaching SimpleVehiclePhysics
        // internals not homed in a committed header, so it is intentionally NOT bodied here.
        void AddTrafficState(EntityId lEntityID, const void* lpPhysicalTrafficVehicle);

        RaceCarState*       GetRaceCarState(s32 liRaceCarIndex);         // @0x822B4860 (non-const)
        const RaceCarState* GetRaceCarState(s32 liRaceCarIndex) const;  // @0x825C08D0 (const)

        // The X360 asm-called symbol distinct from GetRaceCarState -- returns the same &maRaceCarStates[i].
        const RaceCarState* GetRaceCar(u32 luRaceCarIndex) const;

        const CgsContainers::BitArray<8u>& GetUsedCarsBitArray() const { return mUsedRaceCars; } // DWARF :370

        // ⭐⭐ ADDED 2026-08-11 (physics->output publish wave) -- THE PUBLISH SURFACE.
        // Four DWARF-declared methods (BrnVehicleOutputInterface.h :323 / :328 / :333 / :339),
        // all four X360-ATTESTED by VehicleManager::WriteOutVehicleStats @0x8263F460, which is
        // the console's only caller of the first and inlines the other three:
        //     UpdateRaceCarState  -> out-of-line @0x825EC808 (535 insns), `bl` at 0x8263F724
        //     SetEntityID         -> inlined,  `stw r11, 0x3D8(state)` == maRaceCarStates[i].mEntityId
        //     SetRaceCarHidden    -> inlined,  `stb r11, 0x462(state)` == ....mbIsHidden
        //     SetWheelTransform   -> inlined,  the 4x4 `lvx128/stvx128` run at 0x8263F7F4..0x8263F844
        // Bodied in BrnVehicleOutputInterface_UpdateRaceCarState.cpp.
        //
        // ⚠️ THE INDEX ARGUMENT IS THE ENTITY INDEX, NOT THE RACE-CAR SLOT, on UpdateRaceCarState
        // ALONE -- and that is the console's own arithmetic, reproduced rather than "corrected":
        // WriteOutVehicleStats passes `maRaceCarEntityIDs[liRaceCar].GetEntityIndex()`
        // (`lwzx r11,r31,r18 ; extrwi r4,r11,14,8` == (id >> 10) & 0x3FFF) while every OTHER
        // store in the same iteration is indexed by liRaceCar. The two coincide on the console
        // because the vehicle manager mints each race car's EntityId with entityIndex == its own
        // slot; if they ever diverge on this build the divergence is the create path's, not this
        // function's. FLAGGED for the verifier.
        void UpdateRaceCarState(s32 liRaceCarIndex,
                                const RaceCarPhysics* lpRaceCarPhysics,
                                const VehicleDriver*  lpDriver,
                                bool                  lbForceReset);
        void SetEntityID(s32 liRaceCarIndex, EntityId lEntityID);
        void SetRaceCarHidden(s32 liRaceCarIndex, bool lbHidden);
        void SetWheelTransform(u8 lu8RaceCarIndex, u8 lu8Wheel, Matrix44Affine lTransform);

        // ⚠️ ADDITIVE GROW, flagged. The DWARF names only the CONST bitset accessor (:370); this is
        // its non-const sibling, and it exists because WriteOutVehicleStats' very first act is a
        // whole-word write of the vehicle manager's live-car bitset over this member --
        //     addis r9,r18,1 ; addi r9,r9,-0x5340   ; == &VehicleManager::mUsedRaceCars (this+44224)
        //     ld    r10,0(r9)  ;  std r10,0(r19)    ; == lpOutputInterface->mUsedRaceCars = ...
        // -- which the console emits inline (member access from a friend/same-TU context that this
        // reconstruction cannot reproduce). Exposing the member's own accessor pair is the smallest
        // honest seam; no field is reordered or retyped.
        CgsContainers::BitArray<8u>& GetUsedCarsBitArray() { return mUsedRaceCars; }

        // @0x823C89C8: hand-written copy assignment (ADDITIVE GROW: a real ledger func not in the
        // DWARF member set; no field reordered/retyped).
        VehicleOutputInterface& operator=(const VehicleOutputInterface& lOther);

        // ⭐ ADDED 2026-08-06 (UpdateVehiclePhysics wave). The game-event queue @0x65F0 as its
        // LIVE type -- DWARF :386 types the member GameEventQueue == CgsModule::
        // VariableEventQueue<1536,16>; the storage stays the size-correct opaque span until the
        // queue is homed as a typed member, and this accessor is the one sanctioned span-cast
        // seam (same pattern as VehicleManager::Construct's maRaceCarDebugComponent store).
        // The X360 reaches it as a bare `addi this, 0x65F0` at its AddEvent sites
        // (0x826437A4 / 0x82643B48) and at UpdateVehiclePhysics' ProcessAftertouchEvents call.
        CgsModule::VariableEventQueue<1536, 16>* GetGameEventQueue()
        {
            return reinterpret_cast<CgsModule::VariableEventQueue<1536, 16>*>(
                &mGameEventQueueStorage[0]);
        }

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
        //
        // ⛔⛔ TWO OF THE THREE BELOW ARE ON THE WRONG CLASS -- PROVEN 2026-08-03 (task #110),
        // NOT FIXED HERE. They are not X360 symbols at all (nothing named GetEventQueue,
        // AddRemappedEntityIdEvent or FlagTakedownScoredForDriver exists in progress/identity.json);
        // they are accessor names a previous wave minted over raw offsets on the sink pointer the
        // asm calls r17, and it attached them to THIS class. The asm settles which object r17 is:
        //     0x826437A4  addi r3, r17, 0x65F0   ; bl VariableEventQueue<1536,16>::AddEventSafe
        //     0x82643B48  addi r3, r17, 0x65F0   ; bl VariableEventQueue<1536,16>::AddEvent
        //     0x82643B00  lbz  r9, 0x6C00(r17)
        //     0x82643B1C  stb  r9, 0x6C00(r17)
        // 0x65F0 == 26096 and 0x6C00 == 27648 are, EXACTLY, VehicleOutputInterface::
        // mGameEventQueueStorage and VehicleOutputInterface::mAggressiveDrivingFlags -- see that
        // struct's member table thirty lines above. VehicleManagerOutputInterface's own last member
        // ends at 0x87C, so +26096 and +27648 are ~24 KB OUT OF BOUNDS for the class they are
        // declared on; a body written here could only have reached them by leaving the object.
        // The load-bearing consequence is at the CALL SITES: BrnVehicleManager.cpp:611/707/738
        // invoke these on `lpManagerOutputInterface`, but r17 is the *other* interface argument.
        // ⇒ Before bodying any of these, move the two out-of-bounds ones onto the VehicleOutputInterface
        // the contact handler is actually handed (mind that BrnGameState::GameStateModuleIO has its
        // own same-named class -- decide which of the two r17 is) and re-point the call sites.
        // ⭐ AddRemappedEntityIdEvent is the one that DOES belong here: its +1872 == 0x750 is
        // mTrafficTypeRequestQueue, in bounds and on this class.

        // sink+26096: the IO event queue the crash record + takedown/grind events push onto.
        // Returned by reference so the bodies can call .AddEvent / .AddEventSafe by name.
        // ⛔ WRONG CLASS -- see the block above. 0x65F0 is VehicleOutputInterface's.
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
        // ⭐ IN BOUNDS AND ON THE RIGHT CLASS: 1872 == 0x750 == mTrafficTypeRequestQueue below.
        void AddRemappedEntityIdEvent(u32 luRemappedActiveRaceCarIndex);

        // sink+27648 / sink+27649: the two driver-feedback bytes HandleRaceCarRaceCarContact OR-sets
        // when a takedown is scored (the asm `*(a32+27648) |= ...; *(a32+27649) = ...`). Declare-only.
        // ⛔ WRONG CLASS -- see the ⛔⛔ block above. 27648 == 0x6C00 is VehicleOutputInterface::
        // mAggressiveDrivingFlags, whose first two bools are mbPlayerWonSlamThisFrame (+0) and
        // mbPlayerLostSlamThisFrame (+1) -- which is what "takedown scored, for which slot" means.
        void FlagTakedownScoredForDriver(bool lbVictimIsHighSlot);
    

        typedef CgsModule::EventQueue<TrafficCrashedEvent, 20> TrafficCrashedEventQueue;      // :55
        typedef CgsModule::EventQueue<TrafficSlammedEvent, 20> TrafficSlammedEventQueue;      // :56
        typedef CgsModule::EventQueue<TrafficCrashedEvent, 10> FineTrafficCrashedEventQueue;  // :57
        typedef CgsModule::EventQueue<RaceCarCrashEvent, 8>    RaceCarCrashEventQueue;        // :58
        typedef CgsModule::EventQueue<RaceCarResetEvent, 8>    RaceCarResetEventQueue;        // :59
        typedef CgsModule::EventQueue<CreateVehicleResult, 8>  CreateVehicleResultQueue;      // :54
        typedef CgsModule::EventQueue<u16, 32>                 TrafficTypeRequestQueue;       // BrnTrafficTypeInterface.h:50
        typedef CgsModule::EventQueue<TrafficRemovedEvent, 25> RemovedTrafficEventQueue;      // :60

        // ⭐ ADDED 2026-08-10 (create-path wave). DWARF BrnVehicleOutputInterface.h:86, and an
        // OUT-OF-LINE X360 symbol at 0x822E6790 (25 instructions) -- not an inline, so the body
        // in the .cpp is a direct transcription rather than an inference. Called by
        // PhysicsModuleIO::OutputBuffer::Construct @0x825ABB10 over its +41952 seat.
        void Construct();

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

        // ⭐ ADDED 2026-08-06 (UpdateVehiclePhysics wave). DWARF-attested accessor
        // (BrnVehicleOutputInterface.h:256 `void SetPlayerWheelFFSpring(const CgsInput::
        // Device::WheelFFSpring&)`); the X360 inlines it as the two-word copy into +0x874
        // (UpdateVehiclePhysics @0x82645FB8..C4).
        void SetPlayerWheelFFSpring(const CgsInput::Device::WheelFFSpring& lrSpring)
        {
            mWheelFFSpring = lrSpring;
        }

        // ⭐ ADDED 2026-08-11 (create-drain wave). DWARF-attested (BrnVehicleOutputInterface.h:214
        // `int32_t AddCreateVehicleResult(CreateVehicleResult)`), and the X360 inlines it as a bare
        // `addi r11, <managerOut>, 0x6C0` + `bl BaseEventQueue<CreateVehicleResult>::AddEvent` at
        // the tail of VehicleManager::ProcessCreateEvents (@0x82616818 / @0x826173E4). 0x6C0 ==
        // 1728 == mCreateVehicleResultQueue's seat below, so the accessor is the same member the
        // console addresses, reached by name instead of by that offset.
        // ⚠️ The DWARF returns int32_t (the slot index -- the queue length after the append, minus
        // one, the same convention as AddCrashedTrafficEvent). ProcessCreateEvents ignores it.
        s32 AddCreateVehicleResult(CreateVehicleResult lResult)
        {
            mCreateVehicleResultQueue.AddEvent(lResult);
            return mCreateVehicleResultQueue.GetLength() - 1;
        }

        // ⭐ ADDED 2026-08-18 (wave Q5, car-registration finisher). The READ half of the pair
        // above. DWARF-attested (BrnVehicleOutputInterface.h:148, dumpfile :232
        // `const VehicleManagerOutputInterface::CreateVehicleResultQueue* GetCreateVehicleResults()
        // const`), and X360-attested at its one consumer: RaceCarEntityModule::
        // ProcessCreateVehicleEvents @0x822FF620 reaches the queue with a bare
        // `addi r30, r3, 0x6C0` (0x822FF6A0) off the VehicleManagerOutputInterface it just
        // fetched, then walks it with `lwz r11, 8(r30)` (miLength) + BaseEventQueue<
        // CreateVehicleResult>::GetEvent -- i.e. the accessor is inlined there, over exactly
        // mCreateVehicleResultQueue's own seat (0x6C0 == 1728, the member below).
        // Same shape as the sibling GetXxxQueue readers the DWARF lists at :142/:145/:151/:154.
        const CreateVehicleResultQueue* GetCreateVehicleResults() const
        {
            return &mCreateVehicleResultQueue;
        }

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

    // ------------------------------------------------------------------------
    // VehicleOutputRequestInterface  (DWARF BrnVehicleOutputInterface.h:197)   NEW 2026-08-03
    //
    // The vehicle manager's REQUEST channel into the physics simulation: six queues of
    // "please do this to the simulation" events, drained by
    // PhysicsModule::BridgeVehicleManagerRequestsToSimulation.
    //
    // ⭐⭐ THE LAYOUT IS DERIVED, NOT GUESSED, AND IT CLOSES ON TWO INDEPENDENT ASM PINS.
    // The member ORDER is the DecFIGS DWARF's (BrnVehicleOutputInterface.h:263,264,265,268,270,271).
    // Two absolute member offsets are pinned by the X360 asm:
    //   * mAddJointQueue    @ 39904 -- VehicleOutputRequestInterface::AddJoint @0x825E7170 ends
    //       `addis r3,r28,1 ; addi r3,r3,-0x6420 ; bl BaseEventQueue<InAddJoint>::AddEvent`
    //       (65536 - 0x6420 == 39904).  [that function is an .ida-exports HOLE -- absent from
    //        identity.json AND from the PS3 set; pulled with headless IDA 9.3, 53 instructions]
    //   * mRemoveJointQueue @ 41840 -- ArticulatedJointPool::SendCreateRemoveJointEvents @0x826013C0
    //       does `addis r26,r26,1 ; addi r26,r26,-0x5C90` before
    //       `bl BaseEventQueue<InRemoveJoint>::AddEvent` (65536 - 0x5C90 == 41840).
    // Running the chain forward from 0 using ONLY element strides that were attested elsewhere
    // (each of the five payload types carries its own X360 stride derivation in
    // CgsPhysicsSimulationIO_Events.h; VariableEventQueue<N,16> is 1 + N -> 4-aligned + 12):
    //     mRequiredRigidBodiesQueue     16 + 50*192 =  9616      [    0 ..  9616)
    //     mRemoveRigidBodyQueue         16 + 50*16  =   816      [ 9616 .. 10432)
    //     mRequestFineLineQueue                       13456      [10432 .. 23888)
    //     mChangeRigidBodyInertiaQueue  16 + 200*80 = 16016      [23888 .. 39904)   <-- PIN 1 hit
    //     mAddJointQueue                16 + 10*192 =  1936      [39904 .. 41840)   <-- PIN 2 hit
    //     mRemoveJointQueue             16 + 10*8   =    96      [41840 .. 41936)
    // Both pins land with ZERO slack, so sizeof == 41936 -- and the HOST reproduces it exactly,
    // because every element type is pointer-free and BaseEventQueue<T>'s only pointer sits inside a
    // header that is padded to 16 on both targets (X360 4+4+4 -> 16-aligned element array; host
    // 8+4+4 == 16). The gate is in ArticulatedJointPool_layout_check.cpp.
    //
    // FLAG (named honestly rather than hidden): the DWARF spells mRequestFineLineQueue's type
    // `CgsSceneManager::SceneManagerIO::InFineQueryQueue<13440>`, which the DWARF also shows is
    // `: public VariableEventQueue<13440,16>` WITH NO DATA MEMBERS OF ITS OWN (only the four
    // LineTest*/VolumeTest* helpers). That derived class has no home in this tree yet, so the
    // member is declared as its base. Layout-identical; the four helpers are simply not reachable
    // through this member until the SceneManagerIO fine-query TU lands.
    //
    // FLAG: only the two joint methods are declared. The DWARF lists twelve more (Append, Construct,
    // Clear, CreateNewRigidBody, RemoveRigidBody, AddLineRequest, AddChangeRigidBodyInertiaEvent and
    // the five Get*Queue accessors); none of them is bodied anywhere in this tree, and per the
    // "gate each DWARF declaration on X360 attestation" rule they are left out rather than declared
    // as an unbacked surface.
    // ------------------------------------------------------------------------
    struct alignas(16) VehicleOutputRequestInterface
    {
        typedef CgsPhysics::PhysicsSimulationIO::InAddJoint    InAddJoint;     // BrnVehicleQueues.h:39 element
        typedef CgsPhysics::PhysicsSimulationIO::InRemoveJoint InRemoveJoint;  // BrnVehicleQueues.h:41 element

        typedef CgsModule::EventQueue<CgsPhysics::PhysicsSimulationIO::InAddRigidBody, 50>            InAddRigidBodyQueue;           // BrnVehicleQueues.h:32
        typedef CgsModule::EventQueue<CgsPhysics::PhysicsSimulationIO::InRemoveRigidBody, 50>         InRemoveRigidBodyQueue;        // BrnVehicleQueues.h:33
        typedef CgsModule::VariableEventQueue<13440, 16>                                              OutFineQueryQueue;             // BrnPhysicsToSceneQueueIO.h:41 (see FLAG above)
        typedef CgsModule::EventQueue<CgsPhysics::PhysicsSimulationIO::InChangeRigidBodyInertia, 200> InChangeRigidBodyInertiaQueue; // InputBuffer::InChangeRigidBodyInertiaQueue
        typedef CgsModule::EventQueue<InAddJoint, 10>                                                 AddArticulatedJointQueue;      // BrnVehicleQueues.h:39
        typedef CgsModule::EventQueue<InRemoveJoint, 10>                                              RemoveArticulatedJointQueue;   // BrnVehicleQueues.h:41

        // ⭐ ADDED 2026-08-09 (conductor wave) -- DWARF :204 / :201, both now X360-ATTESTED
        // by PhysicsModule::Update @0x825B0640, which inlines BOTH bodies:
        //   * Construct: CreateIOBuffer<VehicleManagerOutputBuffer> @0x8259DAF0 constructs
        //     the buffer's embedded interface (PS3 keeps VehicleManagerOutputBuffer::
        //     Construct out of line, which lands here). The PC stack template reaches this the
        //     same way -- CreateIOBuffer<T> runs T::Construct (2026-08-15).
        //   * Append: Update's tail drains the vehicle-manager request buffer into the
        //     module output buffer's request interface with exactly five queue appends
        //     (0x825B2480..0x825B24C0): InAddRigidBody<50>::Append @0x825A3898 (+0),
        //     InRemoveRigidBody<50>::Append @0x825A3988 (+9616), VariableEventQueue
        //     <13440,16>::Append<13440,16> @0x825AC068 (+10432), InAddJoint<10>::Append
        //     @0x825A3A68 (+39904), InRemoveJoint<10>::Append @0x825A3B58 (+41840).
        //     ⚠️ mChangeRigidBodyInertiaQueue (+23888) is NOT appended here -- that queue
        //     is drained separately by BridgeVehicleManagerToSimulation_PostPhysics
        //     @0x825ADF60 into the SIM input buffer. Reproduced as-is.
        // Bodied in BrnVehicleOutputInterface.cpp.
        void Construct();
        void Append(const VehicleOutputRequestInterface* lpSource);

        // @0x825E7170 (DWARF :236). Validate the event's three handles, then queue it.
        // Bodied in BrnVehicleOutputInterface.cpp -- out of line, exactly as the console emits it.
        void AddJoint(const InAddJoint& lAddJointEvent);

        // DWARF :241. The console has NO out-of-line symbol for this one: it is inlined into
        // ArticulatedJointPool::SendCreateRemoveJointEvents @0x826013C0, whose asm carries the
        // assert (SharedIO/BrnVehicleOutputInterface.h:730) and then the queue AddEvent at +41840.
        // Header-inline here, matching that.
        void RemoveJoint(const InRemoveJoint& lRemoveJointEvent)
        {
            CGS_ASSERT(lRemoveJointEvent.mu64Id != KU64_INVALID_JOINT_ID,
                       "!lRemoveJointEvent.mId.IsInvalid()");
            mRemoveJointQueue.AddEvent(lRemoveJointEvent);
        }

        // ⭐ ADDED 2026-08-06 (bridge de-facade wave): the two joint-queue read accessors
        // (DWARF :256 / :259). Now X360-ATTESTED, which is what had gated them out: the drain
        // PhysicsModule::BridgeVehicleManagerRequestsToSimulation @0x825AB968 names
        // GetAddJointQueue in its own assert string ("lpRequestInterface->GetAddJointQueue()->
        // GetLength() == 0", reading the miLength word at +0x9BE8 == mAddJointQueue+8) and then
        // feeds `this + 0xA370` == &mRemoveJointQueue to AppendRemoveJointQueue<10>. Both were
        // header-inline on the console (no out-of-line emissions); header-inline here.
        const AddArticulatedJointQueue*    GetAddJointQueue()    const { return &mAddJointQueue; }
        const RemoveArticulatedJointQueue* GetRemoveJointQueue() const { return &mRemoveJointQueue; }

        // ⭐ ADDED 2026-08-09 (conductor wave): three more of the DWARF's five Get*Queue
        // accessors, now X360-ATTESTED by BridgeVehicleManagerToSimulation_PostPhysics
        // @0x825ADF60, which reads exactly these three seats (+9616 / +0 / +23888) off the
        // request interface and feeds them to the sim Append*Queue drains.
        InRemoveRigidBodyQueue*        GetRemoveRigidBodyQueue()        { return &mRemoveRigidBodyQueue; }
        InAddRigidBodyQueue*           GetRequiredRigidBodiesQueue()    { return &mRequiredRigidBodiesQueue; }
        InChangeRigidBodyInertiaQueue* GetChangeRigidBodyInertiaQueue() { return &mChangeRigidBodyInertiaQueue; }

        // Never called; bodied in BrnVehicleOutputInterface.cpp (a MOUNTED TU) and nothing but
        // static_asserts. Static so it can see the private queue block through offsetof.
        static void _AssertLayout();

        // The invalid-JointId sentinel the two joint asserts compare against (X360 qword_82F2A3B0).
        // ⛔ THE REASON RECORDED HERE UNTIL 2026-08-04 WAS FALSE (corrected in task #141). It read:
        // "CgsPhysics::JointId is one of the names that is currently declared TWICE in this tree
        // (CgsPhysicsSimulationModule.h:102 vs the CgsRigidBody.h family), so pulling either header
        // in here would make an open ODR fork meet and fail to compile." CgsRigidBody.h declares
        // exactly one type -- RigidBodyId -- and no JointId at all, so there was never a JointId
        // fork to meet. (The RigidBodyId fork WAS real; it is retired, and the .cpp now includes
        // CgsRigidBody.h and uses the real K_INVALID_RIGID_BODY_ID.)
        // ⭐ This constant survives for a different and honest reason: `CgsPhysics::JointId`
        // (CgsPhysicsSimulationModule.h) carries no sentinel of its own anywhere in the tree, so
        // there is nothing to include. Give it a real home when a JointId consumer needs one.
        static const u64 KU64_INVALID_JOINT_ID = 0xFFFFFFFFFFFFFFFFull;

    private:
        InAddRigidBodyQueue           mRequiredRigidBodiesQueue;     // @0x0000  (DWARF :263)
        InRemoveRigidBodyQueue        mRemoveRigidBodyQueue;         // @0x2590  (DWARF :264)
        OutFineQueryQueue             mRequestFineLineQueue;         // @0x28C0  (DWARF :265)
        InChangeRigidBodyInertiaQueue mChangeRigidBodyInertiaQueue;  // @0x5D50  (DWARF :268)
        AddArticulatedJointQueue      mAddJointQueue;                // @0x9BE0  (DWARF :270) ASM-PINNED
        RemoveArticulatedJointQueue   mRemoveJointQueue;             // @0xA370  (DWARF :271) ASM-PINNED
    };
}
}
