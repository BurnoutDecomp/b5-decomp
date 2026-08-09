// ============================================================================
// b5-decomp/src/GameSource/Physics/BrnPhysicsModuleIO.h
//
// Canonical (DWARF) home for BrnPhysics::PhysicsModuleIO::{OutputBuffer, InputBuffer}
// (BrnPhysicsModuleIO.h). Grown across waves to cover the X360-emitted accessors of both
// IO buffers.
//
// OutputBuffer -- MINIMAL-COMPLETE slice covering the OutputBuffer's X360-emitted accessors:
//   GetVehicleOutputRequestInterface() @ 0x8259FF30 write (bit 3) -> +16     (DWARF :349)
//   GetVehicleOutputInterface() const  @ 0x8279F598 read  (bit 4) -> +44128  (DWARF :354)
//   GetVehicleOutputInterface()        @ 0x825A0080 write (bit 3) -> +44128  (DWARF :355)
//   GetPropManagerOutputInterface() const @ 0x8279F640 read (bit 4) -> +71792 (DWARF :357)
//   GetPropManagerOutputInterface()    @ 0x825C0DC8 write (bit 3) -> +71792  (DWARF :358)
//   GetDeformationOutputInterface()    @ 0x825A0128 write (bit 3) -> +148656 (DWARF :361)
//   GetContactSpyInterface()           @ 0x825A0320 write (bit 3) -> +998192 (DWARF :370)
//   GetSceneInputInterface() const     @ 0x8279F838 read  (bit 4) -> +179424 (DWARF :366)
//   [wave5 ADDITIVE] const GetVehicleOutputRequestInterface() @ 0x8279F448 read -> +16     (:298)
//   [wave5 ADDITIVE] const GetDeformationOutputInterface()    @ 0x8279F6E8 read -> +148656 (:322)
//   [wave5 ADDITIVE] non-const GetSceneInputInterface()       @ 0x825A0278 write -> +179424 (:337)
//
// LAYOUT (DWARF :260 member order + X360 getter return-offsets, authoritative):
//   base   CgsModule::IOBuffer                                  (1-byte status; +1..+15 pad)
//   +16     VehicleOutputRequestInterface mVehicleOutputRequestInterface         :376
//   +...    VehicleManagerOutputInterface mVehicleManagerOutputInterface         :378
//   +44128  VehicleOutputInterface        mVehicleOutputInterface                :379
//   +71792  PropOutputInterface           mPropManagerOutputInterface            :381
//   +148656 DeformationOutputInterface    mDeformationOutputInterface            :383
//   +...    DeformationOutputInterfaceForEntityModules mDeformationOutputInterfaceForEntityModules :384
//   +179424 SceneInputInterface           mSceneInputInterface                   :385
//   +998192 ContactSpyInterface           mContactSpyInterface                   :386
//
// FLAG (foreign types): every interface member here has its own owning home elsewhere
// and is NOT reconstructed in this slice. The X360-pinned return offsets are made exact via
// correctly-sized opaque storage; the intervening members whose offsets are not separately
// X360-attested are folded into the padding (named in comments). Adopt the named interface
// types additively when their homes land.
#pragma once

#include "types.hpp"

#include "GameShared/GameClasses/Module/CgsIOBuffer.h"  // CgsModule::IOBuffer
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"  // CgsModule::VariableEventQueue<13312,16> (mGameActionQueue)
#include "GameShared/GameClasses/System/Timer/CgsTimerStatusInterface.h" // CgsSystem::TimerStatusInterface (mTimerInterface, retyped 2026-08-09)
#include "GameShared/GameClasses/Module/CgsEventQueue.h"                 // CgsModule::EventQueue<T,N> (the two unfolded input queues, 2026-08-09)
#include "GameShared/GameClasses/SceneManager/SharedIO/CgsPotentialContact.h"          // SceneManagerIO::PotentialContact (mPotentialContactQueue element)
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventOutOverlapPair.h" // SceneManagerIO::OutOverlapPair (mOverlapPairsQueue element)
#include "GameSource/Physics/ContactSpies/BrnContactSpyInterface.h" // ContactSpy::ContactSpyInterface (real member @ +998192, promoted 2026-08-06)
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleInputInterface.h" // Vehicle::VehicleInputInterface (mVehicleInputInterface, retyped 2026-08-06)
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleOutputInterface.h" // Vehicle::VehicleOutputRequestInterface / VehicleManagerOutputInterface / VehicleOutputInterface (promoted 2026-08-09)

