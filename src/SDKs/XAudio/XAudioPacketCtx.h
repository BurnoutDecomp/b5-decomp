#pragma once

// ===========================================================================
// XAUDIO::XAUDIOPACKETCTX<...> -- the intrusive linked-list iterator helper
// instantiated for the XAudio packet/voice context lists in
// BURNOUT_X360_ARTIST.XEX (CPacketList / CMasterVoiceList / CActiveVoiceList /
// CEngine all walk their lists through it). The X360 ledger truncates the
// template-instance name to "XAUDIOPACKETCTX>"; the instance modelled here is
// XAUDIO::XAUDIOPACKETCTX_ from the Hex-Rays symbol. This header is the
// canonical OWNING home for:
//
//     XAUDIO::XAUDIOPACKETCTX_::GetNextEntry  @ 0x829646E0
//
// There is no Feb-2007 leak source and no DWARF for this TU, so the SHAPE below
// is reconstructed from the X360 asm. `XAUDIO` is an external middleware
// boundary, so its identifiers are preserved verbatim per the naming
// convention.
//
// GetNextEntry @ 0x829646E0 (asm):
//   entry = (curOffset != 0) ? (this->mpBase + curOffset) : this;  // r11
//   nbr   = (bForward != 0)  ? entry->mpNext : entry->mpPrev;       // r11
//   if (this == nbr) return 0;            // walked back to the sentinel head
//   return nbr - this->mpBase;            // entry's byte offset within the pool
//
// So `this` (a1) is the list's sentinel head: its +0/+4 are the next/prev links
// (Link) and +8 (mpBase) is the base address the entries' byte offsets are
// measured from. `curOffset` (a2) is the current entry's byte offset (0 selects
// the head itself); `bForward` (a3) chooses the +0 (next) vs +4 (prev) link.
// The result is the neighbour entry's byte offset from mpBase, or 0 when the
// neighbour is the head (end of iteration).
// ===========================================================================

#include "types.hpp"

namespace XAUDIO
{

// The two link words shared by the head and every pooled entry node: +0 next,
// +4 prev. (lwz 0(r11) / lwz 4(r11) in the asm.)
struct XAudioListLink
{
    XAudioListLink* mpNext; // +0
    XAudioListLink* mpPrev; // +4
};

// The packet/voice context list head the iterator walks. The two link words
// come first (it is itself a node so `this == nbr` detects the sentinel), and
// +8 holds the pool base the entry offsets are relative to.
struct XAUDIOPACKETCTX_
{
    XAudioListLink* mpNext; // +0  forward link  (matches XAudioListLink::mpNext)
    XAudioListLink* mpPrev; // +4  backward link (matches XAudioListLink::mpPrev)
    u8*             mpBase; // +8  pool base the entry byte-offsets measure from

    // @ 0x829646E0 -- return the next/prev entry's byte offset within the pool,
    // or 0 when iteration has wrapped back to this head node.
    s32 GetNextEntry(u32 uCurOffset, int bForward);
};

} // namespace XAUDIO
