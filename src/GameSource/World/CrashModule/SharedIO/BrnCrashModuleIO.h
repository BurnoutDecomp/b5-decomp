#pragma once

// Canonical (DWARF) home for the crash-module IO buffers (BrnCrashModuleIO.h). MINIMAL-COMPLETE
// slice: it currently homes only BrnWorld::CrashIO::OutputBuffer_PostPhysics, scoped to the single
// X360-emitted accessor this group owns:
//   GetNetworkOutputInterface()  @ 0x827BB9C0  write-lock (bit 3) -> this + 0x10  (BrnCrashModuleIO.h:208)
//
// The crash module's other post-physics buffers (and the InputBuffer_PostPhysics read-side
// accessors, DWARF BrnCrashModuleIO.h:171) have their own owning homes / slices and are NOT
// reconstructed here.
//
// LAYOUT (X360 getter return-offset + member alignment, authoritative):
//   base  CgsModule::IOBuffer                                       (1-byte status; +1..+0xF pad)
//   +0x10 NetworkOutputInterface mNetworkOutputInterface            (16-aligned: holds an
//                                                                    EventQueue<CrashingTrafficUpdateEvent,24>
//                                                                    whose element is alignas(16))
//
// The X360 GetNetwo getter (truncated -> GetNetworkOutputInterface) asserts the buffer is locked
// for writing (status bit 3, "Not locked for writing") and returns this + 0x10. The producer
// (CrashModule::GenerateOwnedTrafficUpdates @0x827C53F0) calls this then forwards the result to
// NetworkOutputInterface::AddOwnedTrafficUpdate. The IOBuffer base is a single status byte; the
// 16-aligned mNetworkOutputInterface naturally lands at +0x10.
#include "types.hpp"
#include "GameShared/GameClasses/Module/CgsIOBuffer.h"                          // CgsModule::IOBuffer
#include "GameShared/GameClasses/Core/CgsAssert.h"                              // CGS_ASSERT
#include "GameSource/World/CrashModule/SharedIO/BrnCrashModuleNetworkIOInterfaces.h" // NetworkOutputInterface + NetworkInputInterface
#include "GameSource/World/CrashModule/SharedIO/BrnCrashModuleTrafficIOInterfaces.h" // TrafficInputInterface + TrafficOutputInterface
#include "GameSource/World/CrashModule/SharedIO/BrnCrashModuleRaceCarIOInterfaces.h" // RaceCarOutputInterface (the crash-complete ring)
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleOutputInterface.h"    // BrnPhysics::Vehicle::VehicleOutputInterface + VehicleManagerOutputInterface
#include "GameShared/GameClasses/System/Timer/CgsTimerStatusInterface.h"             // CgsSystem::TimerStatusInterface
// [crash exit 2026-08-25] InputBuffer_PreScene now holds the REAL RCEntityActiveRaceCarOutputInterface
// by value (it used to be a console-sized opaque blob), so its home header is a hard dependency.
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h"

namespace BrnWorld
{
    // (RCEntityActiveRaceCarOutputInterface is now held BY VALUE by InputBuffer_PreScene, so its
//  real header is included below rather than forward-declared.)

namespace CrashIO
{
    // BrnCrashModuleIO.h:208 -- the crash module's post-physics output buffer.
    struct OutputBuffer_PostPhysics : public CgsModule::IOBuffer
    {
        // 0x827BB9C0 -- write-lock tripwire ("Not locked for writing"); return &mNetworkOutputInterface
        // (X360 returns this + 0x10).
        NetworkOutputInterface* GetNetworkOutputInterface();

    private:
        NetworkOutputInterface mNetworkOutputInterface;   // at offset +0x10 (16-aligned member)
    };

