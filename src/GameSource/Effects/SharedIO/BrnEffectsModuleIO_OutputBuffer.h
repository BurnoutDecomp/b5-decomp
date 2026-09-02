#pragma once

// ============================================================================
// b5-decomp/src/GameSource/Effects/SharedIO/BrnEffectsModuleIO_OutputBuffer.h
// ============================================================================
// Canonical (DWARF) home of BrnEffects::EffectsIO::OutputBuffer
// (EffectsModuleIO.h:275) -- the per-frame buffer the effects module publishes to
// request resources / vault attribs / replay data. A CgsModule::IOBuffer payload
// holding three request-interface sub-objects:
//
//   struct OutputBuffer : public IOBuffer {
//       PrepareOutputBuffer::EffectsModuleResourceQueue    mResourceRequestInterface; :303
//       AttribSysRequestInterface<2048>                    mVaultRequestInterface;    :304
//       (ReplayRequestInterface = BrnReplays::ReplayIO::RequestInterface) mReplayRequestInterface; :305
//   };
// (PrepareOutputBuffer::EffectsModuleResourceQueue is RequestInterface<4096> per
//  ParticleModuleIO.h:32 DWARF.)
//
// 2026-09-02 (tyre-mark wave): the three members are the REAL committed request
// interfaces now (they used to be opaque byte payloads). That is what lets the
// loading spine forward them the way it forwards the sound and world modules'
// (`AppendRequestInterface<4096>(*out->GetResourceRequestInterface())` and the
// AttribSys <2048> queue append in LoadEffectsModule @0x823E7820), and what lets
// EffectsModule::Prepare's `SetResourceRequestInterface(out, prepareOut->
// GetResourceRequestInterface())` be a typed copy.
//
// X360 offsets (console widths), the pins of the old opaque model:
//   mResourceRequestInterface @ this+0x4    size 0x1010 (4112)  -- SetResourceRequestInterface memcpy stride
//   mVaultRequestInterface    @ this+0x1014 size 0x810  (2064)  -- gap [0x1014, 0x1824)
//   mReplayRequestInterface   @ this+0x1824 (6180)
// Both request queues are VariableEventQueue<N,16> (1 + N + 12 bytes on the console),
// so the widths above ARE sizeof(RequestInterface<4096>) / sizeof(AttribSysRequest-
// Interface<2048>) on the console; on the host the by-name members carry their host
// sizes and nothing addresses this buffer by offset.
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

#include "types.hpp"                                                    // u8
#include "GameShared/GameClasses/Module/CgsIOBuffer.h"                  // CgsModule::IOBuffer
#include "GameSource/Resource/SharedIO/BrnGameDataRequestQueue.h"       // BrnResource::GameDataIO::RequestInterface<N>
#include "GameShared/GameClasses/System/AttribSys/CgsAttribSysModuleIO.h" // CgsAttribSys::AttribSysIO::AttribSysRequestInterface<N>
#include "GameSource/Replays/BrnReplayRequestInterface.h"               // BrnReplays::ReplayIO::RequestInterface

namespace BrnEffects
{
namespace EffectsIO
{
    struct OutputBuffer : public CgsModule::IOBuffer
    {
    public:
        // The DWARF typedefs (EffectsModuleIO.h:303-305 / ParticleModuleIO.h:32).
        typedef BrnResource::GameDataIO::RequestInterface<4096>                 EffectsModuleResourceQueue;
        typedef CgsAttribSys::AttribSysIO::AttribSysRequestInterface<2048>       VaultRequestInterface;   // AttribSysUserVaultRequestInterface
        typedef BrnReplays::ReplayIO::RequestInterface                          ReplayRequestInterface;

        // (:308) The buffer-stack Construct. The X360 CreateIOBuffer<EffectsIO::OutputBuffer>
        // instantiation is an export hole; the body is the type's own: IOBuffer::Construct
        // (status = 1) then each request interface's Construct -- the same shape as the sound
        // RootOutputBuffer's (BrnRootSoundModuleIo.h) and the world UpdateOutputBuffer's.
        void Construct();
        // (:311) -- ICF-folded into IOBuffer::Destruct on the console (no member teardown).
        void Destruct() { CgsModule::IOBuffer::Destruct(); }

        // X360 0x823BABF0 (R, :291) -- read-lock handle at this+0x4.
        const EffectsModuleResourceQueue* GetResourceRequestInterface() const;
        // X360 0x8227E130 (W, :292) -- write-lock handle at this+0x4.
        EffectsModuleResourceQueue* GetResourceRequestInterface();
        // X360 0x8227E078 (W, :290) -- write-lock copy of the whole request interface into this+0x4.
        void SetResourceRequestInterface(const EffectsModuleResourceQueue* lpInterface);

        // X360 0x823BAC98 (R, :296) -- read-lock handle at this+0x1014.
        const VaultRequestInterface* GetVaultRequestInterface() const;
        // X360 0x8227E1D8 (W, :295) -- write-lock handle at this+0x1014.
        VaultRequestInterface* GetVaultRequestInterface();

        // X360 0x823BAD40 (R, :298) -- read-lock handle at this+0x1824.
        const ReplayRequestInterface* GetReplayRequestInterface() const;
        // X360 0x8227E280 (W, :300) -- write-lock handle at this+0x1824.
        ReplayRequestInterface* GetReplayRequestInterface();

    private:
        EffectsModuleResourceQueue mResourceRequestInterface;  // @ this+0x4    :303
        VaultRequestInterface      mVaultRequestInterface;     // @ this+0x1014 :304
        ReplayRequestInterface     mReplayRequestInterface;    // @ this+0x1824 :305
    };
}
}
