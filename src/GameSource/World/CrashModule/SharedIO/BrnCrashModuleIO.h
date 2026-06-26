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
#include "GameSource/World/CrashModule/SharedIO/BrnCrashModuleNetworkIOInterfaces.h" // NetworkOutputInterface

namespace BrnWorld
{
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

    // BrnCrashModuleIO.h:81 -- crash-module pre-scene input buffer (read-side accessor
    // @ 0x827BB3D8 -> this + 0x3CD0; caller CrashModule::ResetCrashedNetworkRaceCars).
    struct InputBuffer_PreScene : public CgsModule::IOBuffer
    {
        struct ReadInterfaceStorage { unsigned char maBytes[1]; };

        // 0x827BB3D8 -- read-lock tripwire; returns this + 0x3CD0.
        const ReadInterfaceStorage* GetReadInterface() const;

        static void _AssertLayout();

    private:
        unsigned char        maPrecedingPayload[0x3CD0 - 1];   // +1..+0x3CCF
        ReadInterfaceStorage mReadInterface;                   // +0x3CD0
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

    // BrnCrashModuleIO.h:174 -- crash-module post-physics input buffer (read-side accessor
    // @ 0x827BB918 -> this + 0x79B0; callers CrashModule::ProcessCrashedRaceCarEvents /
    // ProcessSlammedTrafficEvents / PostPhysicsUpdate).
    struct InputBuffer_PostPhysics : public CgsModule::IOBuffer
    {
        struct ReadInterfaceStorage { unsigned char maBytes[1]; };

        // 0x827BB918 -- read-lock tripwire; returns this + 0x79B0.
        const ReadInterfaceStorage* GetReadInterface() const;

        static void _AssertLayout();

    private:
        unsigned char        maPrecedingPayload[0x79B0 - 1];   // +1..+0x79AF
        ReadInterfaceStorage mReadInterface;                   // +0x79B0
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
    struct OutputBuffer_PreScene : public CgsModule::IOBuffer
    {
        struct TrafficOutputInterfaceStorage { unsigned char maBytes[1]; };
        struct VehicleInputInterfaceStorage  { unsigned char maBytes[1]; };
        struct RaceCarOutputInterfaceStorage { unsigned char maBytes[1]; };

        // +0x8 (line 130/131).
        const TrafficOutputInterfaceStorage* GetTrafficOutputInterface() const;   // 0x827A23E0 read-lock
        TrafficOutputInterfaceStorage*       GetTrafficOutputInterface();         // 0x827BB5D0 write-lock
        // +0x670 (line 133/134).
        const VehicleInputInterfaceStorage*  GetVehicleInputInterface() const;    // 0x827A2488 read-lock
        VehicleInputInterfaceStorage*        GetVehicleInputInterface();          // 0x827BB678 write-lock
        // +0x231D0 (line 136/137).
        const RaceCarOutputInterfaceStorage* GetRaceCarOutputInterface() const;   // 0x827A2530 read-lock
        RaceCarOutputInterfaceStorage*       GetRaceCarOutputInterface();         // 0x827BB720 write-lock

        static void _AssertLayout();

    private:
        // The IOBuffer base is a single status byte; the X360 places mTrafficOutputInterface at
        // this+0x8, so pad bytes +1..+7 explicitly.
        u8                            maStatusPad[7];                      // +1..+7 (force +0x8)
        TrafficOutputInterfaceStorage mTrafficOutputInterface;            // +0x8
        unsigned char                 maTrafficPad[0x670 - 0x8 - 1];      // +0x9..+0x66F
        VehicleInputInterfaceStorage  mVehicleInputInterface;            // +0x670
        unsigned char                 maVehiclePad[0x231D0 - 0x670 - 1];  // ...
        RaceCarOutputInterfaceStorage mRaceCarOutputInterface;           // +0x231D0
    };
}
}
