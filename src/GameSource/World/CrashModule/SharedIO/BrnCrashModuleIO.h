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
}
}