    // ========================================================================
    // The class:BrnWorld::CrashIO group also homes the read-side accessor of the crash module's
    // post-physics output buffer (DWARF BrnCrashModuleIO.h:210, @ 0x827A2680 -> this + 0x7A0,
    // caller WorldModule::BridgeCrashModuleToOutput). The DWARF places this getter in the same
    // OutputBuffer_PostPhysics type as GetNetworkOutputInterface (line 208) above, but the
    // returned member sits at +0x7A0 -- just past the embedded NetworkOutputInterface
    // (~EventQueue<CrashingTrafficUpdateEvent,24>) at +0x10. To keep this slice's layout
    // deterministic without depending on the exact sizeof that foreign queue, the read view is
    // modelled here as a self-contained buffer whose preceding region (the network-output
    // payload, owned by GetNetworkOutputInterface above) is opaque padding up to the +0x7A0
    // read member. The getter tests the read-lock bit (`extrwi r11,r11,1,27` == bit 4) and on
    // failure fires "Not locked for reading\n".
    //
    // NAME (PS3 DecFIGS reconcile): the PS3 DWARF (BrnCrashModuleIO.h:210/216) identifies this
    // second OutputBuffer_PostPhysics accessor as GetGameEventQueue() returning the member
    // mGameEventQueue. Names adopted here; the slice is kept as a separate read-view struct
    // (NOT restructured into OutputBuffer_PostPhysics) so the +0x7A0 offset stays exact without
    // depending on the foreign NetworkOutputInterface sizeof.
    struct OutputBuffer_PostPhysics_ReadView : public CgsModule::IOBuffer
    {
        struct GameEventQueueStorage { unsigned char maBytes[1]; };

        // 0x827A2680 -- read-lock tripwire; returns this + 0x7A0 (PS3: GetGameEventQueue).
        const GameEventQueueStorage* GetGameEventQueue() const;

        static void _AssertLayout();

    private:
        unsigned char         maPrecedingPayload[0x7A0 - 1];   // +1..+0x79F
        GameEventQueueStorage mGameEventQueue;                 // +0x7A0
    };

    // ========================================================================
    // The class:BrnWorld::CrashIO group also homes three single-accessor INPUT buffers whose
    // read-side getters the X360 emitted out-of-line. Each tests the read-lock bit
    // (`lbz r11,0(this); extrwi r11,r11,1,27` == bit 4 == IsBufferLockedForReading()) and on
    // failure fires the streamed "Not locked for reading\n" assert against BrnCrashModuleIO.h,
    // then returns the address of an embedded foreign-type member (`this + offset`). The getter
    // method names and the returned members' names are not recoverable from the truncated
    // symbols, so each returned member is modelled as correctly-positioned opaque storage so the
    // single X360-pinned return offset is exact; the preceding region (the buffer's other
    // payload, not recovered by this slice) is opaque padding.

    // BrnCrashModuleIO.h:63 (DWARF) -- crash-module pre-scene input buffer. DWARF-authoritative
    // full-member layout (the prior slice modelled this as an opaque read view with a single
    // mReadInterface @+0x3CD0, exposed as GetReadInterface; that getter is the const
    // GetVehicleDriverInterface below). Member ORDER + names from DecFIGS DWARF; the three
    // blind-copied interface members (vehicle-driver / active-race-car / game-action) have their
    // own homes and are modelled as correctly-sized opaque storage (X360 setter copy counts).
    // X360 member offsets (documentation): mTimerStatusInterface @+0x4, mNetworkInputInterface
    // @+0x40, mVehicleDriverInterface @+0x3CD0, mActiveRaceCarInterface @+0x5180, mGameActionQueue
    // @+0x7A70, mbPlayerPressingBoost @+0xAE80. On this PC build the embedded NetworkInputInterface
    // widens (its EventQueue holds a host pointer), so only the pointer-free prefix offset
    // (mTimerStatusInterface @+0x4) is pinned by _AssertLayout; the widening members' accessors
    // return &member (host-correct regardless of absolute offset).
    struct InputBuffer_PreScene : public CgsModule::IOBuffer
    {
        // Blind-copied foreign types (own homes elsewhere); correctly-sized opaque storage.
        struct VehicleDriverInterfaceStorage { unsigned char maBytes[0x14B0]; };   // 5296
        struct GameActionQueueStorage        { unsigned char maBytes[0x3410]; };   // 13328

