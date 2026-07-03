#pragma once

// BrnTraffic::BrnTrafficIO IO buffers (TrafficEntityModule shared IO). Reconstructed from the
// DecFIGS DWARF (GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModuleIO.h)
// with member OFFSETS pinned by the X360 retail XEX.
//
// This header homes the traffic module's per-stage IO buffers: the InputBuffer_* buffers the
// world/physics/race-car bridges fill for the traffic module to drain, and the OutputBuffer_*
// buffers the traffic module produces. Members whose owning homes live in other TUs are embedded
// by-value using their real committed types where available, or as correctly-sized opaque stand-
// ins (flagged) sized to the X360 spans so the X360-pinned member offsets are exact.

#include "types.hpp"
#include "GameShared/GameClasses/Module/CgsIOBuffer.h"                                   // CgsModule::IOBuffer
#include "GameSource/World/EntityModules/TrafficEntityModule/SharedIO/BrnTrafficAIInterfaces.h" // TrafficAIInterface

// ---- InputBuffer_PreScene member/parameter type homes ----
#include "GameShared/GameClasses/System/Timer/CgsTimerStatusInterface.h"                 // CgsSystem::TimerStatusInterface
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h" // RCEntity{Active,Global}RaceCarOutputInterface, RCEntityPlayerResetInterface
#include "GameSource/World/EntityModules/TrafficEntityModule/SharedIO/BrnTrafficNetworkInterfaces.h"           // TrafficNetworkInputInterface

// ---- InputBuffer_PrePhysics member type homes ----
#include "GameShared/GameClasses/Module/CgsEventQueue.h"                                 // CgsModule::EventQueue<T,N>
#include "GameShared/GameClasses/SceneManager/SharedIO/CgsPotentialContact.h"            // SceneManagerIO::PotentialContact (80B)
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventOutOverlapPair.h"   // SceneManagerIO::OutOverlapPair (24B)

// ---- InputBuffer_PostScene / InputBuffer_PostPhysics member type homes ----
#include "GameSource/World/CrashModule/SharedIO/BrnCrashModuleTrafficIOInterfaces.h"     // BrnWorld::CrashIO::TrafficOutputInterface
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleOutputInterface.h"        // VehicleOutputInterface, VehicleManagerOutputInterface
#include "GameSource/Physics/DeformationManager/SharedIO/BrnDeformationOutputInterface.h" // DeformationOutputInterfaceForEntityModules
#include "GameSource/Physics/ContactSpies/BrnContactSpyInterface.h"                       // ContactSpyInterface