namespace BrnPhysics
{
namespace PhysicsModuleIO
{
    class OutputBuffer : public CgsModule::IOBuffer
    {
    public:
        // Opaque foreign-type storages (see FLAG above). Each is a 1-byte placeholder; the
        // member offsets are pinned by the explicit padding arrays in the private section.
        // ⭐ PROMOTED 2026-08-09 (conductor wave): the first THREE seats are the REAL committed
        // types now (BrnVehicleOutputInterface.h) -- PhysicsModule::Update @0x825B0640 hands
        // +16 to the request-queue Append run, +41952 (the previously PAD-BURIED
        // mVehicleManagerOutputInterface, accessor 0x8259FFD8) to DoCrashPrediction /
        // UpdateDrivers / ProcessContactSpies / ProcessResetEvents, and +44128 to
        // UpdateVehiclePhysics -- each callee declared over the real type. All three are
        // pointer-free queue/POD bundles whose host sizeof equals the console span
        // (41936 / 2176-aligned / 27664), so every downstream pin below is UNCHANGED --
        // and _AssertLayout still gates exactly that.
        // The typedefs keep the pre-promotion accessor signatures compiling.
        typedef Vehicle::VehicleOutputRequestInterface VehicleOutputRequestInterfaceStorage;
        typedef Vehicle::VehicleOutputInterface        VehicleOutputInterfaceStorage;
        struct PropOutputInterfaceStorage           { unsigned char maBytes[1]; };
        struct DeformationOutputInterfaceStorage    { unsigned char maBytes[1]; };
        struct DeformationOutputInterfaceForEntityModulesStorage { unsigned char maBytes[1]; }; // :384 (unfolded 2026-08-09)
        struct SceneInputInterfaceStorage           { unsigned char maBytes[1]; };
        // mContactSpyInterface PROMOTED 2026-08-06 (bridge de-facade wave): the real
        // ContactSpy::ContactSpyInterface (one ContactSpyData* + SetData/IsEmpty), consumed by
        // PhysicsModule::BridgeSimulationToOutput @0x825B0448. Its +998192 seat is 8-aligned,
        // so the pin below is unchanged.

        // ---- accessors owned/bodied by this group --------------------------------------
        VehicleOutputRequestInterfaceStorage*       GetVehicleOutputRequestInterface();       // +16,     write
        const VehicleOutputInterfaceStorage*        GetVehicleOutputInterface() const;        // +44128,  read
        VehicleOutputInterfaceStorage*              GetVehicleOutputInterface();              // +44128,  write
        const PropOutputInterfaceStorage*           GetPropManagerOutputInterface() const;    // +71792,  read
        PropOutputInterfaceStorage*                 GetPropManagerOutputInterface();          // +71792,  write
        DeformationOutputInterfaceStorage*          GetDeformationOutputInterface();          // +148656, write
        ContactSpy::ContactSpyInterface*            GetContactSpyInterface();                 // +998192, write (retyped with the promotion)
        // ADDITIVE GROW (BridgePhysicsSceneUpdateToScene @0x827ABAA8): the scene-update
        // sub-interface (DWARF :385), read-locked. @0x8279F838 -> +179424 -- this pins
        // the previously-unpinned mSceneInputInterface offset.
        const SceneInputInterfaceStorage*           GetSceneInputInterface() const;           // +179424, read

        // ---- ADDITIVE this wave (wave5 IO family) --------------------------------------
        const VehicleOutputRequestInterfaceStorage* GetVehicleOutputRequestInterface() const; // +16,     read  (0x8279F448, DWARF :298)
        const DeformationOutputInterfaceStorage*    GetDeformationOutputInterface() const;     // +148656, read  (0x8279F6E8, DWARF :322)
        SceneInputInterfaceStorage*                 GetSceneInputInterface();                  // +179424, write (0x825A0278, DWARF :337)

