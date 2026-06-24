#include "SDKs/XAudio/XAudioPacketCtx.h"

// ===========================================================================
// XAUDIO::XAUDIOPACKETCTX_::GetNextEntry @ 0x829646E0 -- reconstructed from
// BURNOUT_X360_ARTIST.XEX. See XAudioPacketCtx.h for the layout and asm notes.
// ===========================================================================

namespace XAUDIO
{

s32 XAUDIOPACKETCTX_::GetNextEntry(u32 uCurOffset, int bForward)
{
    // entry = (uCurOffset != 0) ? (mpBase + uCurOffset) : (this as link)
    XAudioListLink* pEntry;
    if (uCurOffset != 0)            // cmplwi cr6,r4,0 ; beq
        pEntry = reinterpret_cast<XAudioListLink*>(mpBase + uCurOffset); // lwz 8(r3); add
    else
        pEntry = reinterpret_cast<XAudioListLink*>(this);               // mr r11,r3

    // nbr = bForward ? entry->mpNext (+0) : entry->mpPrev (+4)
    XAudioListLink* pNbr;
    if (bForward != 0)             // cmpwi cr6,r5,0 ; beq
        pNbr = pEntry->mpNext;     // lwz 0(r11)
    else
        pNbr = pEntry->mpPrev;     // lwz 4(r11)

    // if (this == nbr) end of iteration -> 0
    if (reinterpret_cast<XAudioListLink*>(this) == pNbr) // cmplw cr6,r3,r11 ; beq
        return 0;                                        // li r3,0

    // return nbr - mpBase  (entry's byte offset within the pool)
    return static_cast<s32>(reinterpret_cast<u8*>(pNbr) - mpBase); // lwz 8(r3); subf
}

} // namespace XAUDIO
