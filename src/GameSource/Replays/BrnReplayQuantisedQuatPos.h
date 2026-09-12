#pragma once

// BrnReplays::QuantisedQuatPos -- packs/unpacks a quaternion-orientation + position
// pair into a compact 12-byte replay record. No type information recovered;
// the surface below is taken from the call sites in TrafficEntitySerialiser:
//
//   WriteAsQuatPos : builds an 8-float scratch (quat lanes + position +
//       a derived term) then `QuantisedQuatPos::Pack(out12, scratchFloats)` and writes
//       the 12-byte result with BaseSerialiser::Write(this, out12, 12).
//   ReadAsQuatPos : reads 12 bytes with BaseSerialiser::Read(this, in12, 12),
//       then `QuantisedQuatPos::UnPack(out32, in12)` reconstructs the quat+pos and the
//       caller VMX-expands it into the destination transform. UnPack returns its out ptr.
//
// The exact float layout of the scratch / unpacked buffers is produced by VMX code in
// the caller and is not separately attested as named fields; Pack/UnPack are therefore
// declared with byte/float buffer parameters and their bodies live in the QuantisedQuatPos TU
// (BrnReplayQuantisedQuatPos.cpp).

#include "types.hpp"

namespace BrnReplays
{
    namespace QuantisedQuatPos
    {
        // Quantise the source quat+pos floats into the 12-byte record lpDest.
        void Pack(void* lpDest12, const float* lpSource);

        // Expand the 12-byte record lpSource into the 32-byte (8-float)
        // working buffer lpDest; returns lpDest.
        float* UnPack(void* lpDest32, const void* lpSource12);

        // Assert every field of the working buffer is in its packed
        // range (debug-only). lpContext is forwarded to the debug dump. TU-internal
        // (bl'd only within the QuantisedQuatPos TU); declared here for a coherent surface.
        void ValidateQuatPos(void* lpContext, const float* lpQuatPos);

        // Log the quat+pos working buffer, gated by the message filter.
        // liUnused is a dead pass-through argument in the shipped build. TU-internal.
        void PrintQuatPos(void* lpContext, const float* lpQuatPos, int liUnused = 0);

        // COMBINED stream+quantise entry points the SoundSerialiser drives, DISTINCT from
        // the buffer-only Pack/UnPack. Read pops a
        // 12-byte record off the serialiser stream and UnPacks it into the 8-float working set;
        // Write Packs the 8-float working set into a 12-byte record and writes it. lpSerialiser
        // is the driving BaseSerialiser-derived channel (opaque here to avoid a cyclic include).
        // Bodies live in their own TU. Do NOT alias onto Pack/UnPack.
        void Read(void* lpSerialiser, float* lpWorkingSet8);
        void Write(void* lpSerialiser, const float* lpWorkingSet8);
    }
}