        // ---- ADDITIVE 2026-08-09 (conductor wave) --------------------------------------
        // The manager-output accessor pair (DWARF :351/:352), X360 @0x8279F4F0(const-read
        // sibling block) / @0x8259FFD8 (write; the accessor PhysicsModule::Update calls
        // five times). Returns the :378 member the pre-promotion layout buried in padding.
        const Vehicle::VehicleManagerOutputInterface* GetVehicleManagerOutputInterface() const; // +41952, read
        Vehicle::VehicleManagerOutputInterface*       GetVehicleManagerOutputInterface();       // +41952, write (0x8259FFD8)
        DeformationOutputInterfaceForEntityModulesStorage*
                                                      GetDeformationOutputInterfaceForEntityModules(); // +159648, write (0x825A01D0, DWARF :364)

        static void _AssertLayout();

    private:
        u8                                   maStatusPad[15];                 // +1..+15 (force +16)
        VehicleOutputRequestInterfaceStorage mVehicleOutputRequestInterface;  // +16     :376 (real type, 41936 both targets -> ends 41952)
        // ⚠ HOST GROWTH from here down (the InputBuffer precedent): the manager/vehicle
        // interfaces contain event queues whose CONSOLE header is 12 bytes (4+4+4) where the
        // host's is 16 (8-byte mpEvents), so their host sizeof EXCEEDS the console span and
        // every later member sits at console+drift. The trailing comments keep the CONSOLE
        // offsets as documentation; _AssertLayout gates the console DELTAS between the
        // pad-separated seats (relative gates -- the "parity by NAMED MEMBERS" rule).
        Vehicle::VehicleManagerOutputInterface mVehicleManagerOutputInterface; // console +41952 :378 (console span 2176)
        VehicleOutputInterfaceStorage        mVehicleOutputInterface;          // console +44128 :379 (console span 27664)
        PropOutputInterfaceStorage           mPropManagerOutputInterface;      // console +71792 :381
        // gap to mDeformationOutputInterface: +71793 .. +148656.
        unsigned char                        maDeformationPad[148656 - 71793]; // ...
        DeformationOutputInterfaceStorage    mDeformationOutputInterface;      // +148656 :383
        // gap to mDeformationOutputInterfaceForEntityModules: +148657 .. +159647.
        unsigned char                        maDeformOutPad[159648 - 148657];  //
        // ⭐ 2026-08-09 (conductor wave): unfolded -- its own accessor @0x825A01D0 returns
        // `addis 2; addi 28576` == +159648 (148656 + the 10992-byte deformation output span,
        // zero slack).
        DeformationOutputInterfaceForEntityModulesStorage
                                             mDeformationOutputInterfaceForEntityModules; // +159648 :384
        // gap to mSceneInputInterface: +159649 .. +179423.
        unsigned char                        maEntityModulesPad[179424 - 159649];     // ...
        SceneInputInterfaceStorage           mSceneInputInterface;             // +179424 :385 (0x8279F838)
        // gap to mContactSpyInterface: +179425 .. +998191.
        unsigned char                        maScenePad[998192 - 179425];      // ...

        ContactSpy::ContactSpyInterface      mContactSpyInterface;             // +998192 :386 (real type; seat 8-aligned)
    };