        // ---- read-side getters (read-lock tripwire "Not locked for reading") ----
        const CgsSystem::TimerStatusInterface* GetTimerStatusInterface() const;    // 0x827BB288 -> +0x4
        const NetworkInputInterface*           GetNetworkInputInterface() const;   // 0x827BB330 -> +0x40
        const VehicleDriverInterfaceStorage*   GetVehicleDriverInterface() const;  // 0x827BB3D8 -> +0x3CD0 (ex-GetReadInterface)
        // 0x827BB480 -> +0x5180. THE crash module's window onto the race cars: TickCrashes,
        // ClearupCrashes and PreSceneUpdate all reach the player index, the per-car RaceCarState
        // and the per-car flags word through this one getter.
        const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface*
            GetActiveRaceCarInterface() const;                                     // 0x827BB480 -> +0x5180
        // +0xAE80. A header inline on the console: TickCrashes folds it to a bare
        // `lbzx r11, lpInput, 0xAE80` (@0x827C66A8) with no call, and
        // BridgeInputToCrashModule writes it with a bare store (@0x827ADEF8).
        bool GetPlayerPressingBoost() const { return mbPlayerPressingBoost; }
        void SetPlayerPressingBoost(bool lbPressing) { mbPlayerPressingBoost = lbPressing; }

        // ---- write-side setters (write-lock tripwire "Not locked for writing") ----
        void SetTimerStatusInterface(const CgsSystem::TimerStatusInterface* lpInterface);   // 0x827A20A8
        void SetNetworkInputInterface(const NetworkInputInterface* lpInterface);            // 0x827ACF88
        void SetVehicleDriverInterface(const VehicleDriverInterfaceStorage* lpInterface);   // 0x827A21B8
        // Latch the race-car module's pre-scene active-car view (caller
        // WorldModule::BridgeEntityModulesToCrashModule_PreScene @0x827A50D4). Pointer-only use of
        // the interface type (home: BrnRaceCarEntityModuleOutputInterface.h).
        void SetActiveRaceCarInterface(
            const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpInterface); // 0x827A2270
        // ⭐⭐ [crash exit 2026-08-25] mActiveRaceCarInterface WAS `ActiveRaceCarInterfaceStorage`
        // -- a 10480-byte OPAQUE BLOB copied in with a raw memcpy of the CONSOLE's size. That is
        // exactly the [[silent-drop-stubs]] shape and it is worse than inert: it is a WRONG-SIZE
        // copy of a host object. RCEntityActiveRaceCarOutputInterface holds pointers and
        // ResourceHandles, so its host sizeof is NOT 0x28F0; the memcpy read past (or short of)
        // the source, and every field the crash module wants -- mePlayerActiveRaceCarIndex,
        // maRaceCarStates[i], maxRaceCarFlags[i] -- sits at a HOST offset the blob cannot express.
        // The member is now the real committed type, the copy goes through the interface's own
        // operator= (already bodied in BrnRCEntityActiveRaceCarOutputInterface.cpp:105, and it is
        // what the console inlines here), and the crash module reads it BY NAME.
        void SetGameActionQueue(const GameActionQueueStorage* lpQueue);                     // 0x827A2328

        static void _AssertLayout();

    private:
        CgsSystem::TimerStatusInterface mTimerStatusInterface;    // X360 +0x4    (48B POD)
        NetworkInputInterface           mNetworkInputInterface;   // X360 +0x40   (align-16; widens on host)
        VehicleDriverInterfaceStorage   mVehicleDriverInterface;  // X360 +0x3CD0
        BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface
                                        mActiveRaceCarInterface;  // X360 +0x5180 (widens on host)
        GameActionQueueStorage          mGameActionQueue;         // X360 +0x7A70
        bool                            mbPlayerPressingBoost;    // X360 +0xAE80
    };

