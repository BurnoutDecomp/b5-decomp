#pragma once

// ===========================================================================
// D3D::CBlocker -- the GPU "block" scope object inside BURNOUT_X360_ARTIST.XEX.
// This header is the canonical OWNING home for the CBlocker TYPE and its three
// methods (bodied in D3DBlocker.cpp):
//
//     D3D::CBlocker::CBlocker  @ 0x82947498   (ctor)
//     D3D::CBlocker::~CBlocker @ 0x82947518   (dtor)
//     D3D::CBlocker::Check     @ 0x82947608
//
// A CBlocker is a short-lived RAII object that the CDevice block-primitives
// (BlockOnFence / BlockOnPrimaryRange / BlockOnSecondaryPosition, D3DDevice_Swap,
// the XPS work path) construct on the stack before they spin-wait on the GPU.
// It records, at construction, the device's current readback progress, the live
// GPU fence and the CPU time-base; while the caller polls it calls Check(),
// which watches whether the GPU is still making progress and, once the fence has
// advanced past a stall threshold with no progress, runs the deadlock diagnostic.
// On destruction it accumulates the elapsed blocked time into the device's
// per-type counters and fires an optional profiling callback.
//
// `D3D` is the X360 graphics-SDK boundary, so its identifiers (CBlocker, Check)
// are preserved verbatim per the naming convention.
//
// LAYOUT -- there is NO DWARF and NO reference source for this TU. The six-word
// object shape below is reconstructed purely from the store offsets the ctor and
// Check emit in the X360 asm. The device fields CBlocker pokes are named members
// of D3D::CDevice (see CDevice.h) reached through mpDevice -- CBlocker is a
// `friend` of CDevice; nothing is offset-hacked.
// ===========================================================================

#include "types.hpp"

namespace D3D
{

class CDevice; // CBlocker only holds a CDevice* -- forward declaration suffices.

class CBlocker
{
public:
    // @ 0x82947498 -- snapshot device readback progress, the live GPU fence and
    // the CPU time-base; emit a PIX begin-op for a non-zero block type.
    CBlocker(CDevice* pDevice, s32 iType);

    // @ 0x82947518 -- accumulate the elapsed blocked ticks into the device's
    // per-type counter, emit a PIX end-op and fire the profiling callback.
    ~CBlocker();

    // @ 0x82947608 -- poll the block: true = keep waiting, false = give up
    // (deadlock flagged, or the deadlock diagnostic confirmed a hang).
    bool Check();

private:
    CDevice* mpDevice;        // +0x00  owning device (word[0])
    s32      miType;          // +0x04  block type / PIX op selector (word[1])
    u32      muLastProgress;  // +0x08  last observed GPU readback progress word
    u32      muProgressFence; // +0x0C  GPU fence value when progress last advanced
    u32      muStartFence;    // +0x10  GPU fence at construction
    u32      muStartTimeBase; // +0x14  CPU time-base low word at construction

    friend void _CBlockerAssertLayout();
};

} // namespace D3D
