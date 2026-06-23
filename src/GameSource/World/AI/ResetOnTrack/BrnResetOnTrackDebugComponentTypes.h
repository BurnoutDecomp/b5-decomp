#pragma once

// BrnAI::ResetOnTrackDebugComponent::HngTestEntry -- one "hang test" record kept by the
// reset-on-track debug component's history ring buffer: the start/end world positions of
// a hang-detection ray cast plus whether it hit. DWARF home:
// GameSource/World/AI/ResetOnTrack/BrnResetOnTrackDebugComponent.h (the nested struct
// HngTestEntry, :137 in that header / :28 in the DecFIGS dump). The owning component
// embeds it as FixedRingBuffer<HngTestEntry,16> mHngTestRingBuffer.
//
// This header isolates just the HngTestEntry element type so the
// CgsContainers::RingBuffer<HngTestEntry>::Push instantiation TU (X360 @0x82769D60) can
// be homed without pulling in the full DebugComponent / ResetOnTrackRequest dependency
// tree. The full ResetOnTrackDebugComponent layout is owned by its own TU.
//
// LAYOUT (DecFIGS DWARF, BrnResetOnTrackDebugComponent.h:139-141): two 16-byte Vector3
// endpoints plus a bool, the whole element 16-byte aligned -> 48 bytes. Confirmed against
// the X360 asm of RingBuffer<HngTestEntry>::Push @0x82769D60, which copies the element as
// six 8-byte stores (48 bytes) and strides the backing array by 48 (`48 * miWritePos`).
// Access is by name.

#include "types.hpp"
#include "BrnCommonTypes.h"   // Vector3 (rw::math::vpu::Vector3, 16-byte SIMD)

namespace BrnAI
{
    struct ResetOnTrackDebugComponent
    {
        // DWARF BrnResetOnTrackDebugComponent.h:137. 48-byte element.
        struct HngTestEntry
        {
            Vector3 mStartPosition; // +0x00  :139 (16-byte SIMD lane)
            Vector3 mEndPosition;   // +0x10  :140 (16-byte SIMD lane)
            bool    mbHit;          // +0x20  :141 (tail padded to 48 by the 16-byte align)
        };
    };
}