    // BrnCrashModuleIO.h:87 -- crash-module input buffer (read-side accessor @ 0x827BB528 ->
    // this + 0x7A70; caller CrashModule::HandleGameActions).
    struct InputBuffer_HandleGameActions : public CgsModule::IOBuffer
    {
        struct ReadInterfaceStorage { unsigned char maBytes[1]; };

        // 0x827BB528 -- read-lock tripwire; returns this + 0x7A70.
        const ReadInterfaceStorage* GetReadInterface() const;

        static void _AssertLayout();

    private:
        unsigned char        maPrecedingPayload[0x7A70 - 1];   // +1..+0x7A6F
        ReadInterfaceStorage mReadInterface;                   // +0x7A70
    };

    // BrnCrashModuleIO.h:156 (DWARF) -- crash-module post-physics input buffer. DWARF-authoritative
    // full-member layout (the prior slice modelled this as an opaque read view with a single
    // mReadInterface @+0x79B0, exposed as GetReadInterface; that getter is the const
    // GetVehicleManagerOutputInterface below). Three embedded-by-value interface members, each
    // exposed via a const read-lock getter + a write-lock setter. X360 member offsets
    // (documentation): mTrafficInputInterface @+0x8, mVehicleOutputInterface @+0xDA0,
    // mVehicleManagerOutputInterface @+0x79B0. The embedded interfaces hold host-widening
    // EventQueue members, so only the pointer-free prefix offset (mTrafficInputInterface @+0x8)
    // is pinned; the later accessors return &member (host-correct regardless of absolute offset).
    struct InputBuffer_PostPhysics : public CgsModule::IOBuffer
    {
        typedef BrnPhysics::Vehicle::VehicleOutputInterface        VehicleOutputInterface;         // DWARF :45
        typedef BrnPhysics::Vehicle::VehicleManagerOutputInterface VehicleManagerOutputInterface;  // DWARF :44

        // ---- read-side getters (read-lock tripwire "Not locked for reading") ----
        const TrafficInputInterface*         GetTrafficInputInterface() const;          // 0x827BB7C8 -> +0x8
        const VehicleOutputInterface*        GetVehicleOutputInterface() const;         // 0x827BB870 -> +0xDA0
        const VehicleManagerOutputInterface* GetVehicleManagerOutputInterface() const;  // 0x827BB918 -> +0x79B0 (ex-GetReadInterface)

        // ---- write-side setters (write-lock tripwire "Not locked for writing") ----
        void SetTrafficInputInterface(const TrafficInputInterface* lpTrafficInputInterface);                       // 0x827AD038
        void SetVehicleOutputInterface(const VehicleOutputInterface* lpVehicleOutputInterface);                    // 0x827AA388
        void SetVehicleManagerOutputInterface(const VehicleManagerOutputInterface* lpVehicleManagerOutputInterface); // 0x827AA438

        // ⭐⭐ [crash exit 2026-08-25] Construct @0x827CEA58. THIS WAS MISSING AND IT WAS FATAL THE
        // MOMENT THE MODULE WENT LIVE. CgsIOBufferStack::CreateIOBuffer<T> calls T::Construct()
        // right after placement-new; with no override here that bound to the BASE
        // CgsModule::IOBuffer::Construct, which only sets the "constructed" status bit. The
        // embedded VehicleManagerOutputInterface / VehicleOutputInterface / TrafficInputInterface
        // therefore had UNCONSTRUCTED EventQueues -- mpEvents == nullptr -- and the first frame
        // the crash bridge ran, VehicleManagerOutputInterface::operator= took its Clear()+Append()
        // path over a null mpEvents and the process took an access violation inside memcpy.
        // ⚠️ MEASURED, not theorised: the AV report named
        //   memcpy +0x131 <- VehicleManagerOutputInterface::operator= +0x397
        //                 <- WorldModule::EntityModulePostPhysicsUpdate
        // with "access violation WRITING 0x0000000000000000", preceded by the queue's own two
        // asserts ("mpEvents != NULL", CgsBaseEventQueue.h:189, and "Base event queue overflow").
        // ⭐ THE GENERAL LESSON: a buffer whose consumers are ALL gated can carry a missing
        // Construct indefinitely with nothing to notice -- the defect is created by un-gating.
        void Construct();

