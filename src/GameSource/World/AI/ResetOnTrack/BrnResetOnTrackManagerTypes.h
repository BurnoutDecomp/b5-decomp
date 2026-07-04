#pragma once

// BrnAI::ResetOnTrackManager::RecentResetEntry -- one "recent reset" record kept by the
// reset-on-track manager's history ring buffer: the world position of a recent reset plus
// the time it happened. DWARF home:
// GameSource/World/AI/ResetOnTrack/BrnResetOnTrackManager.h (the nested struct
// RecentResetEntry, :78-85 in the DecFIGS dump). The owning manager embeds it as
// FixedRingBuffer<RecentResetEntry,8> mRecentResets (:330).
//
// This header isolates just the RecentResetEntry element type so the
// CgsContainers::RingBuffer<RecentResetEntry>::Push instantiation TU (X360 @0x8276A090)
// can be homed without pulling in the full ResetOnTrackManager dependency tree. The full
// ResetOnTrackManager layout is owned by its own TU. Mirrors the sibling
// BrnResetOnTrackDebugComponentTypes.h.
//
// LAYOUT (DecFIGS DWARF, BrnResetOnTrackManager.h:78-85): a 16-byte Vector3 position plus
// a float, the whole element 16-byte aligned -> 32 bytes. Confirmed against the X360 asm of
// RingBuffer<RecentResetEntry>::Push @0x8276A090, which copies the element as four 8-byte
// stores (32 bytes) and strides the backing array by 32 (`slwi r11,r11,5`). Access is by name.

#include "types.hpp"
#include "BrnCommonTypes.h"   // Vector3 (rw::math::vpu::Vector3, 16-byte SIMD)

namespace BrnAI
{
    struct ResetOnTrackManager
    {
        // DWARF BrnResetOnTrackManager.h:78. 32-byte element.
        struct alignas(16) RecentResetEntry
        {
            Vector3 mPosition; // +0x00  :94 (16-byte SIMD lane)
            f32     mfTime;    // +0x10  :95 (tail padded to 32 by the 16-byte align)
        };
    };
}