    // ================================================================================
    // BrnPhysics::PhysicsModuleIO::InputBuffer (DWARF BrnPhysicsModuleIO.h:259) -- the
    // physics module's per-frame input buffer. This unified block MERGES two waves:
    //   * the InputBuffer IO-family slice (camera / vehicle-input / RC-entity-output /
    //     timer / solver-max-iterations / prop-manager accessors), AND
    //   * the bare PhysicsModuleIO group's InputBuffer members
    //     (mVehicleDriverInterface :312, mVehicleEffectsInputInterface :313,
    //     mGameActionQueue :323) with their four accessors.
    //
    // STYLE: mirrors the OutputBuffer sibling above -- NAMED members (DWARF names + offsets,
    // authoritative from references/DecFIGS/dwarfdump/GameSource/Physics/BrnPhysicsModuleIO.h)
    // + explicit padding pinning the X360-attested offsets + _AssertLayout() static_asserts.
    //
    // DWARF member order (:259, all private):
    //   Camera mCameraInput                          :309  +0x00010 (16)      copied extent 0x160
    //   VehicleInputInterface mVehicleInputInterface :311  +0x00170 (368)
    //   VehicleDriverInputInterface mVehicleDriverInterface :312 +0x22CD0 (142544)
    //   VehicleEffectsInputInterface mVehicleEffectsInputInterface :313 +0x24180 (147840)
    //   RCEntityOutputInterface mRCEntityOutputInterface :314 +0x24880 (149632)  copy 0x28F0
    //   CreateWorldEventQueue mCreateWorldEventQueue :316  (folded)
    //   InPotentialContactQueue mPotentialContactQueue :317 (folded)
    //   InOverlapPairsQueue mOverlapPairsQueue       :318  (folded)
    //   TimerStatusInterface mTimerInterface         :319  +0x4FDF0 (327152)   copy 0x30
    //   uint32_t mSolverMaxIterations                :320  +0x4FE20 (327200)
    //   PropInputInterface mPropManagerInputInterface:322  +0x4FE30 (327216)
    //   GameActionQueue mGameActionQueue             :323  +0x52A40 (338496)
    //
    // Lock bit per asm + CgsIOBuffer.h (eStatusLockedForWrite=0x08 bit3, eStatusLockedForRead=0x10
    // bit4): read-lock (status>>4 &1) => IsBufferLockedForReading() "Not locked for reading\n";
    // write-lock (status>>3 &1) => IsBufferLockedForWriting() "Not locked for writing\n". BOTH
    // strings carry the trailing \n per X360 rodata. Some Get* (mutable overloads) test the WRITE
    // bit -- reproduced as-is.
    //
    // FLAG (foreign types): every interface sub-object has its own owning home; kept as
    // correctly-SIZED opaque *Storage. mCameraInput is sized 0x160 (the copied Camera extent),
    // NOT the 0x170 CgsGraphics::Camera -- 0x10 + 0x160 == 0x170 == next member start. Adopt the
    // real member types additively when their homes land.
    //
    // NOTE: GetVehicleInputInterface() is the DWARF-authoritative name for the +368 accessor
    // (X360 0x8279ED28) previously provisionally called GetVehicle(); the one committed consumer
    // (WorldBridgeCrashToEntityModules.cpp) was updated to the DWARF name.
    // ================================================================================
    class InputBuffer : public CgsModule::IOBuffer
    {
    public:
        // Opaque foreign-type storages, sized to their X360-attested copied/embedded extent.
        struct CameraStorage                        { unsigned char maBytes[0x160]; };  // 352
        // ⭐ RETYPED 2026-08-06 (big-five #2, contact-generation wave; GameActionQueueStorage
        // precedent -- typedef keeps the four accessors' signatures). The vehicle-input span is
        // the REAL committed BrnPhysics::Vehicle::VehicleInputInterface now: the bridge driver
        // BridgeContactsToSimulation @0x825A9A64 reads the tri-cache interface out of it
        // (GetVehicleInputInterface() @0x8259F8A0 + the inlined `+128016` ==
        // mTriangleCacheInterface, PS3 lpInputBuffer+128384), which a 1-byte opaque span cannot
        // express. The pad after the member absorbs the host-size difference (compile fails if
        // the real host type ever outgrows the console span -- the intended tripwire).
        typedef BrnPhysics::Vehicle::VehicleInputInterface VehicleInputInterfaceStorage;
        struct VehicleDriverInputInterfaceStorage   { unsigned char maBytes[1]; };
        struct VehicleEffectsInputInterfaceStorage  { unsigned char maBytes[1]; };
        struct RCEntityOutputInterfaceStorage       { unsigned char maBytes[0x28F0]; }; // 10480
        // ⭐ RETYPED 2026-08-09 (conductor wave; same typedef pattern as the vehicle-input
        // span): the real CgsSystem::TimerStatusInterface -- 48 bytes on both targets (two
        // pointer-free 24-byte TimerStatus blocks), so the seat and every pin below are
        // unchanged. PhysicsModule::Update @0x825B0640 reads both sub-statuses through it
        // (sim step = [+32]*[+28], game step = [+8]*[+4], sim Time = [+40..47]).
        typedef CgsSystem::TimerStatusInterface     TimerStatusInterfaceStorage;         // 48
        struct PropInputInterfaceStorage            { unsigned char maBytes[1]; };
        // ⛔⛔ CORRECTED 2026-08-01 (BridgeGameStateToWorld wave) -- THIS WAS A ONE-BYTE MEMBER
        // THAT A COMMITTED BRIDGE WRITES A 13328-BYTE QUEUE INTO.
        // WorldModule::BridgeActionsToPhysicsModule (X360 0x827AC568,
        // WorldBridgeToEntityModules.cpp:28) does
        //     reinterpret_cast<VariableEventQueue<13312,16>*>(input->GetGameActionQueue())
        //         ->AddEvent(event, type, size);
        // for eighteen game-action ids (7/11/23/34/37/39/42/43/65/97/98/99/116/135/138/146/
        // 176/198). mGameActionQueue is the LAST member of this buffer, so AddEvent's first
        // store -- the CBufferEntry at macData[0], i.e. this member + 16 -- lands PAST THE END
        // of the IOBufferStack allocation, and the payload memcpy follows it. It was invisible
        // only because nothing had ever put a game action into the world input buffer: there was
        // no BridgeGameStateToWorld. Sizing it as the real queue makes the write in-bounds; the
        // buffer grows 13327 bytes (the update input stack is 16 MB).
        // The name is kept as a typedef so the four accessors' signatures do not change; the
        // two bridges' reinterpret_casts are now casts to the same type and can be retired.
        typedef CgsModule::VariableEventQueue<13312, 16> GameActionQueueStorage;

