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

    // ========================================================================
    // BrnWorld::WorldEntityIO::OutputBuffer_PostPhysics (DWARF BrnWorldEntityModuleIO.h:~240).
    // ADDITIVE GROW: this slice homes ONLY the IO-OutputBuffers group's single X360-emitted
    // accessor of the post-physics output buffer:
    //   Get() @ 0x822BA8B8  write-lock (bit 3) -> this + 822896 (0xC8E70)  (asm-line 249)
    //
    // The X360 Get() tests the write-lock bit (`lbz r11,0(this); extrwi r11,r11,1,28` ==
    // bit 3 == IsBufferLockedForWriting()), and on failure fires the streamed
    // "Not locked for writing\n" assert against
    // ..\\gamesource\\world\\entitymodules\\worldentitymodule\\BrnWorldEntityModuleIO.h:249,
    // then returns the embedded output-payload member's address (this + 0xC8E70 == 822896,
    // computed by the asm as `addis r3,this,0xD; addi r3,r3,-0x7190`). Called by
    // WorldEntityModule::UpdateCollisionValidation / PostPhysicsUpdate /
    // WorldModule::UpdateForBootUpVideo.
    //
    // LAYOUT (X360 getter return-offset, authoritative):
    //   base    CgsModule::IOBuffer                 (1-byte status; +1..+0xC8E6F is the
    //                                                preceding payload region)
    //   +822896 (foreign payload member)            the post-physics output payload Get() returns
    //
    // FLAG (foreign type): the returned post-physics output payload has its own owning home
    // elsewhere and is NOT reconstructed here; the region from the status byte to the
    // +822896 member start is modelled as correctly-sized opaque storage so the single
    // X360-pinned return offset (this + 822896) is exact. Sized only to the asm-attested
    // offset; the intervening member split is not recoverable from this slice. The returned
    // member's interior is honestly opaque (the asm only takes its address).
    struct OutputBuffer_PostPhysics : public CgsModule::IOBuffer
    {
        // Opaque foreign-type storage (see FLAG above): first byte at this + 822896.
        struct OutputPayloadStorage { unsigned char maBytes[1]; };

        // X360 0x822BA8B8: write-lock tripwire ("Not locked for writing"); return this + 822896.
        OutputPayloadStorage* Get();

        static void _AssertLayout();

    private:
        // The IOBuffer base is a single status byte; the X360 build places the returned
        // output-payload member at this + 822896, so the intervening bytes (the preceding
        // payload region, not recovered by this slice) are modelled as opaque padding.
        unsigned char        maPrecedingPayload[822896 - 1];   // +1..+822895
        OutputPayloadStorage mOutputPayload;                   // +822896
    };
}
}
