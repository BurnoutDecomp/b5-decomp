#pragma once

// Canonical (DWARF) home for the BrnWorld::WorldEntityIO world-entity IO buffers
// (BrnWorldEntityModuleIO.h). This is a MINIMAL-COMPLETE slice: it currently homes only
// BrnWorld::WorldEntityIO::OutputBuffer_Prepare, scoped to the single X360-emitted
// accessor this group owns:
//   GetResourceRequestInterface()  @ 0x822BA180  write-lock (bit 3) -> this + 4  (DWARF :73)
//
// The const overload, the SceneInputInterface accessors, and the sibling prepare/scene/
// physics buffers (DWARF BrnWorldEntityModuleIO.h:61+) are NOT reconstructed here yet;
// they have their own owning homes / slices.
//
// LAYOUT (DWARF :61 member order + the X360 getter return-offset, authoritative):
//   base  CgsModule::IOBuffer                                  (1-byte status; +1..+3 pad)
//   +4    ResourceRequestInterface mResourceRequestInterface   (RequestInterface<4096>) :80
//   +...  SceneInputInterface      mSceneInputInterface        (InSceneUpdateInterface)  :81
//
// FLAG (foreign type): mResourceRequestInterface (a RequestInterface<4096>) and
// mSceneInputInterface have their own owning homes elsewhere and are NOT reconstructed
// here. The +4 member is modelled as correctly-positioned opaque storage so the single
// X360-pinned return offset (this + 4) is exact; the trailing scene-interface bytes are
// not separately recoverable from this slice and are omitted (this buffer is only
// constructed/locked elsewhere, never sized by this TU).
#include "types.hpp"
#include "GameShared/GameClasses/Module/CgsIOBuffer.h"   // CgsModule::IOBuffer
#include "GameShared/GameClasses/Core/CgsAssert.h"        // CGS_ASSERT

namespace BrnWorld
{
namespace WorldEntityIO
{
    // DWARF BrnWorldEntityModuleIO.h:61 — the world-entity "prepare" phase output buffer.
    struct OutputBuffer_Prepare : public CgsModule::IOBuffer
    {
        // Foreign type (RequestInterface<4096>): own home elsewhere. Modelled as opaque
        // storage so the X360 +4 return offset is exact. First byte at this+4.
        struct ResourceRequestInterfaceStorage { unsigned char maBytes[1]; };

        // 0x822BA180 — write-lock tripwire ("Not locked for writing"); return this + 4.
        ResourceRequestInterfaceStorage* GetResourceRequestInterface();

        static void _AssertLayout();

    private:
        // The IOBuffer base is a single status byte; the X360 build places
        // mResourceRequestInterface at this + 4, so pad bytes +1..+3 explicitly (the
        // 1-byte storage would otherwise pack at +1). Matches the sibling
        // PropEntityIO::OutputBuffer_PreScene layout.
        u8                              maStatusPad[3];              // +1..+3 (force +4)
        ResourceRequestInterfaceStorage mResourceRequestInterface;   // :80, at offset +4
    };
}
}