        // ---- getters (read-lock: status bit 4) ----
        const CameraStorage*                        GetCameraInput() const;                 // 0x8259F7F8 :272
        const VehicleInputInterfaceStorage*         GetVehicleInputInterface() const;       // 0x8259F8A0 :275
        const VehicleDriverInputInterfaceStorage*   GetVehicleDriverInterface() const;      // 0x8259F948 :278
        const VehicleEffectsInputInterfaceStorage*  GetVehicleEffectsInputInterface() const;// 0x8259F9F0 :281
        const TimerStatusInterfaceStorage*          GetTimerInterface() const;              // 0x8259FC90 :295
        // ---- ADDITIVE 2026-08-09 (conductor wave): the two unfolded queue getters ------
        const CgsModule::EventQueue<CgsSceneManager::SceneManagerIO::PotentialContact, 2048>*
                                                    GetPotentialContactQueue() const;       // 0x8259FB40 :289
        const CgsModule::EventQueue<CgsSceneManager::SceneManagerIO::OutOverlapPair, 128>*
                                                    GetOverlapPairsQueue() const;           // 0x8259FBE8 :292
        const u32*                                  GetSolverMaxIterations() const;         // 0x8259FD38 :298
        const PropInputInterfaceStorage*            GetPropManagerInputInterface() const;   // 0x8259FDE0 :301
        const GameActionQueueStorage*               GetGameActionQueue() const;             // 0x8259FE88 :304

        // ---- getters (write-lock: status bit 3 -- mutable overloads test the WRITE bit) ----
        VehicleInputInterfaceStorage*               GetVehicleInputInterface();             // 0x8279ED28 :276
        VehicleEffectsInputInterfaceStorage*        GetVehicleEffectsInputInterface();      // 0x8279EE78 :282
        PropInputInterfaceStorage*                  GetPropManagerInputInterface();         // 0x8279F2F8 :302
        GameActionQueueStorage*                     GetGameActionQueue();                   // 0x8279F3A0 :305

        // ---- setters (write-lock: status bit 3) ----
        void SetCameraInput(const CameraStorage* lpCamera);                                 // 0x827A9D30 :273
        void SetRCEntityOutputInterface(const RCEntityOutputInterfaceStorage* lpInterface); // 0x8279EF20 :285
        void SetTimerInterface(const TimerStatusInterfaceStorage* lpTimer);                 // 0x8279F128 :296
        void SetSolverMaxIterations(const u32* lpValue);                                    // 0x8279F240 :299