namespace BrnTraffic
{
namespace BrnTrafficIO
{
    // ============================================================================
    // InputBuffer_Dispatch  (ADDITIVE GROW: WorldBridgeWorldModuleToEntityModules_Render TU)
    // ============================================================================
    // The traffic-entity module's generate-dispatch-lists INPUT buffer
    // (WorldModule::BridgeWorldModuleToEntityModules_Render fills it via the four setters;
    // BrnTraffic::TrafficEntityModule::GenerateDispatchLists drains it via the four getters).
    // Mirrors the near-identical sibling BrnWorld::WorldEntityIO::InputBuffer_GenerateDispatchLists.
    //
    // LAYOUT (X360 accessor offsets, authoritative -- four contiguous 32-bit words at the tail):
    //   base    CgsModule::IOBuffer                     (1-byte status FlagSet8; +1.. pad)
    //   +0x8014 u32 muDispatchFrame                     dispatch-frame index
    //   +0x8018 u32 muBlobbyShadowBuffer                blobby-shadow buffer handle
    //   +0x801C u32 muCoronaSubmissionInterface         corona-submission interface handle
    //   +0x8020 u32 muShadowMap                         shadow-map handle
    //
    // FLAG (opaque interior): the corona/shadow/blobby fields are 32-bit handles of foreign
    // interfaces whose homes land elsewhere; the asm treats each as a single 32-bit word. The
    // dispatch-list payload from base up to +0x8014 is the buffer's other (unrecovered) contents
    // and is modelled as correctly-sized opaque storage so the four X360-pinned offsets are exact.
    class InputBuffer_Dispatch : public CgsModule::IOBuffer
    {
    public:
        // X360 0x827120D8: read-lock; return the dispatch-frame index (this+0x8014).
        u32  GetDispatchFrame() const;
        // X360 0x827A0EC0: write-lock; set the dispatch-frame index (this+0x8014).
        void SetDispatchFrame(u32 luDispatchFrame);
        // X360 0x82712188 (Hex-Rays "G"): read-lock; return the blobby-shadow buffer handle (this+0x8018).
        u32  GetBlobbyShadowBuffer() const;
        // X360 0x827A0F70: write-lock; set the blobby-shadow buffer handle (this+0x8018).
        void SetBlobbyShadowBuffer(u32 luBlobbyShadowBuffer);
        // X360 0x82712238 (Hex-Rays "GetCor"): read-lock; return the corona-submission interface handle (this+0x801C).
        u32  GetCoronaSubmissionInterface() const;
        // X360 0x827A1020: write-lock; set the corona-submission interface handle (this+0x801C).
        void SetCoronaSubmissionInterface(u32 luCoronaSubmissionInterface);
        // X360 0x827122E8: read-lock; return the shadow-map handle (this+0x8020).
        u32  GetShadowMap() const;
        // X360 0x827A10D0: write-lock; set the shadow-map handle (this+0x8020).
        void SetShadowMap(u32 luShadowMap);

        static void _AssertLayout();

    private:
        // The IOBuffer base is a single status byte; the four handle words sit contiguously at the
        // tail (+0x8014..+0x8020). The preceding dispatch-list payload is folded into correctly-sized
        // opaque storage: 0x8014 - 1 accounts for the base's 1-byte status (see FLAG).
        u8            maPayloadAndPad[0x8014 - 1];        // +0x0001..+0x8013 (status pad + payload)
        u32           muDispatchFrame;                    // +0x8014
        u32           muBlobbyShadowBuffer;               // +0x8018
        u32           muCoronaSubmissionInterface;        // +0x801C
        u32           muShadowMap;                        // +0x8020
    };

    // ============================================================================
    // InputBuffer_PreScene  (ADDITIVE GROW: full DWARF layout, BrnTrafficEntityModuleIO.h:143)
    // ============================================================================
    // The traffic module's pre-scene input buffer. Grown from the earlier minimal 2-setter slice
    // to the full DWARF layout with REAL committed member types, so the batch setter bodies get
    // exact X360 offsets. WorldModule::BridgeInputToEntityModules fills the timer + network members;
    // WorldModule::BridgeRaceCarModuleToTrafficModule_PreScene fills the two race-car interfaces.
    class InputBuffer_PreScene : public CgsModule::IOBuffer
    {
    public:
        typedef BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface ActiveRaceCarOutputInterface;   // DWARF :85
        typedef BrnWorld::RaceCarEntityModuleIO::RCEntityGlobalRaceCarOutputInterface GlobalRaceCarOutputInterface;   // DWARF :86

        // X360 0x82710B30 -- read-lock; returns &mTimerStatusInterface (this + 4).
        const CgsSystem::TimerStatusInterface* GetTimerStatusInterface() const;                                       // DWARF :150
        // X360 0x8279FAD8 -- write-lock; field-copies the source timer status into +4 (operator=).
        void SetTimerStatusInterface(const CgsSystem::TimerStatusInterface* lpTimerStatusInterface);                  // DWARF :151
        // X360 0x8279FBE8 -- write-lock; flat 10480B member copy into +64.
        void SetActiveRaceCarOutputInterface(const ActiveRaceCarOutputInterface* lpInterface);                        // DWARF :154
        // X360 0x8279FCA0 -- write-lock; flat 2416B member copy into +10544.
        void SetGlobalRaceCarOutputInterface(const GlobalRaceCarOutputInterface* lpInterface);                        // DWARF :157
        // X360 0x827ACD28 -- write-lock; clear+append the hull queue, copy mbDiverged into +12960.
        void SetTrafficNetworkInputInterface(const TrafficNetworkInputInterface* lpTrafficNetworkInputInterface);     // DWARF :160

