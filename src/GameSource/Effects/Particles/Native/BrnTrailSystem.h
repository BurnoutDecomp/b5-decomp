#pragma once

// ============================================================================
// GameSource/Effects/Particles/Native/BrnTrailSystem.h
//
// BrnParticle::Native::EmitterArray -- a fixed-capacity (knTrailEmitterPoolSize = 96)
// unordered array of active trail-emitter pointers. AddEntry appends; RemoveEntry
// swap-removes (last element into the freed slot) so the array stays dense.
//
// DWARF AUTHORITY (DecFIGS BrnTrailSystem.h dump):
//   struct EmitterArray {
//       TrailEmitter* mapEmitters[96];   // BrnTrailSystem.h:200 -- @ +0x000
//       int32_t       mnCurrentSize;     // BrnTrailSystem.h:201 -- @ +0x180
//   };
//   const int32_t knTrailEmitterPoolSize = 96 (BrnTrailSystem.h:63).
//
// The +0x180 offset of mnCurrentSize (== 96 * 4) attested by the asm (lwz/stw 0x180(this))
// pins the array as 96 four-byte pointer slots ahead of the size field. Only the two
// functions in this batch (AddEntry / RemoveEntry(int32_t)) are modelled; the rest of the
// trail system grows additively.
// ============================================================================

#include "types.hpp"

namespace BrnParticle
{
namespace Native
{
    // BrnTrailSystem.h:63 (DWARF) -- pool / array capacity.
    const s32 KN_TRAIL_EMITTER_POOL_SIZE = 96;

    class TrailEmitter; // forward decl -- payload not needed by these methods

    // BrnTrailSystem.h:120 (DWARF).
    struct EmitterArray
    {
        TrailEmitter* mapEmitters[KN_TRAIL_EMITTER_POOL_SIZE]; // @ +0x000
        s32           mnCurrentSize;                           // @ +0x180

        // BrnTrailSystem.h:132 -- append an emitter. X360 AddEntry @ 0x82277D50.
        void AddEntry( TrailEmitter* lpEmitter );

        // BrnTrailSystem.h:163 -- swap-remove the emitter at liIndex. X360
        // RemoveEntry(int32_t) @ 0x82277DC8.
        void RemoveEntry( s32 liIndex );
    };
}
}
