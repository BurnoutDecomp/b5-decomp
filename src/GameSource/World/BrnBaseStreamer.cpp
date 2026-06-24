#include "GameSource/World/BrnBaseStreamer.h"

// =============================================================================
// BrnWorld::InternalBaseStreamer  (GameSource/World/BrnBaseStreamer.h DWARF home)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. This is the real .cpp home for the
// two InternalBaseStreamer query members; both touch the current-entry list and
// the live count by NAME (the X360 absolute member offsets below are X360 32-bit
// facts, not load-bearing for the host build's member-by-name access).
//
// StreamerCurrentEntry on X360 is 24 bytes: mResourceId(CgsID, u64 @+0) +
// mbSafe(bool @+8) + meStatus(EEntryStatus @+0xC) + muUserId(u64 @+0x10). Both
// bodies index mpCurrentEntryList with that 24-byte stride.
// =============================================================================

namespace BrnWorld
{

// IsAssetLoaded @ 0x822A1300.
//   miStreamListLength @ +0x10 (lwz r9,0x10(r3)); early-out 0 when <= 0.
//   mpCurrentEntryList @ +0xC  (lwz r11,0xC(r3)).
//   Per entry (stride 0x18): match when mResourceId (ld @0, 8 bytes) == lId AND
//   muUserId (ld @0x10, 8 bytes) == luUserData AND meStatus (lwz @0xC) == 3
//   (E_STATUS_READY). Returns 1 on the first match, 0 if none / empty list.
bool InternalBaseStreamer::IsAssetLoaded( CgsID lId, u64 luUserData ) const
{
    for( s32 liIndex = 0; liIndex < miStreamListLength; liIndex++ )
    {
        if( ( mpCurrentEntryList[liIndex].mResourceId == lId ) &&
            ( mpCurrentEntryList[liIndex].muUserId    == luUserData ) &&
            ( mpCurrentEntryList[liIndex].meStatus    == StreamerCurrentEntry::E_STATUS_READY ) )
        {
            return true;
        }
    }
    return false;
}

// IsEntryReady @ 0x827B0258.
//   Asserts liIndex < miStreamListLength (BrnBaseStreamer.h:409, "liIndex <
//   miStreamListLength"); the assert collapses to the house CGS_ASSERT.
//   Returns mpCurrentEntryList[liIndex].meStatus == 3 (E_STATUS_READY): the X360
//   computes 24*liIndex (slwi r11,liIndex,1; add liIndex; slwi r11,3), adds
//   mpCurrentEntryList, loads meStatus @ +0xC, subtracts 3, then cntlzw/extrwi to
//   produce the bool (status == 3).
bool InternalBaseStreamer::IsEntryReady( s32 liIndex ) const
{
    CGS_ASSERT( liIndex < miStreamListLength, "liIndex < miStreamListLength" );
    return mpCurrentEntryList[liIndex].meStatus == StreamerCurrentEntry::E_STATUS_READY;
}

} // namespace BrnWorld
