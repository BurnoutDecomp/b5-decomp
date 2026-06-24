#ifndef RW_RWHASH32STRING_H
#define RW_RWHASH32STRING_H

#include <cstdint>

#include "rw/rwcore_structs.h"  // rw::ResourceDescriptor (for the free operator-)

// ===========================================================================
// rw:: free-function vocabulary (hand-maintained; not part of the generated
// rwcore_structs.h type dump). Declares the two top-level rw:: free functions
// reconstructed from BURNOUT_X360_ARTIST.XEX:
//
//   rw::RwHash32String  @ 0x82BBC458
//   rw::operator-(rw::ResourceDescriptor)  @ 0x82B63E50
//
// Both are external-linkage symbols called from multiple other TUs, so they are
// declared here and defined once in renderware/src/rw/RwHash32String.cpp.
// ===========================================================================

namespace rw
{

// FNV-1a 32-bit string hash over a NUL-terminated byte string. The X360 body
// @ 0x82BBC458 seeds with the caller-supplied accumulator and, for each byte,
// folds: hash = (hash * 0x01000193) ^ byte. The 0x01000193 multiplier is the
// 32-bit FNV prime (mullw r4,r4,0x1000193 then xor with the byte). Returns the
// final accumulator. Called by the BrnSound resource registrar and friends.
uint32_t RwHash32String(const char* lpcString, uint32_t luSeed);

// Per-pool resource-descriptor subtraction: lrLhs - lrRhs. The X360 body
// @ 0x82B63E50 first copies lrLhs into the result (memcpy of 40 bytes -- the
// X360 game build's serialised descriptor is a 5-entry BaseResourceDescriptors,
// 5 * sizeof(BaseResourceDescriptor)=40, see the cross-build drift note in
// rwcore_structs.h), then walks the entries at the 8-byte BaseResourceDescriptor
// stride subtracting only the first dword (m_size) of each:
//     result[i].m_size = lrLhs[i].m_size - lrRhs[i].m_size
// (m_alignment is left as copied from lrLhs). Used by
// rw::LinearResourceAllocator::GetAvailable to compute capacity - usage.
//
// The committed PC rw::ResourceDescriptor is BaseResourceDescriptors<4> (32B),
// so this reconstruction subtracts the four committed entries' m_size by name;
// the X360 fifth-entry store is documented in the .cpp and FLAGGED (widening
// the committed type to <5> is non-additive and NOT applied here).
ResourceDescriptor operator-(const ResourceDescriptor& lrLhs, const ResourceDescriptor& lrRhs);

}  // namespace rw

#endif  // RW_RWHASH32STRING_H
