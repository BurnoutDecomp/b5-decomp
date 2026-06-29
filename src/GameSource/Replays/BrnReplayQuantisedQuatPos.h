#pragma once

// BrnReplays::QuantisedQuatPos -- packs/unpacks a quaternion-orientation + position
// pair into a compact 12-byte replay record. No DWARF or Feb-2007 source recovered;
// the surface below is taken from the X360 call sites in TrafficEntitySerialiser:
//
//   WriteAsQuatPos @0x82658B50 : builds an 8-float scratch (quat lanes + position +
//       a derived term) then `QuantisedQuatPos::Pack(out12, scratchFloats)` and writes
//       the 12-byte result with BaseSerialiser::Write(this, out12, 12).
//   ReadAsQuatPos  @0x82658D98 : reads 12 bytes with BaseSerialiser::Read(this, in12, 12),
//       then `QuantisedQuatPos::UnPack(out32, in12)` reconstructs the quat+pos and the
//       caller VMX-expands it into the destination transform. UnPack returns its out ptr.
//
// The exact float layout of the scratch / unpacked buffers is produced by VMX code in
// the caller and is not separately attested as named fields; Pack/UnPack are therefore
// declared with byte/float buffer parameters and their bodies live in the (not-yet
// reconstructed) QuantisedQuatPos TU.

#include "types.hpp"

namespace BrnReplays
{
    namespace QuantisedQuatPos
    {
        // @ Pack : quantise the source quat+pos floats into the 12-byte record lpDest.
        void Pack(void* lpDest12, const float* lpSource);

        // @ UnPack : expand the 12-byte record lpSource into the 32-byte (8-float)
        // working buffer lpDest; returns lpDest.
        float* UnPack(void* lpDest32, const void* lpSource12);
    }
}
