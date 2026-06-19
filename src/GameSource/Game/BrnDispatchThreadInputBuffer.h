#pragma once

#include "types.hpp"

#include "GameShared/GameClasses/Module/CgsIOBuffer.h"   // CgsModule::IOBuffer base + IsBufferLockedForReading()

// BrnGame::DispatchThreadInputBuffer -- the per-frame input payload the particle/effects dispatch
// thread reads. It derives the shared CgsModule::IOBuffer (status-flag-guarded read/write locking)
// and carries the particle-update data the simulation produced, plus brightness/contrast/renderer
// flags and (deferred) the render data, crash-triangle cache, inter-thread event queue, etc.
//
// Layout + member order recovered from the DecFIGS DWARF
// (references/DecFIGS/dwarfdump/GameSource/Game/BrnDispatchThreadInputBuffer.h): struct
// BrnGame::DispatchThreadInputBuffer : public IOBuffer, with the first four members being
// mxRendererFlags (h:191), miBrightness (h:192), miContrast (h:193), mParticleData (h:194).
//
// FLAG (MINIMAL SLICE): only the members up to and including mParticleData are declared -- that is
// all GetParticleData() (this TU) needs. The many trailing members (mParticleRenderData,
// mBufferCrashTriangleCache, the InterThreadEventQueue<16384> mParticleInterThreadEventQueue,
// meLoadingScreenCommand, mhCalibrationTextureHandle, mSnapShotRequest, the bool flags, and the
// occlusion Matrix44) and ALL the other accessors/lifecycle methods are DEFERRED to their own TUs
// and are intentionally omitted here. This is semantic parity (real names/types/order for the
// declared prefix), not a byte-exact full object.
namespace BrnParticle
{
namespace ParticleModule
{
    // FLAG (ad-hoc opaque): BrnParticle::ParticleModule::DispatchThreadUpdateData is NOT committed
    // anywhere yet. It is modelled here as a minimal opaque struct ONLY so it can be a complete
    // member type by value (so mParticleData has a real address and &mParticleData yields the
    // correct pointer type for GetParticleData's return). The real size/layout is UNKNOWN -- the
    // 1-byte placeholder storage is a stand-in, NOT the true size. When DispatchThreadUpdateData is
    // reconstructed in its own home this definition is replaced (and this file includes it instead).
    struct DispatchThreadUpdateData
    {
        u8 maStorage[1];   // FLAG: opaque, true size unknown
    };
}
}

namespace BrnGame
{
    // DWARF h:68 -- struct BrnGame::DispatchThreadInputBuffer : public IOBuffer (IOBuffer here is
    // CgsModule::IOBuffer, the committed shared base).
    struct DispatchThreadInputBuffer : public CgsModule::IOBuffer
    {
        // GetParticleData @ 0x8227F4F0 / DWARF h:97 -- assert the buffer is read-locked, then hand
        // back the embedded particle-update data. Const overload only in this slice; the non-const
        // overload (DWARF h:98) is deferred.
        const BrnParticle::ParticleModule::DispatchThreadUpdateData* GetParticleData() const;

    private:
        // Members in DWARF declaration order. With the committed CgsModule::IOBuffer base (a single
        // 1-byte FlagSet8 status field, padded to 4), mxRendererFlags lands at +4, miBrightness at
        // +8, miContrast at +12, and mParticleData at +16 -- matching the asm's `return a1 + 16`
        // (= &mParticleData). FLAG: semantic parity; the +16 offset is implied by the base size,
        // not asserted byte-exactly here.
        uint32_t mxRendererFlags;                                       // DWARF h:191
        int32_t  miBrightness;                                          // DWARF h:192
        int32_t  miContrast;                                            // DWARF h:193
        BrnParticle::ParticleModule::DispatchThreadUpdateData mParticleData;  // DWARF h:194
    };
}