        // The X360 CreateIOBuffer<T> stack template runs T::Construct after the alloc; the PC
        // template placement-news only, so WorldModule::Update calls this explicitly. Until now
        // it resolved to the base IOBuffer::Construct, which raises the status byte and nothing
        // else -- so the embedded game-action queue was never Constructed and every
        // BridgeActionsToPhysicsModule AddEvent fired "Not Constructed"
        // (CgsVariableEventQueue.h:454) before corrupting memory. Raise the status, then run the
        // queue's own Construct (the console's own construct-list shape for this buffer).
        void Construct();

        static void _AssertLayout();

    private:
        u8                                  maStatusPad[15];                 // +1..+15 (force +16)
        CameraStorage                       mCameraInput;                    // +16      :309
        VehicleInputInterfaceStorage        mVehicleInputInterface;          // +368     :311 (real type; console span 142176)
        // ⚠ The console gap [+369..+142543] is GONE on the host: the real interface's host
        // sizeof EXCEEDS the 142176-byte console span (its embedded event queues carry the
        // x64-widened pointers), so -- per the mGameActionQueue precedent at the bottom of this
        // class -- the buffer simply GROWS and the members below sit at host offsets. The
        // console offsets in the trailing comments stay as console documentation only. (The old
        // `maDriverPad[142176 - sizeof(...)]` tripwire fired with a negative subscript on the
        // first build, which is how the growth was measured rather than assumed.)
        VehicleDriverInputInterfaceStorage  mVehicleDriverInterface;         // +142544  :312
        // gap to mVehicleEffectsInputInterface: +142545 .. +147839.
        unsigned char                       maEffectsPad[147840 - 142545];   //
        VehicleEffectsInputInterfaceStorage mVehicleEffectsInputInterface;   // +147840  :313
        // gap to mRCEntityOutputInterface: +147841 .. +149631.
        unsigned char                       maRCEntityPad[149632 - 147841];  //
        RCEntityOutputInterfaceStorage      mRCEntityOutputInterface;        // +149632  :314 (0x28F0)
        // ⭐ 2026-08-09 (conductor wave): two of the three folded queues are REAL members now,
        // seated by their own X360 accessor returns (GetPotentialContactQueue @0x8259FB40
        // `addis 2; addi 0x71D0` -> +160208; GetOverlapPairsQueue @0x8259FBE8 `addis 5;
        // addi -0xE20` -> +324064). The spans close with zero slack on both sides:
        //   mCreateWorldEventQueue (:316, still folded)   +160112 .. +160208  (96)
        //   mPotentialContactQueue (:317)                 +160208 .. +324064  (16 + 2048*80)
        //   mOverlapPairsQueue     (:318)                 +324064 .. +327152  (16 + 128*24)
        // (Console offsets; the host adds this buffer's uniform KU_DRIFT after the grown
        // mVehicleInputInterface, exactly like every member below it -- the layout gate in
        // BrnPhysicsModuleIO_InputBuffer.cpp pins both new seats at console+KU_DRIFT.)
        // Both element types are pointer-free and the queue header is 16 bytes on both
        // targets, so the RELATIVE spans equal the console's.
        unsigned char                       maCreateWorldQueuePad[160208 - 160112];   // :316 (folded)
        CgsModule::EventQueue<CgsSceneManager::SceneManagerIO::PotentialContact, 2048>
                                            mPotentialContactQueue;          // +160208  :317
        CgsModule::EventQueue<CgsSceneManager::SceneManagerIO::OutOverlapPair, 128>
                                            mOverlapPairsQueue;              // +324064  :318
        TimerStatusInterfaceStorage         mTimerInterface;                 // +327152  :319 (0x30)
        u32                                 mSolverMaxIterations;            // +327200  :320
        // gap to mPropManagerInputInterface: +327204 .. +327215.
        unsigned char                       maSolverPad[327216 - 327204];    //
        PropInputInterfaceStorage           mPropManagerInputInterface;      // +327216  :322
        // gap to mGameActionQueue: +327217 .. +338495.
        unsigned char                       maGameActionPad[338496 - 327217];//
        GameActionQueueStorage              mGameActionQueue;                // +338496  :323
    };
}
}
