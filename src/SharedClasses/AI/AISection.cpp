#include "SharedClasses/AI/AISectionsResourceType.h"   // BrnAI::AISection (mx8Flags @+23)

// BrnAI::AISection member bodies.
//
// BrnAI::AISection::IsUnsuitableForResetOnTrackLink @0x8276AC18
//   (BrnAI::ResetOnTrackManager::UpdateResetOnTrackSectionUsingCurrentSection):
//   reads mx8Flags @+23 (lbz r11, 0x17(r3)) and returns true if bit 0x01 is set
//   (clrlwi r10,r11,31 -> &1) OR bit 0x40 is set (rlwinm r11,r11,0,25,25 -> &0x40),
//   otherwise false. The result is masked to a byte before return (clrlwi r3,r11,24).

namespace BrnAI
{
    bool AISection::IsUnsuitableForResetOnTrackLink() const
    {
        if ((mx8Flags & 0x01) != 0)
        {
            return true;
        }
        if ((mx8Flags & 0x40) != 0)
        {
            return true;
        }
        return false;
    }
}