        static void _AssertLayout();

    private:
        CgsSystem::TimerStatusInterface mTimerStatusInterface;          // DWARF :167  @ +4     (48B)
        ActiveRaceCarOutputInterface    mActiveRaceCarOutputInterface;  // DWARF :168  @ +64    (10480B)
        GlobalRaceCarOutputInterface    mGlobalRaceCarOutputInterface;  // DWARF :169  @ +10544 (2416B)
        TrafficNetworkInputInterface    mTrafficNetworkInputInterface;  // DWARF :170  @ +12960
        f32                             mfTimeOfDay_Seconds;            // DWARF :171
    };

    // ============================================================================
    // InputBuffer_PostScene  (ADDITIVE GROW: DWARF BrnTrafficEntityModuleIO.h:221)
    // ============================================================================
    // The traffic module's post-scene input buffer. WorldModule::BridgeCrashModuleToTrafficModule_
    // PostScene fills the crash-traffic output interface; BridgeRaceCarModuleToTrafficModule_PreScene
    // fills the active-race-car interface.
    //
    // X360-pinned offsets:
    //   mCrashTrafficOutputInterface  (TrafficOutputInterface)               @ 8     (8-aligned after status)
    //   mActiveRaceCarOutputInterface (RCEntityActiveRaceCarOutputInterface) @ 1648  (0x670; 16-aligned)
    //   mRaceCarToTrafficInterface    (RaceCarToTrafficInterface)            follows
    //
    // FLAG (opaque interior): RaceCarToTrafficInterface's concrete home is not yet in-tree, so it is
    // modelled as a 1-byte opaque placeholder (this batch never touches its interior; the two pinned
    // offsets that matter -- @8 and @1648 -- are both asserted in the .cpp bodies). Adopt the named
    // type additively when its home lands.
    class InputBuffer_PostScene : public CgsModule::IOBuffer
    {
    public:
        typedef BrnWorld::CrashIO::TrafficOutputInterface                             CrashTrafficOutputInterface; // DWARF :250
        typedef BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface ActiveRaceCarOutputInterface; // :240
        // RaceCarToTrafficInterface home not yet reconstructed -- opaque placeholder (see FLAG).
        struct RaceCarToTrafficInterface { u8 muDUMMY; };                                                          // :241

        const CrashTrafficOutputInterface* GetCrashTrafficOutputInterface() const;     // :228
        void SetCrashTrafficOutputInterface(const CrashTrafficOutputInterface*);        // :229 (0x827ACDE8)
        const ActiveRaceCarOutputInterface* GetActiveRaceCarOutputInterface() const;   // :231
        void SetActiveRaceCarOutputInterface(const ActiveRaceCarOutputInterface*);      // :232 (0x8279FEA8)

    private:
        CrashTrafficOutputInterface   mCrashTrafficOutputInterface;    // :239 @8
        ActiveRaceCarOutputInterface  mActiveRaceCarOutputInterface;   // :240 @1648 (0x670)
        RaceCarToTrafficInterface     mRaceCarToTrafficInterface;      // :241
    };

