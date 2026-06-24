#pragma once

// ===========================================================================
// XAUDIO::CPacketList -- one of the intrusive packet lists embedded in
// XAUDIO::CPacketQueue in BURNOUT_X360_ARTIST.XEX. This header is the canonical
// OWNING home for:
//
//     XAUDIO::CPacketList::~CPacketList  @ 0x8296C328
//
// There is no reference source and no DWARF for this TU, so the SHAPE below
// is reconstructed purely from the X360 asm of the destructor. `XAUDIO` is an
// external middleware boundary, so its identifiers are preserved verbatim per
// the naming convention.
//
// LAYOUT (grounded in the destructor's loads):
//   The class begins with an XAUDIOPACKETCTX_ head (next @+0, prev @+4, base
//   @+8). The destructor drives the head through XAUDIOPACKETCTX_::GetNextEntry(
//   0, 1) and unlinks each visited entry, re-seating it self-circular -- i.e.
//   it drains the list. The destructor @0x8296C328 is byte-identical to
//   XAUDIO::CMasterVoiceList::~CMasterVoiceList @0x82C2E1C0 (same drain loop on
//   the same XAUDIOPACKETCTX_ head shape); it is called by CPacketQueue's
//   destructor.
// ===========================================================================

#include "types.hpp"
#include "SDKs/XAudio/XAudioPacketCtx.h" // XAUDIO::XAUDIOPACKETCTX_, XAudioListLink

namespace XAUDIO
{

class CPacketList
{
public:
    // @ 0x8296C328 -- drain the list: repeatedly fetch the head's forward
    // neighbour and unlink it (re-seating it self-circular) until empty.
    ~CPacketList();

    XAUDIOPACKETCTX_ mHead; // +0x00  list head (next/prev/base sentinel node)
};

} // namespace XAUDIO
