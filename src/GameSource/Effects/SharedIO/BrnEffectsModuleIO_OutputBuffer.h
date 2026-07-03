#pragma once

// ============================================================================
// b5-decomp/src/GameSource/Effects/SharedIO/BrnEffectsModuleIO_OutputBuffer.h
// ============================================================================
// Canonical (DWARF) home of BrnEffects::EffectsIO::OutputBuffer
// (EffectsModuleIO.h:275) -- the per-frame buffer the effects module publishes to
// request resources / vault attribs / replay data. A CgsModule::IOBuffer payload
// holding three request-interface sub-objects.
//
// Class SHAPE + member NAMES are DWARF-authoritative
// (references/DecFIGS/dwarfdump/GameSource/Effects/EffectsModuleIO.h:275):
//   struct OutputBuffer : public IOBuffer {
//       PrepareOutputBuffer::EffectsModuleResourceQueue    mResourceRequestInterface; :303
//       AttribSysRequestInterface<2048>                    mVaultRequestInterface;    :304
//       (ReplayRequestInterface = BrnReplays::ReplayIO::RequestInterface) mReplayRequestInterface; :305
//   };
// (PrepareOutputBuffer::EffectsModuleResourceQueue is RequestInterface<4096> per
//  ParticleModuleIO.h:32 DWARF.)
//
// IDIOM: NAMED members accessed BY NAME (the committed sibling pattern in
// RendererIO::OutputBuffer, BrnRendererModuleIO.cpp -- `return &mVaultRequestInterface;`).
// The three interior interface types are NOT reconstructed in-tree, so each member is
// an opaque, correctly-sized byte payload with EXPLICIT PADDING forcing the
// X360-attested offset. Every pinned offset + size below is X360-attested:
//   mResourceRequestInterface @ this+0x4    size 0x1010 (4112)  -- SetResourceRequestInterface memcpy stride
//   mVaultRequestInterface    @ this+0x1014 size 0x810  (2064)  -- gap [0x1014, 0x1824)
//   mReplayRequestInterface   @ this+0x1824 (6180)      size not attested (trailing member)
// (mVaultRequestInterface's 0x810 span == sizeof(VariableEventQueue<2048,16>): 1 + 2048
//  + 3*4 rounded to 4 == 2064; DWARF :63 typedef AttribSysRequestInterface<2048>. Kept
//  as an opaque byte payload here so the accessor return-offset pins WITHOUT pulling the
//  VEQ template cascade; swap in the real request-interface type -- keeping these pinned
//  offsets -- when its DWARF lands.)
// NOTE: base IOBuffer is a single status byte (FlagSet<s8>); a 3-byte pad forces the
// first member to +0x4.
//
// Out-of-line accessors emitted by the X360 ARTIST build (all in
// BrnEffectsModuleIO_OutputBuffer.cpp):
//   GetResourceRequestInterface() const  @0x823BABF0 R  -> this+0x4     :291
//   GetResourceRequestInterface()        @0x8227E130 W  -> this+0x4     :292
//   SetResourceRequestInterface()        @0x8227E078 W  memcpy this+0x4 :290
//   GetVaultRequestInterface() const     @0x823BAC98 R  -> this+0x1014  :296
//   GetVaultRequestInterface()           @0x8227E1D8 W  -> this+0x1014  :295
//   GetReplayRequestInterface() const    @0x823BAD40 R  -> this+0x1824  :298
//   GetReplayRequestInterface()          @0x8227E280 W  -> this+0x1824  :300
//
// Lock-bit guard per the recurring IOBuffer prologue (reproduced verbatim -- the
// non-const writer-side getters test the WRITE bit, the const getters the READ bit):
//   read-lock  (status>>4 & 1) => IsBufferLockedForReading()  ("Not locked for reading\n")
//   write-lock (status>>3 & 1) => IsBufferLockedForWriting()  ("Not locked for writing\n")
// (These Effects lock strings carry the trailing \n per the X360 rodata.)

#include "types.hpp"                                     // u8
#include "GameShared/GameClasses/Module/CgsIOBuffer.h"   // CgsModule::IOBuffer

namespace BrnEffects
{
namespace EffectsIO
{
    struct OutputBuffer : public CgsModule::IOBuffer
    {
    public:
        // --- Opaque, attested-size stand-ins for the interior interface types ---
        // Replace with PrepareOutputBuffer::EffectsModuleResourceQueue (RequestInterface<4096>) when defined.
        struct EffectsModuleResourceQueue { u8 maOpaquePayload[0x1010]; };   // 4112
        // Replace with CgsAttribSys::AttribSysIO::AttribSysRequestInterface<2048> (VEQ<2048,16>, 2064 bytes).
        struct VaultRequestInterface      { u8 maOpaquePayload[0x810];  };   // 2064
        // Replace with BrnReplays::ReplayIO::RequestInterface. Trailing size not attested;
        // 4 bytes is a placeholder minimum (offset 0x1824 is the load-bearing fact).
        struct ReplayRequestInterface     { u8 maOpaquePayload[4]; };

        // X360 0x823BABF0 (R, :291) -- read-lock handle at this+0x4.
        const EffectsModuleResourceQueue* GetResourceRequestInterface() const;
        // X360 0x8227E130 (W, :292) -- write-lock handle at this+0x4.
        EffectsModuleResourceQueue* GetResourceRequestInterface();
        // X360 0x8227E078 (W, :290) -- write-lock memcpy of 0x1010 bytes into this+0x4.
        void SetResourceRequestInterface(const EffectsModuleResourceQueue* lpInterface);

        // X360 0x823BAC98 (R, :296) -- read-lock handle at this+0x1014.
        const VaultRequestInterface* GetVaultRequestInterface() const;
        // X360 0x8227E1D8 (W, :295) -- write-lock handle at this+0x1014.
        VaultRequestInterface* GetVaultRequestInterface();

        // X360 0x823BAD40 (R, :298) -- read-lock handle at this+0x1824.
        const ReplayRequestInterface* GetReplayRequestInterface() const;
        // X360 0x8227E280 (W, :300) -- write-lock handle at this+0x1824.
        ReplayRequestInterface* GetReplayRequestInterface();

        static void _AssertLayout();

    private:
        u8                         maStatusPad[3];              // +1..+3 (force +0x4)
        EffectsModuleResourceQueue mResourceRequestInterface;  // @ this+0x4    :303
        VaultRequestInterface      mVaultRequestInterface;     // @ this+0x1014 :304
        ReplayRequestInterface     mReplayRequestInterface;    // @ this+0x1824 :305
    };
}
}