    // ============================================================================
    // InputBuffer_PrePhysics  (ADDITIVE GROW: DWARF BrnTrafficEntityModuleIO.h:330)
    // ============================================================================
    // The traffic module's pre-physics input buffer. Member SEQUENCE + NAMES from the DecFIGS DWARF
    // (:330-364); byte OFFSETS pinned by the three X360 setter bodies (0x827A9DE0 / 0x827A9E98 /
    // 0x827A0158):
    //   +0        IOBuffer status flag (1-byte FlagSet base)
    //   +16       mPotentialContactQueue  EventQueue<PotentialContact,2048>  (addi this,0x10)
    //   +163872   mOverlapPairsQueue      EventQueue<OutOverlapPair,128>     (this+0x28020)
    //   +166960   mSceneResultQueue       OutSceneQueryResultsQueue<32768> (not set here; opaque pad)
    //   +199744   mPlayerResetInterface   RCEntityPlayerResetInterface (32B) (this+0x30C40)
    //   +199776   mPropToTrafficInterface PropToTrafficInterface (not set here; opaque placeholder)
    class InputBuffer_PrePhysics : public CgsModule::IOBuffer
    {
    public:
        typedef CgsModule::EventQueue<CgsSceneManager::SceneManagerIO::PotentialContact, 2048> PotentialContactQueue; // :91
        typedef CgsModule::EventQueue<CgsSceneManager::SceneManagerIO::OutOverlapPair, 128>    OverlapPairsQueue;     // :92
        typedef BrnWorld::RaceCarEntityModuleIO::RCEntityPlayerResetInterface                  RCEntityPlayerResetInterface; // :104

        const PotentialContactQueue* GetPotentialContactQueue() const;                           // :277
        void  SetPotentialContactQueue(const PotentialContactQueue* lpPotentialContactQueue);    // :278 W (0x827A9DE0)

        const OverlapPairsQueue* GetOverlapPairsQueue() const;                                   // :280
        void  SetOverlapPairsQueue(const OverlapPairsQueue* lpOverlapPairsQueue);                // :281 W (0x827A9E98)

        const RCEntityPlayerResetInterface* GetPlayerResetInterface() const;                     // :283
        void  SetPlayerResetInterface(const RCEntityPlayerResetInterface* lpPlayerResetInterface); // :284 W (0x827A0158)

        static void _AssertLayout();

    private:
        // The mSceneResultQueue / mPropToTrafficInterface members this batch does not touch are
        // modelled as correctly-sized opaque stand-ins so the pinned member offsets are exact.
        PotentialContactQueue        mPotentialContactQueue;                                     // :294 (offset 16)
        OverlapPairsQueue            mOverlapPairsQueue;                                          // :295 (offset 163872)
        unsigned char                maSceneResultQueue[199744 - 166960];                        // :296 (offset 166960, 32784B; OutSceneQueryResultsQueue<32768>, opaque)
        RCEntityPlayerResetInterface mPlayerResetInterface;                                       // :298 (offset 199744, 32B)
        unsigned char                maPropToTrafficInterface[1];                                 // :299 (offset 199776; PropToTrafficInterface, opaque placeholder)
    };

