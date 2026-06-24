#include "SDKs/XAudio/XAudioMasterVoiceList.h"

// ===========================================================================
// XAUDIO::CMasterVoiceList::~CMasterVoiceList @ 0x82C2E1C0 -- reconstructed from
// BURNOUT_X360_ARTIST.XEX. The PowerPC asm is authoritative for every store;
// see XAudioMasterVoiceList.h for the layout. No reference source and no
// DWARF exist for this TU.
// ===========================================================================

namespace XAUDIO
{

// @ 0x82C2E1C0 -- store-for-store:
//   loop: r3 = GetNextEntry(0, 1)              (bl ...; head forward neighbour)
//         beq cr6 -> done                       (offset 0 -> list empty)
//         r11 = *(this+8) + r3                  (lwz 8(r31); add) -> entry link
//         r10 = *(r11+0)                         (entry->mpNext)
//         cmplw r10, r11 ; beq -> skip unlink    (entry->mpNext == entry: empty)
//         r9 = *(r11+4)                          (entry->mpPrev)
//         *(r10+4) = r9                          (entry->mpNext->mpPrev = prev)
//         r10 = *(r11+4) ; r9 = *(r11+0)         (reload prev/next)
//         *(r10+0) = r9                          (entry->mpPrev->mpNext = next)
//         *(r11+4) = r11 ; *(r11+0) = r11        (entry made self-circular)
//         (loop)
XAUDIO::CMasterVoiceList::~CMasterVoiceList()
{
    for (s32 liOffset = mHead.GetNextEntry(0, 1); // lwz 8(r31) head; bl GetNextEntry(0,1)
         liOffset != 0;                            // cmplwi cr6,r3,0 ; beq -> done
         liOffset = mHead.GetNextEntry(0, 1))
    {
        // entry link = mpBase + offset (lwz 8(r31) ; add r11,r11,r3)
        XAudioListLink* lpEntry =
            reinterpret_cast<XAudioListLink*>(mHead.mpBase + liOffset);

        // Skip self-circular (already-empty) entries.
        if (lpEntry->mpNext != lpEntry) // lwz 0(r11); cmplw r10,r11; beq -> skip
        {
            // entry->mpNext->mpPrev = entry->mpPrev
            lpEntry->mpNext->mpPrev = lpEntry->mpPrev; // lwz 4(r11); stw r9,4(r10)
            // entry->mpPrev->mpNext = entry->mpNext  (operands reloaded in asm)
            lpEntry->mpPrev->mpNext = lpEntry->mpNext; // lwz 4(r11)/0(r11); stw r9,0(r10)
            // re-seat the unlinked entry self-circular
            lpEntry->mpPrev = lpEntry;                 // stw r11,4(r11)
            lpEntry->mpNext = lpEntry;                 // stw r11,0(r11)
        }
    }
}

} // namespace XAUDIO
