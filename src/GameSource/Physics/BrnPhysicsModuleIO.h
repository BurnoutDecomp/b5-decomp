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
//     ⭐ 2026-08-19 (wave Q6/A1): its member is the REAL Props::PropOutputInterface now
//     (X360 Construct @0x825ABB10 calls PropOutputInterface::Construct(this+71792) at
//     0x825ABBF8), so both overloads return a typed interface and
//     PropManager::OutputUpdatedProps @0x82627EC8 -- the only producer of the UpdatePropEvent
//     stream that makes a smashed prop's parts move -- can finally be bodied.
//   GetDeformationOutputInterface()    @ 0x825A0128 write (bit 3) -> +148656 (DWARF :361)
//   GetContactSpyInterface()           @ 0x825A0320 write (bit 3) -> +998192 (DWARF :370)
//   GetSceneInputInterface() const     @ 0x8279F838 read  (bit 4) -> +179424 (DWARF :366)
//     ⭐ 2026-08-19 (wave Q5/F2): its member is the REAL SceneManagerIO::InSceneUpdateInterface
//     now (X360 Construct @0x825ABB10 calls InSceneUpdateInterface::Construct(this+179424)),
//     so both overloads return a typed interface and the reinterpret_cast seam their callers
//     used is retired at the call sites that this wave owns.
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
#include "GameShared/GameClasses/Core/CgsAssert.h"      // CGS_ASSERT (the two inline write-lock queue getters, 2026-08-19)
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"  // CgsModule::VariableEventQueue<13312,16> (mGameActionQueue)
#include "GameShared/GameClasses/System/Timer/CgsTimerStatusInterface.h" // CgsSystem::TimerStatusInterface (mTimerInterface, retyped 2026-08-09)
#include "GameShared/GameClasses/Module/CgsEventQueue.h"                 // CgsModule::EventQueue<T,N> (the two unfolded input queues, 2026-08-09)
#include "GameShared/GameClasses/SceneManager/SharedIO/CgsPotentialContact.h"          // SceneManagerIO::PotentialContact (mPotentialContactQueue element)
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventOutOverlapPair.h" // SceneManagerIO::OutOverlapPair (mOverlapPairsQueue element)
#include "GameSource/Physics/ContactSpies/BrnContactSpyInterface.h" // ContactSpy::ContactSpyInterface (real member @ +998192, promoted 2026-08-06)
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleInputInterface.h" // Vehicle::VehicleInputInterface (mVehicleInputInterface, retyped 2026-08-06)
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleOutputInterface.h" // Vehicle::VehicleOutputRequestInterface / VehicleManagerOutputInterface / VehicleOutputInterface (promoted 2026-08-09)
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleDriverInputInterface.h" // Vehicle::VehicleDriverInputInterface (mVehicleDriverInterface, retyped 2026-08-09)
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEffectsInputInterface.h" // Vehicle::VehicleEffectsInputInterface (mVehicleEffectsInputInterface, retyped 2026-08-10)
#include "GameSource/Physics/PropManager/SharedIO/BrnPropInputInterface.h" // Props::PropInputInterface (mPropManagerInputInterface, retyped 2026-08-10)
#include "GameSource/Physics/PropManager/SharedIO/BrnPropOutputInterface.h" // Props::PropOutputInterface (mPropManagerOutputInterface, retyped 2026-08-19)
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_SceneUpdate.h" // SceneManagerIO::InSceneUpdateInterface (mSceneInputInterface, retyped 2026-08-19)
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h" // BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface (mRCEntityOutputInterface, retyped 2026-08-10)

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
        // ⭐⭐ PROMOTED 2026-08-19 (wave Q6 cluster A1 -- the prop READ-BACK seam). This was a
        // 1-byte opaque placeholder pinned by a 76,863-byte pad, and it was the single reason
        // Props::PropManager::OutputUpdatedProps @0x82627EC8 (FOURTEEN instructions) could not
        // be bodied -- i.e. the reason a smashed prop's parts never reach the world module and
        // never visibly move.
        //
        // The type is NOT inferred -- the console names it in its own Construct. X360
        // OutputBuffer::Construct @0x825ABB10, instruction 0x825ABBF8:
        //     addis r3, r30, 1 ; addi r3, r3, 0x1870      == this + 71792
        //     bl BrnPhysics::Props::PropOutputInterface::Construct
        // and the DWARF spells the member's type `PropOutputInterface` (:381).
        //
        // The console SPAN closes the identification arithmetically: the next member,
        // mDeformationOutputInterface, sits at +148656, so the seat is exactly
        // 148656 - 71792 == 76,864 bytes wide -- and BrnPropOutputInterface.h's four embedded
        // EventQueue<T,200> members sum to exactly 38416 + 3216 + 22416 + 12816 == 76,864.
        // MEASURED host sizeof == 76,864 as well -- EXACTLY the console span, not merely >= it,
        // because all four element types are 16-byte aligned, so the host's 16-byte
        // BaseEventQueue header occupies the same slot the console's 12-byte header padded out
        // to (probe scratchpad/waveQ6/probe_seat/probe_seat_sizes.cpp). The pad that used to
        // follow this member is therefore GONE, not shrunk -- see the private section -- and
        // every later member keeps its current host offset (measured before/after with
        // /d1reportSingleClassLayout: mDeformationOutputInterface stays at host +148672 and
        // sizeof(OutputBuffer) stays 998,400).
        typedef Props::PropOutputInterface PropOutputInterfaceStorage;                        // :381
        struct DeformationOutputInterfaceStorage    { unsigned char maBytes[1]; };
        struct DeformationOutputInterfaceForEntityModulesStorage { unsigned char maBytes[1]; }; // :384 (unfolded 2026-08-09)
        // ⭐⭐ PROMOTED 2026-08-19 (wave Q5 cluster F2 -- the physics->scene seam). This was a
        // 1-byte opaque placeholder with an 818,767-byte pad behind it, which is why
        // WorldModule::BridgePhysicsSceneUpdateToScene @0x827ABA40 could not be mounted and why
        // BrnDeformableObject_Update.cpp:1436 has to gate SetEntityRadius behind "is the queue
        // storage even there" ("conductor gate: module-output scene interface unprepared").
        //
        // The type is NOT inferred -- the console names it in its own Construct. X360
        // OutputBuffer::Construct @0x825ABB10, instruction 0x825ABBEC:
        //     addis r3, r30, 3 ; addi r3, r3, -0x4320      == this + 179424
        //     bl CgsSceneManager::SceneManagerIO::InSceneUpdateInterface::Construct
        // and the DWARF spells the member's type `SceneInputInterface` (:385) -- the same
        // typedef-of-InSceneUpdateInterface every sibling IO buffer in the tree already carries
        // (BrnPropEntityModuleIO.h:211 OutputBuffer_PreScene::SceneInputInterfaceStorage,
        // BrnTrafficEntityModuleIO's OutputBuffer_Prepare twin).
        //
        // The console SPAN closes the identification arithmetically: the next member,
        // mContactSpyInterface, sits at +998192, so the seat is exactly 998192 - 179424 ==
        // 818,768 bytes wide -- and InSceneUpdateInterface's 25 embedded queues end at
        // mRemoveAllEntitiesQueue (X360 +0xC7E3C == 818,748) plus its 12-byte header + tail
        // alignment. MEASURED host sizeof == 818,944 (>= the console span, because the host's
        // BaseEventQueue header is 16 bytes where the console's is 12); probe
        // scratchpad/waveQ5/probe_f2/probe_sizes.cpp. So the pad that used to follow this
        // member is GONE, not shrunk -- see the private section.
        typedef CgsSceneManager::SceneManagerIO::InSceneUpdateInterface SceneInputInterfaceStorage;   // :385
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
        // ⭐ ADDITIVE 2026-08-18 (wave Q4, prop bridges). The READ-LOCK const twin, X360
        // @0x8279F8E0 (bit-4 test, "Not locked for reading\n", baked cite BrnPhysicsModuleIO.h:369
        // -- one line above the write twin's :370 already carried here). This was a written-down
        // TODO in the tree, not a discovery: WorldBridgePhysicsToEntityModules.cpp's own banner
        // parks its leg 6 with "the console's read-locked const twin is @0x8279F8E0; adding and
        // bodying it is a two-line follow-up". Both bridges that carry the contact-spy handle
        // hold a `const OutputBuffer*` (their callers read-lock the source), so the non-const
        // overload above is unusable to them -- and one of them,
        // WorldModule::BridgePhysicsModuleToPropModule_PostPhysics @0x827AB998, is the sole
        // producer of PropEntityIO::InputBuffer_PostPhysics::mContactSpyInterface::mpData, whose
        // absence asserts + AVs in PropEntityModule::ProcessContacts on the first prop frame.
        // Declaration + body only: no layout, no member, no existing signature is touched.
        const ContactSpy::ContactSpyInterface*      GetContactSpyInterface() const;           // +998192, read  (0x8279F8E0, DWARF :369)
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

        // ⛔ ADDED 2026-08-10 (root-cause wave). This buffer had NO Construct at all, while the
        // console emits one (X360 0x825ABB10, 64 instructions) that the CreateIOBuffer<T> stack
        // template runs after the alloc. (Historic: the PC template used to placement-new only;
        // CreateIOBuffer<T> runs T::Construct as of 2026-08-15.) Every embedded
        // queue in here was left un-Constructed -- and PhysicsModule::Update's
        // BridgeVehicleManagerToOutput drains the vehicle manager's requests INTO this buffer's
        // mVehicleOutputRequestInterface, whose VariableEventQueue<13440,16> then fired
        // "Not Constructed" (CgsVariableEventQueue.h:759) on every frame the physics module ran.
        // Same family as the InputBuffer partial-Construct fixed 2026-08-09.
        void Construct();

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
        // ⚠ NO PAD HERE ANY MORE (2026-08-19, wave Q6/A1): the seat holds the REAL
        // Props::PropOutputInterface now, whose host sizeof (76,864 -- measured) is EXACTLY the
        // console span [+71792 .. +148656]. The old `maDeformationPad[148656 - 71793]` was
        // pinning the console offset of a member that could never be written; the correct
        // replacement width is ZERO, and a zero-length array is not legal here, so the pad is
        // deleted outright (the same disposition mSceneInputInterface's pad took on 2026-08-19).
        // _AssertLayout gates the surviving relation (`>=` the console delta).
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
        // ⚠ NO PAD HERE ANY MORE (2026-08-19, wave Q5/F2): the seat holds the REAL
        // InSceneUpdateInterface now, whose host sizeof (818,944 -- measured) already EXCEEDS
        // the 818,768-byte console span [+179424 .. +998192], exactly like
        // mVehicleInputInterface / mPropManagerInputInterface do in the InputBuffer below. The
        // old `maScenePad[998192 - 179425]` was holding the console offset of a member that
        // could never be written; keeping it as well would have added 818 KB of dead space.
        // _AssertLayout gates the surviving relation (`>=` the console delta).

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
        // ⭐ RETYPED 2026-08-09 (feed wave; third use of the typedef pattern). The real
        // BrnPhysics::Vehicle::VehicleDriverInputInterface -- host sizeof 5296 == the console
        // span (142544..147840), because every member is pointer-free (the inline
        // VariableEventQueue<5040,16> plus the SIMD target-assist arrays), so the seat and every
        // pin below are UNCHANGED. WorldModule::BridgeInputToPhysicsModule @0x827AB830 calls
        // Vehicle::VehicleDriverInputInterface::Append @0x823DB640 on this member -- an opaque
        // one-byte span cannot express that, and casting one to the real type would walk the
        // driver-update queue 5 KB past the slice.
        typedef BrnPhysics::Vehicle::VehicleDriverInputInterface VehicleDriverInputInterfaceStorage;
        // ⭐ RETYPED 2026-08-10 (pre-physics bridge wave; SIXTH use of the typedef pattern, after
        // the vehicle-input / vehicle-driver / game-action / prop-input / RCEntity seats), and it
        // was the SAME latent memory bug the other five were:
        // WorldModule::BridgeEntityModulesToPhysicsModule_PrePhysics @0x827AAEC0 merges the
        // race-car AND traffic effects interfaces into this member twice per frame
        // (0x827AB138/0x827AB144 and 0x827AB164/0x827AB170 -- an air-ram queue Append at +0 and a
        // spin queue Append at +0x510), i.e. it writes up to 1,792 bytes through what was a
        // one-byte span. Invisible only because the bridge was an inert boot gate.
        // The real type is byte-identical in extent on both targets: EventQueue<CreateAirRamEvent,
        // 20> is 16 + 20*64 == 1296 and EventQueue<CreateSpinEvent,10> is 16 + 10*48 == 496
        // (console 12-byte queue headers pad to 16, host 8+4+4 IS 16), so 1792 == the span this
        // member already reserved and NOTHING below it moves. Pinned exactly in _AssertLayout --
        // if that equality ever breaks the gate fails instead of silently re-drifting the tail.
        typedef BrnPhysics::Vehicle::VehicleEffectsInputInterface VehicleEffectsInputInterfaceStorage;
        // ⭐ RETYPED 2026-08-10 (create-path wave; FIFTH use of the typedef pattern, after the
        // vehicle-input / vehicle-driver / game-action / prop-input seats). This was
        // `unsigned char[0x28F0]` and it is about to go LIVE:
        // PhysicsModule::PostSceneUpdate @0x825ABC10 reads THREE members straight out of it --
        // mePlayerActiveRaceCarIndex (+0x2858 == 10328), mbIsPlayerCarActive (+0x2860 == 10336)
        // and maRaceCarStates[player].mEntityId (`mulli 0x460` + `lwz 0x6F8`) -- and feeds the
        // first to VehicleManager::SetPlayerActiveRaceCarIndex and the third to
        // DeformationManager::FindModelIndexByEntityID. Reaching those through an opaque byte
        // span means three raw console byte offsets on a live path, which is the offset hack this
        // project forbids AND the memory-bug shape the standing rule names.
        // The real type is byte-identical in extent: BrnWorldModuleIO.h:685 pins
        // RCEntityActiveRaceCarOutputInterface at 10480 B == 0x28F0, which is exactly the span
        // this member already reserved, so the seat, every pad below and the existing
        // SetRCEntityOutputInterface memcpy are UNCHANGED (the _AssertLayout static_asserts are
        // the tripwire if that ever stops being true).
        typedef BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface
                                                    RCEntityOutputInterfaceStorage;      // 10480
        // ⭐ RETYPED 2026-08-09 (conductor wave; same typedef pattern as the vehicle-input
        // span): the real CgsSystem::TimerStatusInterface -- 48 bytes on both targets (two
        // pointer-free 24-byte TimerStatus blocks), so the seat and every pin below are
        // unchanged. PhysicsModule::Update @0x825B0640 reads both sub-statuses through it
        // (sim step = [+32]*[+28], game step = [+8]*[+4], sim Time = [+40..47]).
        typedef CgsSystem::TimerStatusInterface     TimerStatusInterfaceStorage;         // 48
        // ⭐ RETYPED 2026-08-10 (root-cause wave; FOURTH use of the typedef pattern, after the
        // vehicle-input / vehicle-driver / game-action seats). This was `unsigned char[1]`
        // while THREE committed bridges reinterpret_cast it to the real ~12 KB
        // BrnPhysics::Props::PropInputInterface and Append into it --
        // BridgePropModuleToPhysicsModule_Prepare @0x827AB410 (mounted and live from
        // BrnWorldModule.cpp:1025), BridgeEntityModulesToPhysicsModule_PreScene @0x827AADB8
        // and _PrePhysics @0x827AAEC0. Appending through the 1-byte slice writes each queue's
        // header and payload straight through maGameActionPad and out the far side into
        // mGameActionQueue; it has stayed invisible only because no prop is physically
        // registered yet, so every source queue is empty. Retyping is the only memory-safe
        // option and it also lets InputBuffer::Construct restore the console's four prop-queue
        // Construct calls (see the Construct body).
        typedef BrnPhysics::Props::PropInputInterface PropInputInterfaceStorage;
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
        // ⭐ ADDITIVE 2026-08-10 (create-path wave). Read-lock accessor for the race-car entity
        // output interface. The DWARF line is READ FROM THE IMAGE, not guessed: the body at
        // 0x8259FA98 fires `li r5, 0x11C` == 284 into FireAssert, and returns
        // `addis r3,r28,2 / addi r3,r3,0x4880` == this+149632 == &mRCEntityOutputInterface.
        // Its one console caller is PhysicsModule::PostSceneUpdate @0x825ABC10 (three times).
        const RCEntityOutputInterfaceStorage*       GetRCEntityOutputInterface() const;     // 0x8259FA98 :284
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
        // ⭐ ADDITIVE 2026-08-09 (feed wave): the MUTABLE driver-interface handle. Recovered by
        // disassembling 0x8279EDD0 out of the image (the function is a HOLE in the IDA export
        // set): write-lock bit 3 (`rlwinm r11,r11,0x1d,0x1f,0x1f`), assert line 279, then
        // `addis r3,r28,2 / addi r3,r3,0x2CD0` == return &mVehicleDriverInterface (this+142544).
        // The ONLY caller is WorldModule::BridgeInputToPhysicsModule @0x827AB830.
        VehicleDriverInputInterfaceStorage*         GetVehicleDriverInterface();            // 0x8279EDD0 :279
        VehicleEffectsInputInterfaceStorage*        GetVehicleEffectsInputInterface();      // 0x8279EE78 :282
        PropInputInterfaceStorage*                  GetPropManagerInputInterface();         // 0x8279F2F8 :302
        GameActionQueueStorage*                     GetGameActionQueue();                   // 0x8279F3A0 :305

        // ---- ADDITIVE 2026-08-19 (wave Q5 cluster F2): the WRITE-locked twins of the two
        // unfolded queue getters above. Both are real out-of-line X360 symbols and both are
        // read off the asm, not inferred:
        //   0x8279EFD8  `extrwi r11,r11,1,28` (bit 3) -> "Not locked for writing\n",
        //               baked cite BrnPhysicsModuleIO.h:290 (li r5,0x122), `return this+160208`
        //   0x8279F080  same bit-3 test, baked cite :293 (li r5,0x125), `return this+324064`
        // Both offsets are the SAME seats the const twins at :289/:292 return, so no member and
        // no layout moves. Their ONLY caller in ARTIST is
        // WorldModule::BridgeScenePotentialContactsToPhysics @0x827ABD80, which write-locks this
        // buffer and read-locks the scene output -- i.e. the const overloads are unusable to it.
        // Bodied INLINE here (rather than in BrnPhysicsModuleIO_InputBuffer_Accessors.cpp beside
        // the const twins) because that TU is another owner's this wave; inline emits no new
        // out-of-line symbol, so mounting order and LNK2005 are both unaffected.
        CgsModule::EventQueue<CgsSceneManager::SceneManagerIO::PotentialContact, 2048>*
        GetPotentialContactQueue()                                                          // 0x8279EFD8 :290
        {
            CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
            return &mPotentialContactQueue;
        }
        CgsModule::EventQueue<CgsSceneManager::SceneManagerIO::OutOverlapPair, 128>*
        GetOverlapPairsQueue()                                                              // 0x8279F080 :293
        {
            CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
            return &mOverlapPairsQueue;
        }

        // ---- setters (write-lock: status bit 3) ----
        void SetCameraInput(const CameraStorage* lpCamera);                                 // 0x827A9D30 :273
        void SetRCEntityOutputInterface(const RCEntityOutputInterfaceStorage* lpInterface); // 0x8279EF20 :285
        void SetTimerInterface(const TimerStatusInterfaceStorage* lpTimer);                 // 0x8279F128 :296
        void SetSolverMaxIterations(const u32* lpValue);                                    // 0x8279F240 :299

        // CreateIOBuffer<T> runs T::Construct (2026-08-15) -- on both targets now; the PC
        // template used to placement-new only, which is why WorldModule::Update used to call
        // this explicitly. Until it was bodied
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
        VehicleDriverInputInterfaceStorage  mVehicleDriverInterface;         // +142544  :312 (real type)
        // No pad here any more: the real driver interface's host sizeof IS the console span
        // (5296 == 147840 - 142544 -- it is entirely pointer-free), so the next seat lands
        // exactly where the console's does. Pinned by _AssertLayout; if that equality ever
        // breaks the gate fails rather than silently re-drifting everything below.
        VehicleEffectsInputInterfaceStorage mVehicleEffectsInputInterface;   // +147840  :313
        // No pad here any more (2026-08-10): the real effects interface's host sizeof IS the
        // console span (1792 == 149632 - 147840 -- the two inline EventQueues' 12-byte console
        // headers already pad to 16, which is exactly what the host's 8-byte mpEvents + two s32
        // occupy), so the next seat lands where the console's does. Pinned by _AssertLayout.
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
        PropInputInterfaceStorage           mPropManagerInputInterface;      // +327216  :322 (real type; console span 11280)
        // ⚠ SECOND HOST GROWTH POINT (2026-08-10). The console gap [+327217..+338495] is GONE:
        // the real prop-input interface's host sizeof EXCEEDS its 11280-byte console span
        // (its four embedded event queues carry the x64-widened mpEvents, and the
        // ResourceHandle is two pointers), so -- exactly like mVehicleInputInterface above --
        // the buffer simply GROWS and mGameActionQueue sits at console+KU_DRIFT+KU_PROP_DRIFT.
        // Both drifts are computed and pinned in _AssertLayout; the pad is deliberately not
        // reinstated (a fixed pad would have to go negative).
        GameActionQueueStorage              mGameActionQueue;                // +338496  :323
    };
}
}