    // ============================================================================
    // InputBuffer_PostPhysics  (ADDITIVE GROW: DWARF BrnTrafficEntityModuleIO.h:459)
    // ============================================================================
    // The traffic module's post-physics input buffer. The physics/race-car bridges publish per-
    // frame snapshots via WorldModule::BridgePhysicsModuleToTrafficModule_PostPhysics (and
    // BridgeRaceCarModuleToTrafficModule_PreScene for the active-race-car member), calling the five
    // write-lock setters below (all real out-of-line X360 symbols). Each tests the IOBuffer write-
    // lock bit, fires the non-gating "Not locked for writing" assert, then copies the source into
    // the matching embedded member.
    //
    // MEMBER LAYOUT authoritative from the DecFIGS DWARF (:459-499, NAMES all members); the X360
    // accessor/store offsets pin their console positions:
    //   +0x10    (16)     mVehicleOutputInterface                     (:370)
    //   +0xEC30  (60464)  mVehicleManagerOutputInterface              (:372)
    //   +0x128C0 (75968)  mActiveRaceCarOutputInterface               (:374; X360 XMemCpy 0x28F0 == 10480)
    //   +0x151B0 (86448)  mDeformationOutputInterfaceForEntityModules (:375)
    //   +0x19EF0 (106224) mContactSpyInterface                        (:376; single-word copy)
    //
    // Offsets are NOT static_asserted: the SIMD/queue aggregates widen on the 64-bit host so their
    // host sizes do not reproduce the 32-bit X360 offsets (same reason the sibling RaceCar
    // InputBuffer_PostPhysics pins no member offsets). The two members this batch does not access
    // (mSceneResultQueue, mGameActionQueue) are modelled as correctly-sized opaque-by-value stand-
    // ins sized to the X360 spans. Adopt the named queue types additively when their homes land.
    class InputBuffer_PostPhysics : public CgsModule::IOBuffer
    {
    public:
        typedef BrnPhysics::Vehicle::VehicleOutputInterface        VehicleOutputInterface;        // :82
        typedef BrnPhysics::Vehicle::VehicleManagerOutputInterface VehicleManagerOutputInterface; // :83
        typedef BrnPhysics::Deformation::DeformationOutputInterfaceForEntityModules DeformationOutputInterfaceForEntityModules; // :106
        typedef BrnPhysics::ContactSpy::ContactSpyInterface        ContactSpyInterface;           // :108
        // :374 spells the member type as InputBuffer_PreScene::ActiveRaceCarOutputInterface, itself a
        // typedef for RCEntityActiveRaceCarOutputInterface (DWARF :144). Use the real type directly.
        typedef BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface ActiveRaceCarOutputInterface;

        // X360 0x827A9F50 (:347): write-lock; mVehicleOutputInterface = *src.
        void SetVehicleOutputInterface(const VehicleOutputInterface* lpVehicleOutputInterface);
        // X360 0x827AA000 (:353): write-lock; mVehicleManagerOutputInterface = *src.
        void SetVehicleManagerOutputInterface(const VehicleManagerOutputInterface* lpVehicleManagerOutputInterface);
        // X360 0x827A06C0 (:359): write-lock; mActiveRaceCarOutputInterface = *src (X360 XMemCpy 0x28F0 == 10480).
        void SetActiveRaceCarOutputInterface(const ActiveRaceCarOutputInterface* lpActiveRaceCarOutputInterface);
        // X360 0x827AA0B8 (:362): write-lock; mDeformationOutputInterfaceForEntityModules = *src.
        void SetDeformationOutputInterfaceForEntityModules(const DeformationOutputInterfaceForEntityModules* lpDeformationOutputInterface);
        // X360 0x827A0778 (:366): write-lock; mContactSpyInterface = *src (single-word copy).
        void SetContactSpyInterface(const ContactSpyInterface* lpContactSpyInterface);

    private:
        // Opaque-by-value stand-ins for the two members this batch does not touch, sized to the
        // X360 spans between the pinned real members (host offsets are not asserted).
        struct SceneResultQueueStorage { unsigned char maReserved[32784]; }; // :371 span +0x10+sizeof(VOI) .. +0xEC30
        struct GameActionQueueStorage  { unsigned char maReserved[13328]; }; // :373 span +0xEC30+sizeof(VMOI) .. +0x128C0

        VehicleOutputInterface                     mVehicleOutputInterface;                       // :370  X360 +0x10
        SceneResultQueueStorage                    mSceneResultQueue;                             // :371
        VehicleManagerOutputInterface              mVehicleManagerOutputInterface;                // :372  X360 +0xEC30
        GameActionQueueStorage                     mGameActionQueue;                              // :373
        ActiveRaceCarOutputInterface               mActiveRaceCarOutputInterface;                 // :374  X360 +0x128C0 (10480 B)
        DeformationOutputInterfaceForEntityModules mDeformationOutputInterfaceForEntityModules;   // :375  X360 +0x151B0
        ContactSpyInterface                        mContactSpyInterface;                          // :376  X360 +0x19EF0
    };