        static void _AssertLayout();

    private:
        TrafficInputInterface         mTrafficInputInterface;          // X360 +0x8    (DWARF :179)
        VehicleOutputInterface        mVehicleOutputInterface;         // X360 +0xDA0  (DWARF :180; align-16, widens on host)
        VehicleManagerOutputInterface mVehicleManagerOutputInterface;  // X360 +0x79B0 (DWARF :181; align-16, widens on host)
    };

    // ========================================================================
    // BrnWorld::CrashIO::OutputBuffer_PreScene (DWARF BrnCrashModuleIO.h:118). Homes the six
    // accessors the X360 emitted out-of-line for the crash module's pre-scene output buffer.
    // Three embedded interface members, each exposed via a const (read-lock) and a non-const
    // (write-lock) getter that return the SAME member address:
    //   mTrafficOutputInterface @ +0x8     read 0x827A23E0 (line 130) / write 0x827BB5D0 (line 131)
    //   mVehicleInputInterface  @ +0x670   read 0x827A2488 (line 133) / write 0x827BB678 (line 134)
    //   mRaceCarOutputInterface @ +0x231D0 read 0x827A2530 (line 136) / write 0x827BB720 (line 137)
    //
    // The const getters test the read-lock bit (`extrwi r11,r11,1,27` == bit 4) and fire
    // "Not locked for reading\n"; the non-const getters test the write-lock bit
    // (`extrwi r11,r11,1,28` == bit 3) and fire "Not locked for writing\n". Each pair returns
    // the same `this + offset` (the read/write overloads of one member accessor; the +0x231D0
    // offset is computed by the asm as `addis r3,this,2; addi r3,r3,0x31D0`).
    //
    // NAMES (PS3 DecFIGS reconcile, X360-confirmed): member + accessor names are from the PS3
    // DWARF (BrnCrashModuleIO.h:141-143 / 130-137) and corroborated by the X360 truncated
    // symbol stems (GetTr.. / GetV.. / GetRa..) plus the matching member ORDER + offsets
    // (Traffic@+0x8, Vehicle@+0x670, RaceCar@+0x231D0). FLAG (foreign types): the three
    // interface members have their own owning homes elsewhere and are NOT reconstructed here;
    // the regions between member starts are modelled as correctly-sized opaque storage so the
    // three X360-pinned return offsets are exact.
    // ⭐⭐ [crash exit 2026-08-25] THIS BUFFER IS ALSO THE "POST-SCENE" CRASH OUTPUT BUFFER.
    // BrnWorld::CrashModuleIO::OutputBuffer_PostScene DOES NOT EXIST. It was a phantom type
    // invented from the mangled signatures of the three post-scene crash bridges, and it stood
    // for weeks as "the single structural blocker for the whole crash-exit path".
    // PROVEN AT THE CALLER, not inferred:
    //   * WorldModule::Update @0x827D63E8 creates exactly FOUR crash IO buffers --
    //     CrashIO::{InputBuffer_PreScene, OutputBuffer_PreScene, InputBuffer_PostPhysics,
    //     OutputBuffer_PostPhysics}. There is no CreateIOBuffer<...OutputBuffer_PostScene>
    //     anywhere in the image. (Contrast TriggerEntityModuleIO::OutputBuffer_PostScene, which
    //     EntityModulePostSceneUpdate really does CreateIOBuffer/DestroyIOBuffer locally -- when
    //     the console genuinely has a distinct post-scene buffer, it makes one.)
    //   * CrashModule::PreSceneUpdate is called with `v202` (the OutputBuffer_PreScene) as its
    //     output, and EntityModulePostSceneUpdate is then called with `v202` in ARGUMENT SLOT 38
    //     -- the same local -- which is exactly the argument all three post-scene crash bridges
    //     receive. One buffer, written pre-scene, read post-scene. The crash module is a
    //     ModuleSingleBuffered: it has ONE output buffer by construction.
    //   * Consistently, the bridges call THIS type's own accessors by address:
    //     BridgeCrashModuleToRaceCarModule_PostScene @0x827AD5A0 -> 0x827A2530
    //     (GetRaceCarOutputInterface, +0x231D0); BridgeCrashModuleToTrafficModule_PostScene
    //     @0x827AD5E0 -> 0x827A23E0 (GetTrafficOutputInterface, +0x8);
    //     BridgeCrashModuleToPropModule_PostScene @0x827AAD78 -> 0x827A2530.
    // ⛔ DO NOT RE-INTRODUCE the phantom. The previous wave saw the same symbols and explained
    // them away as "two byte-identical getters that ICF folded, IDA kept one name". That theory
    // is refuted by the caller: there is no second buffer to have a second getter. Its companion
    // measurement ("CrashModuleIO::OutputBuffer_PostScene 16 bytes, racecar iface @143824 OUT OF
    // BOUNDS") was measuring the phantom's sizeof -- 143824 == 0x231D0 is precisely THIS type's
    // attested member offset, i.e. the evidence for the identity was already in the tree, read
    // as evidence against it.
    //
    // MEMBERS: real committed types in DWARF order, X360 offsets as comments and no artificial
    // padding -- the same shape InputBuffer_PostPhysics above already uses, and for the same
    // reason (these interfaces are align-16 and WIDEN on the host, so console offsets cannot be
    // reproduced and must not be static_asserted; only the ORDER is load-bearing).
    struct OutputBuffer_PreScene : public CgsModule::IOBuffer
    {
        // The crash side's vehicle input interface has no committed home yet and nothing in the
        // tree reads it; kept opaque so the member ORDER around it stays right.
        struct VehicleInputInterfaceStorage  { unsigned char maBytes[1]; };

