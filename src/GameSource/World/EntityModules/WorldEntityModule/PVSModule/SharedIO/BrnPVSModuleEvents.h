#ifndef BRN_PVS_MODULE_EVENTS_H
#define BRN_PVS_MODULE_EVENTS_H

#include "types.hpp"
#include "BrnCommonTypes.h"   // Vector4 (== rw::math::vpu::Vector4)

// ============================================================================
// GameSource/World/EntityModules/WorldEntityModule/PVSModule/SharedIO/BrnPVSModuleEvents.h
//
// The PVS (Potentially Visible Set) module's shared-IO request/response events.
// This slice covers BrnWorld::PVSIO::GetZoneRequest, the "which zone is the
// listener in" request the PVS module's Update processes.
//
// LAYOUT (from GetVelocity @ 0x822A3A48): the request carries a use-velocity flag
// (byte @ +0x24) and a velocity vector (Vector4 @ +0x30). The leading 0x24 bytes
// (the event header / position fields) are opaque in this slice.
//
// FLAG: only the two fields GetVelocity reads are named (mbUseVelocity @+0x24,
// mVelocity @+0x30); the preceding 0x24-byte header and the 11 bytes between the
// flag and the vector are modelled as opaque byte spans at the asm-observed
// offsets. Member names are inferred from the getter's behaviour and its assert
// text ("lpbOutUseVelocity is NULL"); there is no DWARF for this type. Grown
// additively when a later PVS-module TU attests the header fields.
// ============================================================================

namespace BrnWorld
{
namespace PVSIO
{
struct GetZoneRequest
{
    // GetVelocity @ 0x822A3A48. Reads back the request's velocity hint: copies the
    // velocity vector into *lpOutVelocity and the use-velocity flag into
    // *lpbOutUseVelocity. const getter.
    //
    // X360-faithful: asserts lpbOutUseVelocity != NULL (baked
    // BrnPVSModuleEvents.h:208, msg "lpbOutUseVelocity is NULL"; the baked
    // file/line is not reproduced -- CGS_ASSERT injects __FILE__/__LINE__), then
    // copies mVelocity (one lvx128/stvx128 16-byte vector move, a plain copy --
    // NOT a VMX compute pipeline) and the mbUseVelocity byte.
    void GetVelocity(Vector4* lpOutVelocity, bool* lpbOutUseVelocity) const;

    // --- layout (asm-observed offsets) ---------------------------------------
    u8      maHeader[0x24];   // +0x00  opaque event header (deferred)
    u8      mbUseVelocity;    // +0x24  use-velocity flag (lbz 0x24)
    u8      maPad25[0x0B];    // +0x25  opaque padding up to the vector
    Vector4 mVelocity;        // +0x30  velocity hint (lvx128 0x30)
};

// ============================================================================
// BrnWorld::PVSIO::GetZoneResponse -- the PVS module's per-zone visibility reply.
//
// SIZE (X360, authoritative): sizeof == 736 (0x2E0). Pinned two independent ways:
//   * BaseEventQueue<GetZoneResponse>::AddEvent @ 0x822C9E28 block-copies the
//     event with `memcpy(736 * miLength + mpEvents, src, 736)` -- element stride
//     736.
//   * OutputBuffer::Construct @ 0x822EE658 places the next member (a
//     VariableEventQueue<512,16>) at +0x1718, immediately after the
//     EventQueue<GetZoneResponse,8> sub-object: base 16 + 8*736 = 0x1710,
//     rounded up to the 16-aligned 0x1718.
// 736 == 46*16, and EventQueue<...,8>::Construct @ 0x822E51B0 lays the inline
// maEvents[8] at the 16-aligned +0x10 -- so GetZoneResponse is 16-aligned (it
// carries a 16-aligned vector/matrix payload).
//
// FLAG (opaque payload): no getter for GetZoneResponse was recovered in this
// slice and there is no DWARF for the type, so its 736 internal bytes are NOT
// individually named. They are modelled as one correctly-sized, 16-aligned
// opaque byte span so the queue element stride (736) and the owning buffer's
// member offsets are byte-exact. Grow this ADDITIVELY into named fields when a
// later PVS-module TU (a GetZoneResponse accessor) attests them. Nothing here is
// fabricated as X360 fact -- only the size/alignment are pinned by asm.
struct alignas(16) GetZoneResponse
{
    // +0x000 .. +0x2DF  opaque response payload (deferred; see FLAG above).
    u8 maPayload[736];
};
}
}

#endif // BRN_PVS_MODULE_EVENTS_H