    // ============================================================================
    // OutputBuffer_PostScene  (DWARF :291; X360 Construct @ 0x82761830)
    // ============================================================================
    // X360 member offsets (from Construct @ 0x82761830 store displacements):
    //   +0      IOBuffer status flag (*a1 = 1)
    //   +4      mSceneCoarseQueryQueue  (VariableEventQueue<16384,16>::Construct(a1+4))
    //   +16416  mTrafficAIInterface     (GetTrafficAIInterface returns a1+16416; count zeroed)
    //   +61488  mTrafficToRaceCarInterface_PostScene (this trailing interface is the post-scene
    //           traffic->race-car interface)
    //
    // mSceneCoarseQueryQueue is the SceneManager coarse-query input queue
    // (InputBuffer_Query::InSmCoarseQueryQueue == InCoarseQueryQueue<16384>, a
    // VariableEventQueue<16384,16> subclass that adds NO data members). The X360 places it at
    // offset 4 (4-aligned, right after the 1-byte IOBuffer status), so it is modelled as a
    // 4-ALIGNED 16400-byte sized blob -- NOT the alignas(16) SceneCoarseQueryQueue slice, which
    // would force it to offset 16. mTrafficAIInterface (alignas 16) then lands at 16416 (16400
    // queue ends at 16404, padded up to the next 16-boundary), matching the X360.
    struct OutputBuffer_PostScene : public CgsModule::IOBuffer
    {
        // 4-aligned sized blob for the coarse-query queue (sizeof(VariableEventQueue<16384,16>)
        // == 1 + 16384 + 12 -> round to 4 == 16400). The full queue layout/methods belong to
        // the SceneCoarseQueryQueue TU; this buffer only takes &mSceneCoarseQueryQueue.
        struct SceneCoarseQueryQueue { unsigned char maReserved[16400]; };

        // DWARF :187 -- the trailing traffic->race-car post-scene interface. The DWARF spells
        // it as a 1-byte placeholder (muDUMMY); the X360 zeroes it in Construct. Modelled as the
        // DWARF 1-byte struct (this TU only takes its address, never its interior).
        struct TrafficToRaceCarInterface_PostScene { u8 muDUMMY; };

        void Construct();                                                                  // :249
        const SceneCoarseQueryQueue* GetSceneCoarseQueryQueue() const;                     // :252
        SceneCoarseQueryQueue*       GetSceneCoarseQueryQueue();                            // :253
        const TrafficAIInterface*    GetTrafficAIInterface() const;                        // :255
        TrafficAIInterface*          GetTrafficAIInterface();                              // :256 W (0x827111C0)
        const TrafficToRaceCarInterface_PostScene* GetTrafficToRaceCarInterface_PostScene() const; // :258
        TrafficToRaceCarInterface_PostScene*       GetTrafficToRaceCarInterface_PostScene();        // :259

    private:
        SceneCoarseQueryQueue               mSceneCoarseQueryQueue;                // :263 (offset 4)
        TrafficAIInterface                  mTrafficAIInterface;                   // :264 (offset 16416)
        TrafficToRaceCarInterface_PostScene mTrafficToRaceCarInterface_PostScene;  // :265
    };