        // +0x8 (line 130/131) -- the cleanup-traffic + network-crashing-traffic rings.
        const TrafficOutputInterface* GetTrafficOutputInterface() const;   // 0x827A23E0 read-lock
        TrafficOutputInterface*       GetTrafficOutputInterface();         // 0x827BB5D0 write-lock
        // +0x670 (line 133/134).
        const VehicleInputInterfaceStorage*  GetVehicleInputInterface() const;    // 0x827A2488 read-lock
        VehicleInputInterfaceStorage*        GetVehicleInputInterface();          // 0x827BB678 write-lock
        // +0x231D0 (line 136/137) -- holds the EventQueue<RaceCarCrashCompleteEvent,10> that
        // carries "this race car has finished crashing" to the race-car and prop modules.
        const RaceCarOutputInterface* GetRaceCarOutputInterface() const;   // 0x827A2530 read-lock
        RaceCarOutputInterface*       GetRaceCarOutputInterface();         // 0x827BB720 write-lock

        // ⭐ [crash exit 2026-08-25] Construct @0x827CE9E8, the twin of the InputBuffer_PostPhysics
        // one above and just as load-bearing: it is what constructs the
        // EventQueue<RaceCarCrashCompleteEvent,10> that CrashModule::ResetRaceCarFromCrashIndex
        // AddEvent()s into. Without it that AddEvent hits the queue's own "mpEvents != NULL"
        // tripwire and the crash-complete event is never posted -- i.e. the crash never ends,
        // which is the exact symptom this wave exists to remove.
        void Construct();

        static void _AssertLayout();

    private:
        TrafficOutputInterface       mTrafficOutputInterface;   // X360 +0x8     (widens on host)
        VehicleInputInterfaceStorage mVehicleInputInterface;    // X360 +0x670   (opaque, see above)
        RaceCarOutputInterface       mRaceCarOutputInterface;   // X360 +0x231D0 (widens on host)
    };
}
}