    // ============================================================================
    // OutputBuffer_PostPhysics  (DWARF BrnTrafficEntityModuleIO.h, post-physics output buffer)
    // ============================================================================
    // ADDITIVE GROW: this slice homes the post-physics output buffer's four X360-emitted
    // lock-guarded handle accessors (the producer/consumer is TrafficEntityModule::
    // PostPhysicsUpdate / ::GenerateVehicleCrashedEvents, drained by WorldModule::
    // BridgeEntityModulesToOutput_PostPhysics):
    //
    //   <write member @ +8>      @ 0x82711A48  write-lock (bit 3) -> this + 8       (asm-line 392)
    //   <write member @ +834784> @ 0x82711D90  write-lock (bit 3) -> this + 834784  (asm-line 407)
    //   <read  handle  @ +834828>@ 0x827A0E18  read-lock  (bit 4) -> this + 834828  (asm-line 418)
    //   <write handle  @ +834828>@ 0x82712030  write-lock (bit 3) -> this + 834828  (asm-line 419)
    //
    // The const (read) handle tests the read-lock bit (((*a1 >> 4) & 1), `extrwi r11,r11,1,27`);
    // the three non-const (write) handles test the write-lock bit (((*a1 >> 3) & 1),
    // `extrwi r11,r11,1,28`) -- matching CgsModule::IOBuffer's IsBufferLockedForReading()/
    // IsBufferLockedForWriting(). The X360 asserts the lock state (streaming "Not locked for
    // reading/writing\n", a non-gating tripwire at asm-lines 392/407/418/419), then returns the
    // member address via `addi this,8` (+8) / `addis this,0xD; addi this,-0x4320` (+834784) /
    // `addis this,0xD; addi this,-0x42F4` (+834828).
    //
    // The +834828 read-lock and write-lock handles return the SAME member (the const/non-const
    // overload pair, adjacent asm-lines 418/419 in the same header). The +8 and +834784 write-lock
    // handles return two distinct earlier members.
    //
    // FLAG (truncated names / foreign types / opaque interior): the Hex-Rays method names were
    // truncated (G / Ge / GetGuiE / GetR) and not fully recoverable; the lock bit + return offset
    // pin each accessor's identity, so they are named here by offset role with the recoverable name
    // stems documented. The returned members are foreign interface types with their own owning
    // homes elsewhere and are NOT reconstructed here; the storage up to each pinned offset is
    // modelled as correctly-sized opaque storage so the three pinned return offsets (+8, +834784,
    // +834828) are exact. Adopt the named interface types additively when their homes land.
    class OutputBuffer_PostPhysics : public CgsModule::IOBuffer
    {
    public:
        // Opaque foreign-type storages (see FLAG above).
        struct InterfaceAt8Storage      { unsigned char maBytes[1]; };
        struct InterfaceAt834784Storage { unsigned char maBytes[1]; };
        struct InterfaceAt834828Storage { unsigned char maBytes[1]; };

        // X360 0x82711A48 (asm-line 392): write-lock handle, returns this + 8.
        InterfaceAt8Storage*      GetWriteInterfaceAt8();
        // X360 0x82711D90 (asm-line 407): write-lock handle, returns this + 834784. (name stem "GetR...")
        InterfaceAt834784Storage* GetWriteInterfaceAt834784();
        // X360 0x827A0E18 (asm-line 418): read-lock handle, returns this + 834828.
        const InterfaceAt834828Storage* GetReadInterfaceAt834828() const;
        // X360 0x82712030 (asm-line 419): write-lock handle, returns this + 834828. (name stem "GetGui...")
        InterfaceAt834828Storage* GetWriteInterfaceAt834828();

        static void _AssertLayout();

    private:
        // The IOBuffer base subobject's 1-byte status sits at +0; the first handle's member is at
        // +8, so 7 bytes follow the status (the X360 places this member 4-aligned/8-aligned right
        // after the status, matching `addi this,8`). The members between the pinned offsets are
        // folded into correctly-sized opaque storage (see FLAG).
        u8                        maStatusPad[7];                          // +1..+7 (force +8 placement)
        InterfaceAt8Storage       mInterfaceAt8;                           // +8
        unsigned char             maPad8To834784[834784 - 8 - 1];          // span +9..+834783
        InterfaceAt834784Storage  mInterfaceAt834784;                      // +834784
        unsigned char             maPad834784To834828[834828 - 834784 - 1];// span up to +834828
        InterfaceAt834828Storage  mInterfaceAt834828;                      // +834828
    };
}
}
